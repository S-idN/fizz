#include "FontDiscovery.h"
#include <filesystem>
#include <unistd.h>
#include <climits>
#include <cstring>
#include <strings.h>
#include <algorithm>
#include <unordered_set>
#include <fontconfig/fontconfig.h>

namespace fs = std::filesystem;

static const std::vector<std::string> kRelativeCandidates = {
    // 1. Adwaita Sans (Fedora, Arch, Debian/Ubuntu)
    "usr/share/fonts/adwaita-sans-fonts/AdwaitaSans-Regular.ttf",
    "usr/share/fonts/adwaita-sans/AdwaitaSans-Regular.ttf",
    "usr/share/fonts/truetype/adwaita/AdwaitaSans-Regular.ttf",

    // 2. Cantarell (Fedora, Arch, Debian/Ubuntu)
    "usr/share/fonts/abattis-cantarell-fonts/Cantarell-Regular.otf",
    "usr/share/fonts/cantarell/Cantarell-VF.otf",
    "usr/share/fonts/cantarell/Cantarell-Regular.otf",
    "usr/share/fonts/opentype/cantarell/Cantarell-VF.otf",
    "usr/share/fonts/opentype/cantarell/Cantarell-Regular.otf",
    "usr/share/fonts/truetype/cantarell/Cantarell-Regular.ttf",

    // 3. Google Sans (Debian/Ubuntu, Arch, Generic)
    "usr/share/fonts/google-sans/GoogleSans-Regular.ttf",
    "usr/share/fonts/truetype/google-sans/GoogleSans-Regular.ttf",

    // 4. DejaVu Sans (Fedora, Debian/Ubuntu, Arch)
    "usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
    "usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "usr/share/fonts/TTF/DejaVuSans.ttf",

    // 5. JetBrains Mono (Fedora, Debian/Ubuntu, Arch)
    "usr/share/fonts/jetbrains-mono-fonts/JetBrainsMono-Regular.ttf",
    "usr/share/fonts/truetype/jetbrains-mono/JetBrainsMono-Regular.ttf",
    "usr/share/fonts/TTF/JetBrainsMono-Regular.ttf",
    "usr/share/fonts/jetbrains-mono/JetBrainsMono-Regular.ttf"
};

static bool tryAssetCandidate(const fs::path& p, std::string& out) {
    if (p.empty()) return false;
    std::error_code ec;
    if (fs::is_regular_file(p, ec) || fs::is_symlink(p, ec)) {
        if (access(p.c_str(), R_OK) == 0) {
            out = p.lexically_normal().string();
            return true;
        }
    }
    return false;
}

static bool probeAssetDir(const fs::path& dir,
                          const std::string& rel,
                          const std::string& stripped,
                          const std::string& withAssets,
                          std::string& out) {
    if (dir.empty()) return false;
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return false;

    if (dir.filename() == "assets") {
        if (tryAssetCandidate(dir / stripped, out)) return true;
        if (tryAssetCandidate(dir / rel, out)) return true;
    } else {
        if (tryAssetCandidate(dir / withAssets, out)) return true;
        if (tryAssetCandidate(dir / rel, out)) return true;
        if (tryAssetCandidate(dir / stripped, out)) return true;
    }
    return false;
}

std::string resolveAssetFile(const std::string& relativePath, const std::string& customRoot) {
    if (relativePath.empty()) {
        return "";
    }

    std::string out;

    // 1. Handle absolute paths
    if (relativePath.front() == '/') {
        if (!customRoot.empty()) {
            fs::path p = fs::path(customRoot) / relativePath.substr(1);
            if (tryAssetCandidate(p, out)) return out;
        }
        if (tryAssetCandidate(fs::path(relativePath), out)) return out;
        return "";
    }

    // Normalize relative path and derive variants
    std::string rel = fs::path(relativePath).lexically_normal().string();
    std::string stripped;
    std::string withAssets;
    if (rel == "assets") {
        stripped = "";
        withAssets = "assets";
    } else if (rel.rfind("assets/", 0) == 0) {
        stripped = rel.substr(7);
        withAssets = rel;
    } else {
        stripped = rel;
        withAssets = "assets/" + rel;
    }

    // 2. Custom root probe (if specified)
    if (!customRoot.empty()) {
        fs::path cr(customRoot);
        if (probeAssetDir(cr, rel, stripped, withAssets, out)) return out;
        if (probeAssetDir(cr / "assets", rel, stripped, withAssets, out)) return out;
        if (probeAssetDir(cr / "fizz" / "assets", rel, stripped, withAssets, out)) return out;
        if (probeAssetDir(cr / "usr" / "share" / "fizz" / "assets", rel, stripped, withAssets, out)) return out;
        if (probeAssetDir(cr / "usr" / "share" / "fizz", rel, stripped, withAssets, out)) return out;
        if (probeAssetDir(cr / "usr" / "local" / "share" / "fizz" / "assets", rel, stripped, withAssets, out)) return out;
        if (probeAssetDir(cr / ".local" / "share" / "fizz" / "assets", rel, stripped, withAssets, out)) return out;
        if (tryAssetCandidate(cr / rel, out)) return out;
        if (tryAssetCandidate(cr / stripped, out)) return out;
    }

    // 3. FIZZ_DATA_DIR environment variable override
    const char* envFizzData = std::getenv("FIZZ_DATA_DIR");
    if (envFizzData && *envFizzData != '\0') {
        fs::path p(envFizzData);
        if (probeAssetDir(p, rel, stripped, withAssets, out)) return out;
        if (probeAssetDir(p / "assets", rel, stripped, withAssets, out)) return out;
        if (tryAssetCandidate(p / rel, out)) return out;
        if (tryAssetCandidate(p / stripped, out)) return out;
        // FIZZ_DATA_DIR is an explicit directory override
        return "";
    }

    // 4. XDG_DATA_HOME environment variable
    const char* xdgDataHome = std::getenv("XDG_DATA_HOME");
    if (xdgDataHome && *xdgDataHome != '\0') {
        fs::path p(xdgDataHome);
        if (probeAssetDir(p / "fizz" / "assets", rel, stripped, withAssets, out)) return out;
        if (probeAssetDir(p / "fizz", rel, stripped, withAssets, out)) return out;
    }

    // 5. User FreeDesktop data dir (~/.local/share)
    const char* home = std::getenv("HOME");
    if (home && *home != '\0') {
        fs::path p(home);
        if (probeAssetDir(p / ".local" / "share" / "fizz" / "assets", rel, stripped, withAssets, out)) return out;
        if (probeAssetDir(p / ".local" / "share" / "fizz", rel, stripped, withAssets, out)) return out;
    }

    // 6. XDG_DATA_DIRS environment variable
    const char* xdgDataDirs = std::getenv("XDG_DATA_DIRS");
    if (xdgDataDirs && *xdgDataDirs != '\0') {
        std::string s(xdgDataDirs);
        size_t start = 0;
        while (start < s.size()) {
            size_t end = s.find(':', start);
            if (end == std::string::npos) end = s.size();
            std::string d = s.substr(start, end - start);
            if (!d.empty()) {
                fs::path p(d);
                if (probeAssetDir(p / "fizz" / "assets", rel, stripped, withAssets, out)) return out;
                if (probeAssetDir(p / "fizz", rel, stripped, withAssets, out)) return out;
            }
            start = end + 1;
        }
    }

    // 7. Standard system data directories
    if (probeAssetDir(fs::path("/usr/local/share/fizz/assets"), rel, stripped, withAssets, out)) return out;
    if (probeAssetDir(fs::path("/usr/local/share/fizz"), rel, stripped, withAssets, out)) return out;
    if (probeAssetDir(fs::path("/usr/share/fizz/assets"), rel, stripped, withAssets, out)) return out;
    if (probeAssetDir(fs::path("/usr/share/fizz"), rel, stripped, withAssets, out)) return out;

    // 8. Executable binary-relative paths
    char exePath[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len > 0) {
        exePath[len] = '\0';
        fs::path exeDir = fs::path(exePath).parent_path();
        if (probeAssetDir(exeDir / ".." / "share" / "fizz" / "assets", rel, stripped, withAssets, out)) return out;
        if (probeAssetDir(exeDir / ".." / "share" / "fizz", rel, stripped, withAssets, out)) return out;
        if (probeAssetDir(exeDir, rel, stripped, withAssets, out)) return out;
        if (probeAssetDir(exeDir.parent_path(), rel, stripped, withAssets, out)) return out;
        if (probeAssetDir(exeDir / "assets", rel, stripped, withAssets, out)) return out;
    }

    // 9. Working directory and relative fallbacks
    if (probeAssetDir(fs::path("."), rel, stripped, withAssets, out)) return out;
    if (probeAssetDir(fs::path(".."), rel, stripped, withAssets, out)) return out;
    if (probeAssetDir(fs::path("../.."), rel, stripped, withAssets, out)) return out;
    if (tryAssetCandidate(fs::path(rel), out)) return out;
    if (tryAssetCandidate(fs::path("../" + rel), out)) return out;
    if (tryAssetCandidate(fs::path("../../" + rel), out)) return out;

    return "";
}

std::vector<std::string> getDefaultFontCandidates(const std::string& sysRoot) {
    std::vector<std::string> result;
    result.reserve(kRelativeCandidates.size());

    fs::path base = sysRoot.empty() ? fs::path("/") : fs::path(sysRoot);

    for (const auto& rel : kRelativeCandidates) {
        fs::path p = base / rel;
        result.push_back(p.lexically_normal().string());
    }

    return result;
}

std::string resolveSystemFont(const std::string& sysRoot) {
    const std::vector<std::string> candidates = getDefaultFontCandidates(sysRoot);

    for (const auto& path : candidates) {
        if (access(path.c_str(), R_OK) == 0) {
            return path;
        }
    }

    return "";
}

struct FontGroup {
    const char* familyName;
    std::vector<const char*> relativePaths;
};

static const FontGroup kFontGroups[] = {
    {"Adwaita Sans", {
        "usr/share/fonts/adwaita-sans-fonts/AdwaitaSans-Regular.ttf",
        "usr/share/fonts/adwaita-sans/AdwaitaSans-Regular.ttf",
        "usr/share/fonts/truetype/adwaita/AdwaitaSans-Regular.ttf"
    }},
    {"Cantarell", {
        "usr/share/fonts/abattis-cantarell-fonts/Cantarell-Regular.otf",
        "usr/share/fonts/cantarell/Cantarell-VF.otf",
        "usr/share/fonts/cantarell/Cantarell-Regular.otf",
        "usr/share/fonts/opentype/cantarell/Cantarell-VF.otf",
        "usr/share/fonts/opentype/cantarell/Cantarell-Regular.otf",
        "usr/share/fonts/truetype/cantarell/Cantarell-Regular.ttf"
    }},
    {"Google Sans", {
        "usr/share/fonts/google-sans/GoogleSans-Regular.ttf",
        "usr/share/fonts/truetype/google-sans/GoogleSans-Regular.ttf"
    }},
    {"DejaVu Sans", {
        "usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
        "usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "usr/share/fonts/TTF/DejaVuSans.ttf"
    }},
    {"JetBrains Mono", {
        "usr/share/fonts/jetbrains-mono-fonts/JetBrainsMono-Regular.ttf",
        "usr/share/fonts/truetype/jetbrains-mono/JetBrainsMono-Regular.ttf",
        "usr/share/fonts/TTF/JetBrainsMono-Regular.ttf",
        "usr/share/fonts/jetbrains-mono/JetBrainsMono-Regular.ttf"
    }}
};

std::vector<DiscoveredFont> getDiscoveredSystemFonts(const std::string& sysRoot) {
    std::vector<DiscoveredFont> result;

    // 1. Establish bundled Space Grotesk font at the top as default option
    std::string bundledPath = resolveAssetFile("assets/fonts/SpaceGrotesk-VariableFont_wght.ttf", sysRoot);
    DiscoveredFont bundled;
    bundled.familyName = "Space Grotesk (Bundled Default)";
    bundled.path = bundledPath;
    bundled.available = !bundledPath.empty() && access(bundledPath.c_str(), R_OK) == 0;
    result.push_back(std::move(bundled));

    std::vector<DiscoveredFont> systemFonts;

    if (sysRoot.empty()) {
        // Query Fontconfig for scalable system fonts on the host system
        FcConfig* fcConfig = FcInitLoadConfigAndFonts();
        if (fcConfig) {
            FcPattern* pat = FcPatternCreate();
            if (pat) {
                FcPatternAddBool(pat, FC_SCALABLE, FcTrue);
                FcObjectSet* os = FcObjectSetBuild(FC_FAMILY, FC_FILE, nullptr);
                if (os) {
                    FcFontSet* fs = FcFontList(fcConfig, pat, os);
                    if (fs) {
                        std::unordered_set<std::string> seenFamilies;
                        for (int i = 0; i < fs->nfont; ++i) {
                            FcChar8* family = nullptr;
                            FcChar8* file = nullptr;
                            if (FcPatternGetString(fs->fonts[i], FC_FAMILY, 0, &family) == FcResultMatch && family &&
                                FcPatternGetString(fs->fonts[i], FC_FILE, 0, &file) == FcResultMatch && file) {
                                std::string fam = reinterpret_cast<const char*>(family);
                                std::string filePath = reinterpret_cast<const char*>(file);
                                if (fam.empty() || filePath.empty()) continue;
                                if (access(filePath.c_str(), R_OK) != 0) continue;

                                std::string lowerFam = fam;
                                std::transform(lowerFam.begin(), lowerFam.end(), lowerFam.begin(), [](unsigned char c) {
                                    return std::tolower(c);
                                });

                                if (seenFamilies.insert(lowerFam).second) {
                                    DiscoveredFont df;
                                    df.familyName = fam;
                                    df.path = filePath;
                                    df.available = true;
                                    systemFonts.push_back(std::move(df));
                                }
                            }
                        }
                        FcFontSetDestroy(fs);
                    }
                    FcObjectSetDestroy(os);
                }
                FcPatternDestroy(pat);
            }
            FcConfigDestroy(fcConfig);
        }

        // Deduplicated font families sorted alphabetically (case-insensitive)
        std::sort(systemFonts.begin(), systemFonts.end(), [](const DiscoveredFont& a, const DiscoveredFont& b) {
            int cmp = strcasecmp(a.familyName.c_str(), b.familyName.c_str());
            if (cmp != 0) return cmp < 0;
            return a.familyName < b.familyName;
        });
    }

    // Fallback if Fontconfig yielded no fonts or when sysRoot is specified
    if (systemFonts.empty()) {
        fs::path base = sysRoot.empty() ? fs::path("/") : fs::path(sysRoot);
        for (const auto& grp : kFontGroups) {
            DiscoveredFont df;
            df.familyName = grp.familyName;
            df.available = false;
            df.path = "";
            for (const char* rel : grp.relativePaths) {
                fs::path p = base / rel;
                std::string normalized = p.lexically_normal().string();
                if (access(normalized.c_str(), R_OK) == 0) {
                    df.path = normalized;
                    df.available = true;
                    break;
                }
            }
            systemFonts.push_back(std::move(df));
        }
    }

    result.insert(result.end(), systemFonts.begin(), systemFonts.end());
    return result;
}
