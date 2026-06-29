#include "core/config.h"

#include <windows.h>

namespace taskbar_lyrics {
namespace {

bool EnsureDirectory(const std::wstring& path) {
    if (CreateDirectoryW(path.c_str(), nullptr)) {
        return true;
    }
    return GetLastError() == ERROR_ALREADY_EXISTS;
}

std::wstring ConfigPath() {
    const DWORD required = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
    if (required == 0) {
        return {};
    }

    std::wstring local_app_data(required, L'\0');
    const DWORD written =
        GetEnvironmentVariableW(L"LOCALAPPDATA", local_app_data.data(), required);
    if (written == 0 || written >= required) {
        return {};
    }
    local_app_data.resize(written);

    std::wstring app_directory = local_app_data + L"\\TaskbarLyrics";
    if (!EnsureDirectory(app_directory)) {
        return {};
    }
    return app_directory + L"\\config.ini";
}

}  // namespace

Config::Config() : path_(ConfigPath()) {
    if (!path_.empty()) {
        show_translation_ =
            GetPrivateProfileIntW(L"Display", L"ShowTranslation", 1, path_.c_str()) != 0;
    }
}

bool Config::ShowTranslation() const noexcept {
    return show_translation_;
}

bool Config::SetShowTranslation(bool value) noexcept {
    if (path_.empty()) {
        return false;
    }

    if (!WritePrivateProfileStringW(
            L"Display", L"ShowTranslation", value ? L"1" : L"0", path_.c_str())) {
        return false;
    }
    show_translation_ = value;
    return true;
}

const std::wstring& Config::Path() const noexcept {
    return path_;
}

}  // namespace taskbar_lyrics

