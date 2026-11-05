#include "AppLauncher.h"
#include <fstream>
#include <sstream>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

namespace fs = std::filesystem;

bool isDesktopEntry(std::string_view path) {
    constexpr std::string_view kExt = ".desktop";
    if (path.size() < kExt.size()) {
        return false;
    }
    std::string_view tail = path.substr(path.size() - kExt.size());
    for (size_t i = 0; i < kExt.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(tail[i])) != kExt[i]) {
            return false;
        }
    }
    std::error_code ec;
    if (fs::is_directory(std::string(path), ec)) {
        return false;
    }
    return true;
}

static std::vector<std::string> tokenizeExecLine(const std::string& line) {
    std::vector<std::string> tokens;
    size_t i = 0;
    const size_t n = line.size();

    while (i < n) {
        while (i < n && std::isspace(static_cast<unsigned char>(line[i]))) {
            i++;
        }
        if (i >= n) break;

        std::string token;
        bool inQuotes = false;
        char quoteChar = '\0';

        while (i < n) {
            char c = line[i];
            if (!inQuotes && (c == '"' || c == '\'')) {
                inQuotes = true;
                quoteChar = c;
                i++;
                continue;
            }
            if (inQuotes && c == quoteChar) {
                inQuotes = false;
                quoteChar = '\0';
                i++;
                continue;
            }
            if (inQuotes && c == '\\' && i + 1 < n) {
                char next = line[i + 1];
                if (quoteChar == '"' && (next == '"' || next == '\\' || next == '$' || next == '`')) {
                    token.push_back(next);
                    i += 2;
                    continue;
                }
            }
            if (!inQuotes && c == '\\' && i + 1 < n) {
                char next = line[i + 1];
                if (next == 's') {
                    token.push_back(' ');
                } else {
                    token.push_back(next);
                }
                i += 2;
                continue;
            }
            if (!inQuotes && std::isspace(static_cast<unsigned char>(c))) {
                break;
            }
            token.push_back(c);
            i++;
        }

        // Handle FreeDesktop field codes (e.g. %f, %F, %u, %U, %i, %c, %k, %%)
        if (!token.empty() && token[0] == '%') {
            if (token == "%%") {
                tokens.push_back("%");
            }
            // Skip single-character field codes like %u, %U, %f, %F, etc.
            continue;
        }

        if (!token.empty()) {
            tokens.push_back(token);
        }
    }

    return tokens;
}

std::vector<std::string> parseDesktopExec(const std::string& desktopEntryPath) {
    std::ifstream file(desktopEntryPath);
    if (!file.is_open()) {
        return {};
    }

    std::string line;
    bool inDesktopEntrySection = false;
    std::string execLine;

    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        size_t first = 0;
        while (first < line.size() && std::isspace(static_cast<unsigned char>(line[first]))) {
            first++;
        }
        if (first >= line.size() || line[first] == '#') {
            continue;
        }

        if (line[first] == '[') {
            size_t last = line.find(']', first);
            if (last != std::string::npos) {
                std::string section = line.substr(first + 1, last - first - 1);
                inDesktopEntrySection = (section == "Desktop Entry");
            } else {
                inDesktopEntrySection = false;
            }
            continue;
        }

        if (inDesktopEntrySection) {
            constexpr std::string_view kExecPrefix = "Exec=";
            if (line.substr(first, kExecPrefix.size()) == kExecPrefix) {
                execLine = line.substr(first + kExecPrefix.size());
                break;
            }
        }
    }

    if (execLine.empty()) {
        return {};
    }

    return tokenizeExecLine(execLine);
}

static bool commandExistsInPath(const char* cmd) {
    const char* pathEnv = std::getenv("PATH");
    if (!pathEnv) pathEnv = "/usr/bin:/bin";
    std::string_view p(pathEnv);
    size_t start = 0;
    while (start < p.size()) {
        size_t end = p.find(':', start);
        if (end == std::string_view::npos) {
            end = p.size();
        }
        std::string dir(p.substr(start, end - start));
        if (!dir.empty()) {
            if (dir.back() != '/') {
                dir.push_back('/');
            }
            dir += cmd;
            if (access(dir.c_str(), X_OK) == 0) {
                return true;
            }
        }
        start = end + 1;
    }
    return false;
}

std::vector<std::string> buildLaunchCommand(
    std::string_view path,
    ExecState state,
    bool showFolder,
    DesktopLauncherBackend backend
) {
    if (showFolder) {
        std::string targetDir;
        size_t slash = path.rfind('/');
        if (slash == std::string_view::npos) {
            targetDir = ".";
        } else if (slash == 0) {
            targetDir = "/";
        } else {
            targetDir = std::string(path.substr(0, slash));
        }
        return {"xdg-open", std::move(targetDir)};
    }

    if (isDesktopEntry(path)) {
        DesktopLauncherBackend resolvedBackend = backend;
        if (resolvedBackend == DesktopLauncherBackend::Auto) {
            if (commandExistsInPath("gio")) {
                resolvedBackend = DesktopLauncherBackend::Gio;
            } else if (commandExistsInPath("gtk-launch")) {
                resolvedBackend = DesktopLauncherBackend::GtkLaunch;
            } else {
                resolvedBackend = DesktopLauncherBackend::ExecLine;
            }
        }

        if (resolvedBackend == DesktopLauncherBackend::Gio) {
            return {"gio", "launch", std::string(path)};
        }

        if (resolvedBackend == DesktopLauncherBackend::GtkLaunch) {
            size_t slash = path.rfind('/');
            std::string desktopEntryId = (slash == std::string_view::npos)
                                             ? std::string(path)
                                             : std::string(path.substr(slash + 1));
            return {"gtk-launch", std::move(desktopEntryId)};
        }

        if (resolvedBackend == DesktopLauncherBackend::ExecLine) {
            auto parsed = parseDesktopExec(std::string(path));
            if (!parsed.empty()) {
                return parsed;
            }
            return {"xdg-open", std::string(path)};
        }
    }

    if (state == ExecState::Executable) {
        return {std::string(path)};
    }

    return {"xdg-open", std::string(path)};
}

bool launchTarget(std::string_view path, ExecState state, bool showFolder) {
    std::vector<std::string> cmd = buildLaunchCommand(path, state, showFolder, DesktopLauncherBackend::Auto);
    if (cmd.empty()) {
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        return false;
    }

    if (pid == 0) {
        setsid();
        int devNullFd = open("/dev/null", O_RDWR);
        if (devNullFd >= 0) {
            dup2(devNullFd, 0);
            dup2(devNullFd, 1);
            dup2(devNullFd, 2);
            close(devNullFd);
        }

        std::vector<char*> argv;
        argv.reserve(cmd.size() + 1);
        for (auto& arg : cmd) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);

        execvp(argv[0], argv.data());
        _exit(127);
    }

    return true;
}
