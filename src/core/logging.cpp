#include "core/logging.h"

#include <windows.h>

#include <string>

namespace taskbar_lyrics {
namespace {

std::wstring EnvironmentValue(const wchar_t* name) {
    const DWORD required = GetEnvironmentVariableW(name, nullptr, 0);
    if (required == 0) {
        return {};
    }

    std::wstring value(required, L'\0');
    const DWORD written = GetEnvironmentVariableW(name, value.data(), required);
    if (written == 0 || written >= required) {
        return {};
    }
    value.resize(written);
    return value;
}

bool EnsureDirectory(const std::wstring& path) {
    if (CreateDirectoryW(path.c_str(), nullptr)) {
        return true;
    }
    return GetLastError() == ERROR_ALREADY_EXISTS;
}

std::wstring LogDirectory() {
    std::wstring override_path = EnvironmentValue(L"TASKBARLYRICS_LOG_DIR");
    if (!override_path.empty()) {
        EnsureDirectory(override_path);
        return override_path;
    }

    std::wstring local_app_data = EnvironmentValue(L"LOCALAPPDATA");
    if (local_app_data.empty()) {
        return {};
    }

    std::wstring app_directory = local_app_data + L"\\TaskbarLyrics";
    if (!EnsureDirectory(app_directory)) {
        return {};
    }

    std::wstring log_directory = app_directory + L"\\logs";
    if (!EnsureDirectory(log_directory)) {
        return {};
    }
    return log_directory;
}

std::string Utf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }

    const int required = WideCharToMultiByte(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return {};
    }

    std::string result(static_cast<size_t>(required), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        result.data(),
        required,
        nullptr,
        nullptr);
    return result;
}

}  // namespace

void LogInfo(const std::wstring& message) noexcept {
    const std::wstring directory = LogDirectory();
    if (directory.empty()) {
        return;
    }

    SYSTEMTIME now{};
    GetLocalTime(&now);

    wchar_t prefix[96]{};
    swprintf_s(
        prefix,
        L"%04u-%02u-%02u %02u:%02u:%02u.%03u [pid=%lu] INFO ",
        now.wYear,
        now.wMonth,
        now.wDay,
        now.wHour,
        now.wMinute,
        now.wSecond,
        now.wMilliseconds,
        GetCurrentProcessId());

    const std::string line = Utf8(std::wstring(prefix) + message + L"\r\n");
    if (line.empty()) {
        return;
    }

    const std::wstring file_path = directory + L"\\taskbar-lyrics.log";
    HANDLE file = CreateFileW(
        file_path.c_str(),
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD bytes_written = 0;
    WriteFile(file, line.data(), static_cast<DWORD>(line.size()), &bytes_written, nullptr);
    CloseHandle(file);
}

}  // namespace taskbar_lyrics

