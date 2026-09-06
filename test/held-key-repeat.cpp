// SPDX-License-Identifier: GPL-3.0-or-later
#include "lotus-engine.h"
#include "lotus-utils.h"
#include "test-input-context.h"

#include <algorithm>
#include <cerrno>
#include <cstddef>
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
    // "paced" is the control: same 12 presses, but none of them arrive while a
    // replacement is still in flight, so nothing lands in buffered_keys_.
    const bool paced = argc > 1 && std::string(argv[1]) == "paced";
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

    // Reproduces #472: a held 'd' under Telex. Autorepeat keeps delivering keys
    // while a replacement is still waiting on the uinput server, so every extra
    // key lands in buffered_keys_ and comes back through replayBufferedKeys().
    const int kHeldPresses = 12;
    int       sent         = 0;

    // Prime the replacement cycle: d, d -> the engine asks for a deletion.
    while (sent < kHeldPresses) {
        fcitx::KeyEvent event(context.get(), fcitx::Key(FcitxKey_d), false);
        engine.keyEvent(entry, event);
        ++sent;
        if (event.accepted())
            break;
    }

    std::vector<std::string> screen;
    size_t                   lastCommit = 0;
    int cycles = 0;
    while (cycles < 8) {
        int backspaces = 0;
        if (!listener.receive(backspaces, "no further replacement request"))
            break;
        ++cycles;
        std::cerr << "cycle " << cycles << ": server asked for " << backspaces << " backspaces\n";

        // Model what the application window actually shows: the replacement
        // deletes `backspaces` characters before the new commit lands.
        for (int i = 0; i < backspaces && !screen.empty(); ++i)
            screen.pop_back();
        if (context->commits().size() > lastCommit) {
            for (auto& character : splitUtf8(context->commits().back()))
                screen.push_back(character);
            lastCommit = context->commits().size();
        }

        // Autorepeat does not stop while the replacement is in flight.
        if (!paced) {
            for (int i = 0; i < 2 && sent < kHeldPresses; ++i, ++sent) {
                fcitx::KeyEvent extra(context.get(), fcitx::Key(FcitxKey_d), false);
                engine.keyEvent(entry, extra);
            }
        }

        for (int i = 0; i < backspaces; ++i) {
            fcitx::KeyEvent back(context.get(), fcitx::Key(FcitxKey_BackSpace), false);
            engine.keyEvent(entry, back);
        }

        if (paced) {
            while (sent < kHeldPresses) {
                fcitx::KeyEvent next(context.get(), fcitx::Key(FcitxKey_d), false);
                engine.keyEvent(entry, next);
                ++sent;
                if (next.accepted())
                    break;
            }
        }
    }

    std::string text;
    std::cerr << (paced ? "[paced control] " : "[held key] ") << "keys sent: " << sent << ", replacement cycles: " << cycles << "\ncommits: ";
    for (const auto& commit : context->commits())
        std::cerr << "['" << commit << "']";
    for (const auto& piece : screen)
        text += piece;
    std::cerr << "\nwindow shows: '" << text << "'\n";

    // Outside Smooth mode a held key keeps appending, so the window grows with
    // the number of presses. #472 reports that it stops instead.
    if (screen.size() < 3) {
        reportFailure("held key accumulates", "at least 3 characters after " + std::to_string(sent) + " presses",
                      "window shows '" + text + "'",
                      "a held key never grows past the Telex cycle: replayBufferedKeys() resets the engine "
                      "after every replacement, so the leftover autorepeat keys restart from an empty buffer");
        return 1;
    }
    return 0;
}
