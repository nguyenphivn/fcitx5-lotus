/*
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành <thanhpy2009@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

/**
 * @file lotus-utils.h
 * @brief Utility functions and global state for fcitx5-lotus.
 */

#ifndef _FCITX5_LOTUS_UTILS_H_
#define _FCITX5_LOTUS_UTILS_H_

#include <atomic>
#include <sys/un.h>
#include <fcitx-utils/log.h>
#include <fcitx/inputcontext.h>

#include "lotus-config.h"

/**
 * @brief Maximum length of Unix socket paths.
*/
#define UNIX_PATH_MAX sizeof(((struct sockaddr_un*)0)->sun_path)

FCITX_DECLARE_LOG_CATEGORY(lotus);

#define LOTUS_DEBUG(msg) FCITX_LOGC(lotus, Debug) << "[DEBUG] " << msg
#define LOTUS_INFO(msg)  FCITX_LOGC(lotus, Info) << "[INFO] " << msg
#define LOTUS_WARN(msg)  FCITX_LOGC(lotus, Warn) << "[WARN] " << msg
#define LOTUS_ERROR(msg) FCITX_LOGC(lotus, Error) << "[ERROR] " << msg

// Forward declaration for fcitx types
using KeySym = uint32_t;

// Toàn cục THẬT: mỗi biến dưới đây hoặc do luồng theo dõi chuột chạm tới
// (lotus-monitor.cpp), hoặc là một tài nguyên duy nhất của cả tiến trình. Luồng chuột
// không biết cửa sổ nào cả, nên không đưa xuống LotusState được.
//
// realMode, is_deleting_ và realtextLen TỪNG nằm ở đây; chúng là trạng thái riêng của
// từng cửa sổ nên đã chuyển vào LotusState. Xem THIETKE-bien-toan-cuc.md ở kho ghi chú.
extern std::atomic<bool>         needEngineReset;   ///< Luồng chuột báo: cần dựng lại engine
extern std::atomic<bool>         g_mouse_clicked;   ///< Luồng chuột báo: vừa có cú bấm
extern std::atomic<bool>         stop_flag_monitor; ///< Công tắc tắt luồng theo dõi
extern std::atomic<int>          uinput_client_fd; ///< Một kết nối duy nhất tới uinput server
extern std::atomic<int>          mouse_socket_fd;   ///< Socket duy nhất của luồng chuột

/**
 * @brief Builds socket path from base suffix.
 * @param base_path_suffix Suffix to append to base path.
 * @return Full socket path.
 */
std::string buildSocketPath(const char* base_path_suffix);

/**
 * @brief Gets current time in milliseconds.
 * @return Timestamp in milliseconds.
 */
int64_t now_ms();

/**
 * @brief Checks if key symbol is a backspace.
 * @param sym Key symbol to check.
 * @return True if backspace.
 */
bool isBackspace(uint32_t sym);

/**
 * @brief Erases the last UTF-8 codepoint from a string in place.
 *
 * Walks back past any continuation bytes (10xxxxxx) to find the leading
 * byte, then erases from that position. Correctly handles 2/3/4-byte
 * sequences; no-op on empty input. Used by the emoji-mode backspace
 * handler so the preedit stays valid UTF-8 after each backspace.
 */
void eraseLastUtf8Codepoint(std::string& buffer);

/**
 * @brief Compares two strings and computes diff.
 * @param A First string.
 * @param B Second string.
 * @param deletedPart Output deleted portion.
 * @param addedPart Output added portion.
 * @return Comparison result code.
 */
int compareAndSplitStrings(const std::string& A, const std::string& B, std::string& deletedPart, std::string& addedPart);

/**
 * @brief Checks if string starts with prefix.
 * @param str String to check.
 * @param prefix Prefix to check.
 * @return True if string starts with prefix.
 */
bool isStartsWith(const std::string& str, const std::string& prefix);

/**
 * @brief Get the frontend name from the input context.
 * @param ic Input context.
 * @return Frontend name.
 */
std::string getFrontendName(fcitx::InputContext* ic);

/**
 * @brief Key event entry for replay buffer.
 */
struct KeyEntry {
    uint32_t sym;   ///< Key symbol
    uint32_t state; ///< Key state (modifiers)
};

#endif // _FCITX5_LOTUS_UTILS_H_
