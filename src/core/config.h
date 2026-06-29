#pragma once

#include <string>

namespace taskbar_lyrics {

class Config {
public:
    Config();

    [[nodiscard]] bool ShowTranslation() const noexcept;
    bool SetShowTranslation(bool value) noexcept;
    [[nodiscard]] const std::wstring& Path() const noexcept;

private:
    std::wstring path_;
    bool show_translation_ = true;
};

}  // namespace taskbar_lyrics

