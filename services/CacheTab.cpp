#include "CacheTab.h"
#include "FileSearch.h"
#include "LoadFile.h"
#include "Theme.h"
#include "imgui.h"

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <filesystem>
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;

namespace fizz {

static std::string expandTildePath(const std::string& path) {
    if (path == "~") {
        return getHomeDir();
    }
    if (path.rfind("~/", 0) == 0) {
        return getHomeDir() + path.substr(1);
    }
    return path;
}

static std::string normalizeCachePath(std::string path) {
    // Trim surrounding whitespace
    size_t first = path.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = path.find_last_not_of(" \t\r\n");
    path = path.substr(first, (last - first + 1));

    path = expandTildePath(path);
    while (path.size() > 1 && path.back() == '/') {
        path.pop_back();
    }
    return path;
}

std::string formatFileSize(uint64_t bytes) {
    char buf[64];
    if (bytes >= 1024ULL * 1024ULL) {
        double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
        std::snprintf(buf, sizeof(buf), "%.2f MB", mb);
    } else {
        double kb = static_cast<double>(bytes) / 1024.0;
        std::snprintf(buf, sizeof(buf), "%.1f KB", kb);
    }
    return std::string(buf);
}

std::string formatTimestamp(time_t timeSec) {
    if (timeSec <= 0) {
        return "Never";
    }
    struct tm tmBuf;
    if (localtime_r(&timeSec, &tmBuf) == nullptr) {
        return "Unknown";
    }
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmBuf);
    return std::string(buf);
}

static bool readAll(int fd, void* buf, size_t count) {
    char* p = static_cast<char*>(buf);
    size_t remaining = count;
    while (remaining > 0) {
        ssize_t n = read(fd, p, remaining);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (n == 0) return false;
        p += n;
        remaining -= n;
    }
    return true;
}

CacheDiagnostics getCacheDiagnostics(const std::string& cachePath) {
    CacheDiagnostics diag;
    std::string targetPath = cachePath.empty() ? getBinaryCachePath() : cachePath;
    targetPath = normalizeCachePath(targetPath);

    if (targetPath.empty()) {
        diag.formattedSize = "0.0 KB";
        diag.formattedTimestamp = "Never";
        return diag;
    }

    int fd = open(targetPath.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) {
        diag.formattedSize = "0.0 KB";
        diag.formattedTimestamp = "Never";
        return diag;
    }

    struct stat st;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
        close(fd);
        diag.formattedSize = "0.0 KB";
        diag.formattedTimestamp = "Never";
        return diag;
    }

    diag.fileExists = true;
    diag.fileSizeBytes = static_cast<uint64_t>(st.st_size);
    diag.lastModified = st.st_mtime;

    const size_t headerSize = sizeof(size_t) * 2;
    if (diag.fileSizeBytes >= headerSize) {
        size_t structCount = 0;
        size_t charCount = 0;
        if (readAll(fd, &structCount, sizeof(structCount)) &&
            readAll(fd, &charCount, sizeof(charCount))) {
            diag.entryCount = structCount;
        }
    }
    close(fd);

    diag.formattedSize = formatFileSize(diag.fileSizeBytes);
    diag.formattedTimestamp = formatTimestamp(diag.lastModified);
    return diag;
}

bool validateCachePath(const std::string& path, std::string* errorMessage, uid_t currentUid) {
    std::string norm = normalizeCachePath(path);
    if (norm.empty()) {
        if (errorMessage) *errorMessage = "Cache path cannot be empty.";
        return false;
    }

    // 1. Check if the candidate path itself is a symlink or directory
    struct stat path_st;
    if (lstat(norm.c_str(), &path_st) == 0) {
        if (S_ISLNK(path_st.st_mode)) {
            if (errorMessage) *errorMessage = "Cache path cannot be a symbolic link.";
            return false;
        }
        if (S_ISDIR(path_st.st_mode)) {
            if (errorMessage) *errorMessage = "Cache path cannot be a directory.";
            return false;
        }
        if (path_st.st_uid != currentUid) {
            if (errorMessage) *errorMessage = "Cache file is owned by another user.";
            return false;
        }
        if (access(norm.c_str(), W_OK) != 0) {
            if (errorMessage) *errorMessage = "Cache file is not writable.";
            return false;
        }
    }

    // 2. Parent directory extraction and validation
    fs::path p(norm);
    fs::path parent = p.parent_path();
    if (parent.empty()) {
        parent = ".";
    }

    // Reject parent directory if it is a symlink
    struct stat parent_lstat;
    if (lstat(parent.c_str(), &parent_lstat) == 0 && S_ISLNK(parent_lstat.st_mode)) {
        if (errorMessage) *errorMessage = "Parent directory cannot be a symbolic link.";
        return false;
    }

    struct stat parent_st;
    if (stat(parent.c_str(), &parent_st) != 0 || !S_ISDIR(parent_st.st_mode)) {
        if (errorMessage) *errorMessage = "Parent directory does not exist.";
        return false;
    }

    // Check parent directory is writable
    if (access(parent.c_str(), W_OK) != 0) {
        if (errorMessage) *errorMessage = "Parent directory is not writable.";
        return false;
    }

    // Reject paths in world-writable directories not owned by current user (geteuid())
    if ((parent_st.st_mode & S_IWOTH) != 0 && parent_st.st_uid != currentUid) {
        if (errorMessage) *errorMessage = "Parent directory is world-writable and not owned by current user.";
        return false;
    }

    return true;
}

bool applyCustomCachePath(const std::string& newPath, FizzConfig& config,
                          std::string* errorMessage,
                          const std::string& configFilePath) {
    if (!validateCachePath(newPath, errorMessage)) {
        return false;
    }

    std::string norm = normalizeCachePath(newPath);
    config.binary_cache_path = norm;
    setBinaryCachePath(norm);

    if (!saveConfig(config, configFilePath)) {
        if (errorMessage) *errorMessage = "Failed to atomically save configuration.";
        return false;
    }

    // Synchronize in-memory index if the new cache exists on disk
    loadIndexToMemory();
    return true;
}

bool resetToDefaultCachePath(FizzConfig& config,
                            std::string* errorMessage,
                            const std::string& configFilePath) {
    config.binary_cache_path = "";
    setBinaryCachePath("");

    if (!saveConfig(config, configFilePath)) {
        if (errorMessage) *errorMessage = "Failed to atomically save configuration.";
        return false;
    }

    loadIndexToMemory();
    return true;
}

CacheTab::CacheTab() {
    std::memset(m_inputBuffer, 0, sizeof(m_inputBuffer));
}

CacheTab::CacheTab(const FizzConfig& config) {
    std::memset(m_inputBuffer, 0, sizeof(m_inputBuffer));
    syncWithConfig(config);
}

void CacheTab::setInputPath(const std::string& path) {
    m_inputPath = path;
    std::snprintf(m_inputBuffer, sizeof(m_inputBuffer), "%s", path.c_str());
}

void CacheTab::syncWithConfig(const FizzConfig& config) {
    if (!m_initialized || config.binary_cache_path != m_lastSyncedConfigPath) {
        m_lastSyncedConfigPath = config.binary_cache_path;
        setInputPath(config.binary_cache_path);
        m_initialized = true;
    }
}

void CacheTab::render(FizzConfig& config) {
    syncWithConfig(config);

    // Live cache diagnostics from disk
    CacheDiagnostics diag = getCacheDiagnostics(config.binary_cache_path);

    ImGui::Spacing();
    ImGui::TextColored(fizz::theme::Palette::kAccent, "Active Binary Cache Diagnostics");
    ImGui::Separator();
    ImGui::Spacing();

    std::string activePath = getBinaryCachePath();
    ImGui::Text("Active Cache Path: %s", activePath.c_str());
    ImGui::Text("Cache Status: %s", diag.fileExists ? "Online (On-disk)" : "Not Created / Missing");
    ImGui::Text("File Size: %s (%llu bytes)", diag.formattedSize.c_str(), static_cast<unsigned long long>(diag.fileSizeBytes));
    ImGui::Text("Indexed Entries: %zu paths", diag.entryCount);
    ImGui::Text("Last Modified: %s", diag.formattedTimestamp.c_str());

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextColored(fizz::theme::Palette::kAccent, "Custom Cache Destination");
    ImGui::TextDisabled("Specify an absolute file path or ~/ path for the Binary Cache.");
    ImGui::Spacing();

    float availW = ImGui::GetContentRegionAvail().x;
    float buttonsW = 180.0f;
    float inputW = std::max(200.0f, availW - buttonsW - ImGui::GetStyle().ItemSpacing.x * 2.0f);

    ImGui::SetNextItemWidth(inputW);
    bool enterPressed = ImGui::InputText("##cache_destination_input", m_inputBuffer, sizeof(m_inputBuffer),
                                        ImGuiInputTextFlags_EnterReturnsTrue);
    m_inputPath = m_inputBuffer;

    ImGui::SameLine();
    bool applyClicked = ImGui::Button("Apply##cache", {85.0f, 0});
    ImGui::SameLine();
    bool resetClicked = ImGui::Button("Reset Default##cache", {100.0f, 0});

    if (enterPressed || applyClicked) {
        std::string errMsg;
        if (applyCustomCachePath(m_inputPath, config, &errMsg)) {
            m_statusMessage = "Binary cache path updated and saved successfully.";
            m_statusIsError = false;
            m_lastSyncedConfigPath = config.binary_cache_path;
        } else {
            m_statusMessage = errMsg;
            m_statusIsError = true;
        }
    } else if (resetClicked) {
        std::string errMsg;
        if (resetToDefaultCachePath(config, &errMsg)) {
            m_statusMessage = "Binary cache path reset to default and saved.";
            m_statusIsError = false;
            m_lastSyncedConfigPath = "";
            setInputPath("");
        } else {
            m_statusMessage = errMsg;
            m_statusIsError = true;
        }
    }

    if (!m_statusMessage.empty()) {
        ImGui::Spacing();
        if (m_statusIsError) {
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "⚠️ %s", m_statusMessage.c_str());
        } else {
            ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.35f, 1.0f), "✓ %s", m_statusMessage.c_str());
        }
    }
}

} // namespace fizz
