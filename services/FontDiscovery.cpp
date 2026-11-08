#include "FontDiscovery.h"
#include <filesystem>
#include <unistd.h>

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
    result.reserve(1 + sizeof(kFontGroups) / sizeof(kFontGroups[0]));

    // 1. System Default option
    result.push_back({"System Default (Auto-detect)", "", true});

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
        result.push_back(std::move(df));
    }

    return result;
}
