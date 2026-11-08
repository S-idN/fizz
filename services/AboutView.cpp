#include "AboutView.h"
#include "Config.h"
#include "FileSearch.h"
#include "Theme.h"
#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace fizz::ui {

const std::vector<ShortcutEntry>& getDefaultShortcutCheatSheet() {
    static const std::vector<ShortcutEntry> shortcuts = {
        {"↵ (Enter)",        "Open file or launch application",                       "Execution"},
        {"⇧↵ (Shift+Enter)", "Reveal containing directory in file manager",           "Execution"},
        {"Alt+Enter",        "Run selected executable or script in terminal",         "Terminal Execution"},
        {"Ctrl+Enter",       "Launch untrusted desktop entry bypassing security prompt", "Security"},
        {"Tab / ⇧Tab",       "Navigate forward / backward between controls",          "Navigation"},
        {"↑ / ↓",            "Navigate up / down through search results",             "Navigation"},
        {"Esc",              "Return to Search view or dismiss launcher",             "Navigation"},
        {"Ctrl+,",           "Open Settings view",                                    "View Modes"},
        {"F1",               "Open About and diagnostics view",                       "View Modes"},
        {":settings / :about", "Switch view mode via search query",                   "View Modes"}
    };
    return shortcuts;
}

std::string formatShortcutTable(const std::vector<ShortcutEntry>& shortcuts) {
    if (shortcuts.empty()) return "";

    size_t maxKey = std::string("Shortcut").size();
    size_t maxAction = std::string("Action").size();
    size_t maxCat = std::string("Category").size();

    for (const auto& s : shortcuts) {
        maxKey = std::max(maxKey, s.key.size());
        maxAction = std::max(maxAction, s.action.size());
        maxCat = std::max(maxCat, s.category.size());
    }

    std::ostringstream ss;
    ss << "| " << std::left << std::setw(maxKey) << "Shortcut"
       << " | " << std::left << std::setw(maxAction) << "Action"
       << " | " << std::left << std::setw(maxCat) << "Category" << " |\n";

    ss << "|-" << std::string(maxKey, '-')
       << "-|-" << std::string(maxAction, '-')
       << "-|-" << std::string(maxCat, '-') << "-|\n";

    for (const auto& s : shortcuts) {
        ss << "| " << std::left << std::setw(maxKey) << s.key
           << " | " << std::left << std::setw(maxAction) << s.action
           << " | " << std::left << std::setw(maxCat) << s.category << " |\n";
    }

    return ss.str();
}

std::string formatDaemonStatus(bool isRunning, pid_t pid, bool watchLimitReached) {
    if (!isRunning) {
        return "● fizzd: stopped";
    }
    if (watchLimitReached) {
        return "⚠️ fizzd: running (watch limit reached)";
    }
    if (pid > 0) {
        return "● fizzd: running (" + std::to_string(pid) + ")";
    }
    return "● fizzd: running";
}

std::string formatFileSize(uint64_t bytes) {
    if (bytes == 0) return "0 B";
    constexpr uint64_t KB = 1024;
    constexpr uint64_t MB = 1024 * KB;
    constexpr uint64_t GB = 1024 * MB;

    char buf[64];
    if (bytes >= GB) {
        std::snprintf(buf, sizeof(buf), "%.1f GB (%lu bytes)", static_cast<double>(bytes) / GB, bytes);
    } else if (bytes >= MB) {
        std::snprintf(buf, sizeof(buf), "%.1f MB (%lu bytes)", static_cast<double>(bytes) / MB, bytes);
    } else if (bytes >= KB) {
        std::snprintf(buf, sizeof(buf), "%.1f KB (%lu bytes)", static_cast<double>(bytes) / KB, bytes);
    } else {
        std::snprintf(buf, sizeof(buf), "%lu bytes", bytes);
    }
    return buf;
}

std::string formatTimestamp(time_t t) {
    if (t == 0) return "Never";
    struct tm tmBuf;
    if (localtime_r(&t, &tmBuf) == nullptr) return "Unknown";
    char buf[32];
    if (std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmBuf) == 0) {
        return "Unknown";
    }
    return buf;
}

CacheDiagnostics collectCacheDiagnostics(const std::string& customCachePath) {
    CacheDiagnostics diag;
    diag.path = !customCachePath.empty() ? customCachePath : getBinaryCachePath();

    struct stat st;
    if (stat(diag.path.c_str(), &st) != 0) {
        diag.exists = false;
        diag.totalEntries = 0;
        diag.fileSizeBytes = 0;
        diag.formattedFileSize = "0 B";
        diag.lastRebuildTime = "Not found";
        diag.lastRebuildTimestamp = 0;
        return diag;
    }

    diag.exists = true;
    diag.fileSizeBytes = static_cast<uint64_t>(st.st_size);
    diag.formattedFileSize = formatFileSize(diag.fileSizeBytes);
    diag.lastRebuildTimestamp = st.st_mtime;
    diag.lastRebuildTime = formatTimestamp(st.st_mtime);

    // Read indexed item count directly from binary cache header
    int fd = open(diag.path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd >= 0) {
        size_t structCount = 0;
        if (read(fd, &structCount, sizeof(structCount)) == sizeof(structCount)) {
            diag.totalEntries = structCount;
        }
        close(fd);
    } else if (!fileStructTable.empty()) {
        diag.totalEntries = fileStructTable.size();
    }

    return diag;
}

DaemonDiagnostics collectDaemonDiagnostics(const std::string& customRuntimeDir) {
    DaemonDiagnostics diag;
    std::string rtDir = !customRuntimeDir.empty() ? customRuntimeDir : getUserRuntimeDir();
    if (rtDir.empty()) {
        diag.isRunning = false;
        diag.pid = 0;
        diag.watchLimitReached = false;
        diag.statusIndicator = formatDaemonStatus(false, 0, false);
        return diag;
    }

    std::string lockPath = rtDir + "/fizzd.lock";
    std::string watchLimitPath = rtDir + "/fizzd.watch_limit";

    std::error_code ec;
    if (fs::exists(watchLimitPath, ec)) {
        diag.watchLimitReached = true;
    }

    int fd = open(lockPath.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd >= 0) {
        char buf[256] = {0};
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        std::string content;
        if (n > 0) {
            content.assign(buf, n);
            // Check for watch limit sentinel in content
            if (content.find("watch_limit") != std::string::npos ||
                content.find("watch limit") != std::string::npos) {
                diag.watchLimitReached = true;
            }
            // Parse PID from content
            char* endPtr = nullptr;
            long parsed = std::strtol(buf, &endPtr, 10);
            if (parsed > 0 && parsed <= 4194304) {
                diag.pid = static_cast<pid_t>(parsed);
            }
        }

        // Test exclusive flock
        int lockRes = flock(fd, LOCK_EX | LOCK_NB);
        if (lockRes < 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
            // Lock is actively held by daemon
            diag.isRunning = true;
        } else if (lockRes == 0) {
            // Lock is NOT held
            flock(fd, LOCK_UN);
            if (diag.pid > 0 && (kill(diag.pid, 0) == 0 || errno == EPERM)) {
                diag.isRunning = true;
            } else {
                diag.isRunning = false;
            }
        }
        close(fd);
    }

    // Fallback: if not running according to lockfile, scan /proc for running fizzd process
    if (!diag.isRunning) {
        DIR* dir = opendir("/proc");
        if (dir) {
            struct dirent* ent;
            while ((ent = readdir(dir)) != nullptr) {
                if (ent->d_type == DT_DIR && std::isdigit(static_cast<unsigned char>(ent->d_name[0]))) {
                    std::string commPath = std::string("/proc/") + ent->d_name + "/comm";
                    std::ifstream f(commPath);
                    std::string comm;
                    if (f >> comm && comm == "fizzd") {
                        long foundPid = std::strtol(ent->d_name, nullptr, 10);
                        if (foundPid > 0) {
                            diag.isRunning = true;
                            diag.pid = static_cast<pid_t>(foundPid);
                            break;
                        }
                    }
                }
            }
            closedir(dir);
        }
    }

    diag.statusIndicator = formatDaemonStatus(diag.isRunning, diag.pid, diag.watchLimitReached);
    return diag;
}

bool AboutViewDiagnostics::refresh(TimePoint now, bool force) {
    if (!force && m_hasPolled && (now - m_lastPollTime < kPollInterval)) {
        return false;
    }

    m_cacheDiag = collectCacheDiagnostics(m_customCachePath);
    m_daemonDiag = collectDaemonDiagnostics(m_customRuntimeDir);

    m_lastPollTime = now;
    m_hasPolled = true;
    m_pollCount++;
    return true;
}

void AboutViewDiagnostics::reset() {
    m_lastPollTime = {};
    m_hasPolled = false;
    m_pollCount = 0;
    m_cacheDiag = {};
    m_daemonDiag = {};
}

void renderAboutView(const FizzConfig& config, ViewModeStateMachine& stateMachine, AboutViewDiagnostics* diagnostics) {
    static AboutViewDiagnostics s_defaultDiagnostics;
    AboutViewDiagnostics& diag = diagnostics ? *diagnostics : s_defaultDiagnostics;

    // Rate-limited polling (throttled to at most once per second)
    diag.refresh();

    const auto& cache = diag.getCacheDiagnostics();
    const auto& daemon = diag.getDaemonDiagnostics();

    ImGui::BeginChild("about_container", {0, 0}, false);
    ImGui::Spacing();

    // Section 1: Application Metadata
    ImGui::TextColored(fizz::theme::Palette::kAccent, "Fizz Application Launcher");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", std::string(kAppVersion).c_str());
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextWrapped("A fast, keyboard-driven desktop fuzzy file and application launcher for Linux.");
    ImGui::Spacing();

    ImGui::Text("License:       %s", std::string(kAppLicense).c_str());
    ImGui::Text("Repository:    %s", std::string(kRepoUrl).c_str());
    std::string configPath = getConfigPath();
    ImGui::Text("Configuration: %s", configPath.empty() ? "None (using defaults)" : configPath.c_str());
    ImGui::Spacing();

    // Section 2: Live Diagnostic Indicators
    ImGui::TextColored(fizz::theme::Palette::kAccent, "System & Cache Diagnostics");
    ImGui::Separator();
    ImGui::Spacing();

    // Index Daemon Indicator
    if (!daemon.isRunning) {
        ImGui::TextColored(fizz::theme::Palette::kDim, "%s", daemon.statusIndicator.c_str());
    } else if (daemon.watchLimitReached) {
        ImGui::TextColored(fizz::theme::Palette::kAmber, "%s", daemon.statusIndicator.c_str());
    } else {
        ImGui::TextColored(fizz::theme::Palette::kTagGreen, "%s", daemon.statusIndicator.c_str());
    }

    // Binary Cache Diagnostics
    ImGui::Text("Binary Cache: %s", cache.path.c_str());
    if (cache.exists) {
        ImGui::BulletText("Total Indexed Items: %zu", cache.totalEntries);
        ImGui::BulletText("Disk File Size:      %s", cache.formattedFileSize.c_str());
        ImGui::BulletText("Last Rebuilt:        %s", cache.lastRebuildTime.c_str());
    } else {
        ImGui::BulletText("Total Indexed Items: 0 (Cache not found)");
        ImGui::BulletText("Disk File Size:      0 B");
        ImGui::BulletText("Last Rebuilt:        Never");
    }
    ImGui::Spacing();

    // Section 3: Keyboard Navigation Cheat Sheet Table
    ImGui::TextColored(fizz::theme::Palette::kAccent, "Keyboard Navigation Cheat Sheet");
    ImGui::Separator();
    ImGui::Spacing();

    const auto& shortcuts = getDefaultShortcutCheatSheet();
    if (ImGui::BeginTable("shortcut_cheat_sheet", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableHeadersRow();

        for (const auto& item : shortcuts) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(fizz::theme::Palette::kAccent, "%s", item.key.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(item.action.c_str());
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", item.category.c_str());
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Press Escape or click '← Back to Search' to return.");
    ImGui::EndChild();
}

} // namespace fizz::ui
