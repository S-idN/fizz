#pragma once

#include <string>
#include <string_view>
#include <chrono>
#include <functional>
#include "Config.h"

namespace fizz {

class GeneralTab {
public:
    static constexpr std::string_view kResetConfirmationText =
        "Resets all configuration options to defaults. Does not affect launch history, search index, or cached data.";

    using ConfigChangeCallback = std::function<void(const FizzConfig&)>;

    explicit GeneralTab(std::string configPath = "",
                        std::chrono::milliseconds debounceDuration = std::chrono::milliseconds(500));

    const std::string& getConfigPath() const;
    void setConfigPath(const std::string& path);

    void setOnConfigChanged(ConfigChangeCallback cb);
    void setOnReset(ConfigChangeCallback cb);

    bool setAutoClose(FizzConfig& config, bool enabled);

    void setGracePeriod(FizzConfig& config, int seconds,
                        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());

    bool update(FizzConfig& config,
                std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());
    bool flushPendingSave(FizzConfig& config);
    bool hasPendingSave() const;

    void requestReset();
    void cancelReset();
    bool confirmReset(FizzConfig& config);
    bool isResetConfirmationOpen() const;
    std::string_view getResetConfirmationMessage() const;

    void setDebounceDuration(std::chrono::milliseconds duration);
    std::chrono::milliseconds getDebounceDuration() const;

    void render(FizzConfig& config,
                std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());

    size_t getSaveCount() const;

private:
    bool save(const FizzConfig& config);

    std::string m_configPath;
    std::chrono::milliseconds m_debounceDuration;
    ConfigChangeCallback m_onConfigChanged;
    ConfigChangeCallback m_onReset;

    bool m_hasPendingSave = false;
    std::chrono::steady_clock::time_point m_lastGraceChangeTime{};
    size_t m_saveCount = 0;
    bool m_resetConfirmationOpen = false;
};

} // namespace fizz

using fizz::GeneralTab;
