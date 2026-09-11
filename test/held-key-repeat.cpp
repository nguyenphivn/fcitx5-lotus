// SPDX-License-Identifier: GPL-3.0-or-later
#include "lotus-engine.h"
#include "lotus-utils.h"
#include "test-input-context.h"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace {

    void reportFailure(const std::string& step, const std::string& expected, const std::string& actual, const std::string& meaning) {
        std::cerr << "Step: " << step << '\n';
        std::cerr << "Expected: " << expected << '\n';
        std::cerr << "Actual: " << actual << '\n';
        std::cerr << "Meaning: " << meaning << '\n';
    }

    class BackspaceListener {
      public:
        BackspaceListener() {
            fd_ = socket(AF_UNIX, SOCK_SEQPACKET, 0);
            if (fd_ < 0) {
                fail("socket");
                return;
            }
            sockaddr_un address{};
            address.sun_family    = AF_UNIX;
            const auto socketPath = buildSocketPath("kb_socket");
            address.sun_path[0]   = '\0';
            std::memcpy(&address.sun_path[1], socketPath.data(), socketPath.size());
            const auto length = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + socketPath.size() + 1);
            if (bind(fd_, reinterpret_cast<const sockaddr*>(&address), length) < 0 || listen(fd_, 1) < 0) {
                fail("bind/listen");
            }
        }

        ~BackspaceListener() {
            if (client_ >= 0)
                close(client_);
            if (fd_ >= 0)
                close(fd_);
        }

        bool receive(int& count, const char* meaning, const char* requestTimeoutExpected = "request within 2000 ms") {
            if (client_ < 0) {
                if (fd_ < 0) {
                    reportFailure("wait for replacement socket connection", "valid listener descriptor", "listener descriptor is invalid", meaning);
                    return false;
                }
                pollfd     pollfd{fd_, POLLIN, 0};
                const auto pollResult = poll(&pollfd, 1, 2000);
                if (pollResult == 0) {
                    reportFailure("wait for replacement socket connection", "connection request within 2000 ms", "poll timed out", meaning);
                    return false;
                }
                if (pollResult < 0) {
                    reportFailure("wait for replacement socket connection", "poll succeeds", "poll failed: " + std::string(std::strerror(errno)), meaning);
                    return false;
                }
                if (!(pollfd.revents & POLLIN)) {
                    reportFailure("wait for replacement socket connection", "POLLIN revents", "revents=" + std::to_string(pollfd.revents), meaning);
                    return false;
                }
                client_ = accept(fd_, nullptr, nullptr);
                if (client_ < 0) {
                    reportFailure("accept replacement socket connection", "accept succeeds", "accept failed: " + std::string(std::strerror(errno)), meaning);
                    return false;
                }
            }
            pollfd     pollfd{client_, POLLIN, 0};
            const auto pollResult = poll(&pollfd, 1, 2000);
            if (pollResult == 0) {
                reportFailure("wait for replacement request", requestTimeoutExpected, "poll timed out", meaning);
                return false;
            }
            if (pollResult < 0) {
                reportFailure("wait for replacement request", "poll succeeds", "poll failed: " + std::string(std::strerror(errno)), meaning);
                return false;
            }
            if (!(pollfd.revents & POLLIN)) {
                reportFailure("wait for replacement request", "POLLIN revents", "revents=" + std::to_string(pollfd.revents), meaning);
                return false;
            }
            const auto received = recv(client_, &count, sizeof(count), 0);
            if (received < 0) {
                reportFailure("receive replacement request", std::to_string(sizeof(count)) + " bytes", "recv failed: " + std::string(std::strerror(errno)), meaning);
                return false;
            }
            if (received != sizeof(count)) {
                reportFailure("receive replacement request", std::to_string(sizeof(count)) + " bytes", "recv returned " + std::to_string(received) + " bytes", meaning);
                return false;
            }
            return true;
        }

        bool valid() const {
            return fd_ >= 0;
        }

      private:
        void fail(const char* operation) {
            reportFailure(std::string(operation) + " replacement socket", "operation succeeds", std::string(operation) + " failed: " + std::strerror(errno),
                          "the test cannot observe Smooth replacement requests");
            close(fd_);
            fd_ = -1;
        }

        int fd_     = -1;
        int client_ = -1;
    };

    // The window counts characters, not commits: "dd" is two of them.
    std::vector<std::string> splitUtf8(const std::string& text) {
        std::vector<std::string> characters;
        for (size_t i = 0; i < text.size();) {
            const auto lead   = static_cast<unsigned char>(text[i]);
            size_t     length = 1;
            if ((lead & 0xF8U) == 0xF0U)
                length = 4;
            else if ((lead & 0xF0U) == 0xE0U)
                length = 3;
            else if ((lead & 0xE0U) == 0xC0U)
                length = 2;
            length = std::min(length, text.size() - i);
            characters.push_back(text.substr(i, length));
            i += length;
        }
        return characters;
    }

} // namespace

int main(int argc, char** argv) {
    // "paced" is the control: same presses, but none of them arrive while a
    // replacement is still in flight, so nothing lands in buffered_keys_.
    const bool paced = argc > 1 && std::string(argv[1]) == "paced";

    // Own a private socket name so a running fcitx5-lotus-server does not
    // already hold the one the listener binds.
    const std::string socketNamespace = "test-" + std::to_string(getpid());
    setenv("LOTUS_SOCKET_NAMESPACE", socketNamespace.c_str(), 1);

    configureTestPaths("fcitx5-lotus-held-key-repeat");
    TestInstance       testInstance;
    fcitx::LotusEngine engine(&testInstance.instance);
    fcitx::RawConfig   config;
    config.setValueByPath("Mode", "Uinput (Smooth)");
    config.setValueByPath("InputMethod", "Telex");
    engine.setConfig(config);

    BackspaceListener listener;
    if (!listener.valid())
        return 1;
    auto context = std::make_unique<TestInputContext>(&testInstance.instance);
    context->focusIn();
    fcitx::InputMethodEntry  entry("lotus", "Lotus", "vi", "lotus");
    fcitx::InputContextEvent focus(context.get(), fcitx::EventType::InputContextFocusIn);
    engine.activate(entry, focus);
    context->resetPreeditUpdateCount();

    // Reproduces #472: a held 'd' under Telex.
    //
    // The window model matters. In Smooth mode the uinput server passes the
    // physical keystroke straight to the application, so every press shows up
    // there on its own; Lotus only corrects afterwards with backspaces and a
    // commit. Counting commits alone would understate what the user sees.
    const int                kHeldPresses = 12;
    int                      sent         = 0;
    std::vector<std::string> screen;
    size_t                   lastCommit = 0;

    auto press = [&]() {
        screen.emplace_back("d"); // the application sees the raw key first
        fcitx::KeyEvent event(context.get(), fcitx::Key(FcitxKey_d), false);
        engine.keyEvent(entry, event);
        ++sent;
        return event.accepted();
    };

    auto pullCommit = [&]() {
        if (context->commits().size() > lastCommit) {
            for (auto& character : splitUtf8(context->commits().back()))
                screen.push_back(character);
            lastCommit = context->commits().size();
        }
    };

    while (sent < kHeldPresses && !press()) {
    }

    int cycles = 0;
    while (cycles < 8) {
        int backspaces = 0;
        if (!listener.receive(backspaces, "no further replacement request"))
            break;
        ++cycles;
        for (int i = 0; i < backspaces && !screen.empty(); ++i)
            screen.pop_back();

        if (!paced) {
            for (int i = 0; i < 2 && sent < kHeldPresses; ++i)
                press();
        }

        for (int i = 0; i < backspaces; ++i) {
            fcitx::KeyEvent back(context.get(), fcitx::Key(FcitxKey_BackSpace), false);
            engine.keyEvent(entry, back);
        }
        pullCommit();

        if (paced) {
            while (sent < kHeldPresses && !press()) {
            }
        }
    }
    pullCommit();

    std::string text;
    for (const auto& piece : screen)
        text += piece;
    std::cerr << (paced ? "[paced control] " : "[held key] ") << "keys sent: " << sent << ", replacement cycles: " << cycles << "\ncommits: ";
    for (const auto& commit : context->commits())
        std::cerr << "['" << commit << "']";
    std::cerr << "\nforwarded: " << context->forwarded().size() << "\nwindow shows: '" << text << "' (" << screen.size() << " characters)\n";

    // Outside Smooth mode a held key keeps appending, so the window grows with
    // the number of presses. #472 reports that it stops instead.
    if (screen.size() < 3) {
        reportFailure("held key accumulates", "at least 3 characters after " + std::to_string(sent) + " presses",
                      "window shows '" + text + "'",
                      "a held key never grows past the Telex cycle: handleUinputMode() resets the Bamboo engine "
                      "after every commit, so each repeat restarts from an empty word buffer");
        return 1;
    }
    return 0;
}
