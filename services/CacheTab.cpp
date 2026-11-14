#include "CacheTab.h"
#include "FileSearch.h"
#include "LoadFile.h"
#include "Theme.h"
#include "imgui.h"

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cmath>
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
            if (errorMessage) *errorMessage = "Binary cache is owned by another user.";
            return false;
        }
        if (access(norm.c_str(), W_OK) != 0) {
            if (errorMessage) *errorMessage = "Binary cache is not writable.";
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

static void renderSpinner(float radius, float thickness, ImU32 color) {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(radius * 2.0f, radius * 2.0f));

    float time = static_cast<float>(ImGui::GetTime());
    int num_segments = 24;
    float start_angle = time * 6.0f;
    float end_angle = start_angle + 1.5f * 3.14159265f;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (dl) {
        dl->PathClear();
        dl->PathArcTo(ImVec2(pos.x + radius, pos.y + radius), std::max(1.0f, radius - thickness * 0.5f), start_angle, end_angle, num_segments);
        dl->PathStroke(color, false, thickness);
    }
}

CacheTab::CacheTab() {
    std::memset(m_inputBuffer, 0, sizeof(m_inputBuffer));
}

CacheTab::CacheTab(const FizzConfig& config) {
    std::memset(m_inputBuffer, 0, sizeof(m_inputBuffer));
    syncWithConfig(config);
}

CacheTab::~CacheTab() {
    m_stopRebuildWatcher = true;
    if (m_rebuildThread.joinable()) {
        m_rebuildThread.join();
    }
}

CacheTab::CacheTab(CacheTab&& other) noexcept {
    other.m_stopRebuildWatcher = true;
    if (other.m_rebuildThread.joinable()) {
        other.m_rebuildThread.join();
    }
    std::memcpy(m_inputBuffer, other.m_inputBuffer, sizeof(m_inputBuffer));
    m_inputPath = std::move(other.m_inputPath);
    m_lastSyncedConfigPath = std::move(other.m_lastSyncedConfigPath);
    {
        std::lock_guard<std::mutex> lock(other.m_statusMutex);
        m_statusMessage = std::move(other.m_statusMessage);
        m_statusIsError = other.m_statusIsError;
    }
    m_initialized = other.m_initialized;
    m_rebuilding = other.m_rebuilding.load();
    m_daemonClient = std::move(other.m_daemonClient);
}

CacheTab& CacheTab::operator=(CacheTab&& other) noexcept {
    if (this != &other) {
        m_stopRebuildWatcher = true;
        if (m_rebuildThread.joinable()) {
            m_rebuildThread.join();
        }
        other.m_stopRebuildWatcher = true;
        if (other.m_rebuildThread.joinable()) {
            other.m_rebuildThread.join();
        }
        std::memcpy(m_inputBuffer, other.m_inputBuffer, sizeof(m_inputBuffer));
        m_inputPath = std::move(other.m_inputPath);
        m_lastSyncedConfigPath = std::move(other.m_lastSyncedConfigPath);
        {
            std::lock_guard<std::mutex> lock(other.m_statusMutex);
            m_statusMessage = std::move(other.m_statusMessage);
            m_statusIsError = other.m_statusIsError;
        }
        m_initialized = other.m_initialized;
        m_rebuilding = other.m_rebuilding.load();
        m_daemonClient = std::move(other.m_daemonClient);
    }
    return *this;
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

bool CacheTab::isRebuilding() const {
    return m_rebuilding.load();
}

bool CacheTab::isSpinnerActive() const {
    return isRebuilding();
}

void CacheTab::setRebuilding(bool rebuilding) {
    m_rebuilding = rebuilding;
}

void CacheTab::waitForRebuildCompletion(int timeoutMs) {
    auto start = std::chrono::steady_clock::now();
    while (m_rebuilding.load()) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() > timeoutMs) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if (m_rebuildThread.joinable()) {
        m_rebuildThread.join();
    }
}

std::string CacheTab::getStatusMessage() const {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    return m_statusMessage;
}

bool CacheTab::isStatusError() const {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    return m_statusIsError;
}

void CacheTab::triggerRebuild(const FizzConfig& config) {
    if (m_rebuilding.load()) {
        return;
    }

    if (m_rebuildThread.joinable()) {
        m_rebuildThread.join();
    }

    std::string activePath = config.binary_cache_path.empty() ? getBinaryCachePath() : config.binary_cache_path;
    CacheDiagnostics initialDiag = getCacheDiagnostics(activePath);

    if (m_daemonClient.isCooldownActive()) {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        m_statusMessage = "Rebuild rate-limited: please wait before retrying.";
        m_statusIsError = true;
        return;
    }

    m_rebuilding = true;
    m_stopRebuildWatcher = false;
    {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        m_statusMessage = "Rebuilding cache in background...";
        m_statusIsError = false;
    }

    m_rebuildThread = std::thread([this, activePath, initialDiag]() {
        DaemonTriggerStatus res = m_daemonClient.triggerRebuild();
        if (res == DaemonTriggerStatus::CooldownIgnored) {
            std::lock_guard<std::mutex> lock(m_statusMutex);
            m_statusMessage = "Rebuild rate-limited: please wait before retrying.";
            m_statusIsError = true;
            m_rebuilding = false;
            return;
        }

        if (res == DaemonTriggerStatus::DaemonNotRunning || res == DaemonTriggerStatus::Failed) {
            std::lock_guard<std::mutex> lock(m_statusMutex);
            m_statusMessage = "Failed to trigger rebuild: Index Daemon is not running and systemd fallback failed.";
            m_statusIsError = true;
            m_rebuilding = false;
            return;
        }

        // Wait for newly published cache
        auto start = std::chrono::steady_clock::now();
        bool published = false;
        while (!m_stopRebuildWatcher && (std::chrono::steady_clock::now() - start) < std::chrono::seconds(10)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            CacheDiagnostics curr = getCacheDiagnostics(activePath);
            if ((!initialDiag.fileExists && curr.fileExists) ||
                (curr.lastModified > initialDiag.lastModified) ||
                (curr.fileSizeBytes != initialDiag.fileSizeBytes && curr.fileExists) ||
                (curr.entryCount != initialDiag.entryCount && curr.fileExists)) {
                published = true;
                break;
            }
        }

        // Re-read in-memory search cache so query results reflect newly indexed entries
        loadIndexToMemory();

        {
            std::lock_guard<std::mutex> lock(m_statusMutex);
            m_rebuilding = false;
            if (published) {
                m_statusMessage = "Cache rebuild completed successfully.";
                m_statusIsError = false;
            } else {
                m_statusMessage = (res == DaemonTriggerStatus::SuccessSignal)
                    ? "Cache rebuild signal dispatched."
                    : "Cache daemon restarted via systemd.";
                m_statusIsError = false;
            }
        }
    });
}

void CacheTab::render(FizzConfig& config) {
    syncWithConfig(config);

    // Reap background worker thread if rebuild is done
    if (!m_rebuilding.load() && m_rebuildThread.joinable()) {
        m_rebuildThread.join();
    }

    // Live cache diagnostics from disk (refreshes indicators when new cache is published)
    CacheDiagnostics diag = getCacheDiagnostics(config.binary_cache_path);

    ImGui::Spacing();
    ImGui::TextColored(fizz::theme::getActiveAccent(), "Active Binary Cache Diagnostics");
    ImGui::Separator();
    ImGui::Spacing();

    std::string activePath = getBinaryCachePath();
    ImGui::Text("Active Cache Path: %s", activePath.c_str());
    ImGui::Text("Cache Status: %s", diag.fileExists ? "Online (On-disk)" : "Not Created / Missing");
    ImGui::Text("File Size: %s (%llu bytes)", diag.formattedSize.c_str(), static_cast<unsigned long long>(diag.fileSizeBytes));
    ImGui::Text("Indexed Entries: %zu paths", diag.entryCount);
    ImGui::Text("Last Modified: %s", diag.formattedTimestamp.c_str());

    ImGui::Spacing();

    // Rebuild Cache action button and active loading spinner
    bool inProgress = isRebuilding();
    if (inProgress) {
        ImGui::BeginDisabled();
        ImGui::Button("Rebuild Cache##action", {120.0f, 0});
        ImGui::EndDisabled();

        ImGui::SameLine();
        renderSpinner(8.0f, 2.5f, ImGui::GetColorU32(fizz::theme::getActiveAccent()));
        ImGui::SameLine();
        ImGui::TextColored(fizz::theme::getActiveAccent(), "Rebuilding index in background...");
    } else {
        if (ImGui::Button("Rebuild Cache##action", {120.0f, 0})) {
            triggerRebuild(config);
        }
        if (m_daemonClient.isCooldownActive()) {
            ImGui::SameLine();
            ImGui::TextDisabled("(Cooldown: %.0fs)", std::ceil(m_daemonClient.getCooldownRemainingSeconds()));
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextColored(fizz::theme::getActiveAccent(), "Custom Cache Destination");
    ImGui::Spacing();
    ImGui::TextDisabled("Specify an absolute file path or ~/ path for the Binary Cache.");
    ImGui::Spacing();

    float availW = ImGui::GetContentRegionAvail().x;
    float btnApplyW = 85.0f;
    float btnResetW = 100.0f;
    float buttonsW = btnApplyW + btnResetW;
    float inputW = std::max(150.0f, availW - buttonsW - ImGui::GetStyle().ItemSpacing.x * 2.0f);

    ImGui::SetNextItemWidth(inputW);
    bool enterPressed = ImGui::InputText("##cache_destination_input", m_inputBuffer, sizeof(m_inputBuffer),
                                        ImGuiInputTextFlags_EnterReturnsTrue);
    m_inputPath = m_inputBuffer;

    ImGui::SameLine();
    bool applyClicked = ImGui::Button("Apply##cache", {btnApplyW, 0});
    ImGui::SameLine();
    bool resetClicked = ImGui::Button("Reset Default##cache", {btnResetW, 0});

    if (enterPressed || applyClicked) {
        std::string errMsg;
        if (applyCustomCachePath(m_inputPath, config, &errMsg)) {
            std::lock_guard<std::mutex> lock(m_statusMutex);
            m_statusMessage = "Binary cache path updated and saved successfully.";
            m_statusIsError = false;
            m_lastSyncedConfigPath = config.binary_cache_path;
        } else {
            std::lock_guard<std::mutex> lock(m_statusMutex);
            m_statusMessage = errMsg;
            m_statusIsError = true;
        }
    } else if (resetClicked) {
        std::string errMsg;
        if (resetToDefaultCachePath(config, &errMsg)) {
            std::lock_guard<std::mutex> lock(m_statusMutex);
            m_statusMessage = "Binary cache path reset to default and saved.";
            m_statusIsError = false;
            m_lastSyncedConfigPath = "";
            setInputPath("");
        } else {
            std::lock_guard<std::mutex> lock(m_statusMutex);
            m_statusMessage = errMsg;
            m_statusIsError = true;
        }
    }

    std::string currentStatus;
    bool isError = false;
    {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        currentStatus = m_statusMessage;
        isError = m_statusIsError;
    }

    if (!currentStatus.empty()) {
        ImGui::Spacing();
        if (isError) {
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "⚠️ %s", currentStatus.c_str());
        } else {
            ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.35f, 1.0f), "✓ %s", currentStatus.c_str());
        }
    }
}

} // namespace fizz
