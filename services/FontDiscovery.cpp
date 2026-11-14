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

std::string resolveAssetFile(const std::string& relativePath, const std::string& customRoot) {
    if (!customRoot.empty()) {
        fs::path p = fs::path(customRoot) / relativePath;
        if (access(p.c_str(), R_OK) == 0) {
            return p.lexically_normal().string();
        }
    }
    if (access(relativePath.c_str(), R_OK) == 0) {
        return fs::path(relativePath).lexically_normal().string();
    }
    std::string parentRelative = "../" + relativePath;
    if (access(parentRelative.c_str(), R_OK) == 0) {
        return fs::path(parentRelative).lexically_normal().string();
    }
    char exePath[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len > 0) {
        exePath[len] = '\0';
        fs::path dir = fs::path(exePath).parent_path();
        fs::path candidate = dir / relativePath;
        if (access(candidate.c_str(), R_OK) == 0) {
            return candidate.lexically_normal().string();
        }
        candidate = dir.parent_path() / relativePath;
        if (access(candidate.c_str(), R_OK) == 0) {
            return candidate.lexically_normal().string();
        }
    }
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
