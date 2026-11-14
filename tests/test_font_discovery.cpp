#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <unordered_set>
#include <cstring>
#include <strings.h>
#include <unistd.h>
#include "services/FontDiscovery.h"

namespace fs = std::filesystem;

#define FONT_TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed: " #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while (0)

static void createDummyFile(const fs::path& p) {
    fs::create_directories(p.parent_path());
    std::ofstream out(p);
    out << "fake font data";
}

void test_candidate_generation() {
    std::cout << "[TEST] Running test_candidate_generation...\n";

    auto candidates = getDefaultFontCandidates();
    FONT_TEST_ASSERT(!candidates.empty());

    auto containsSubstr = [&](const std::string& substr) {
        return std::any_of(candidates.begin(), candidates.end(), [&](const std::string& p) {
            return p.find(substr) != std::string::npos;
        });
    };

    // Must probe Adwaita Sans on Fedora/Arch/Debian
    FONT_TEST_ASSERT(containsSubstr("adwaita-sans-fonts/AdwaitaSans-Regular.ttf") ||
                     containsSubstr("AdwaitaSans-Regular.ttf"));

    // Must probe Cantarell on Fedora/Arch/Debian including Arch variable font (Cantarell-VF.otf)
    FONT_TEST_ASSERT(containsSubstr("Cantarell-Regular.otf") ||
                     containsSubstr("Cantarell-Regular.ttf"));
    FONT_TEST_ASSERT(containsSubstr("Cantarell-VF.otf"));

    // Must probe Google Sans
    FONT_TEST_ASSERT(containsSubstr("GoogleSans-Regular.ttf"));

    // Must probe DejaVu Sans
    FONT_TEST_ASSERT(containsSubstr("DejaVuSans.ttf"));

    // Must probe JetBrains Mono
    FONT_TEST_ASSERT(containsSubstr("JetBrainsMono-Regular.ttf"));

    // Test sysRoot prefixing
    std::string mockRoot = "/custom/sysroot";
    auto prefixed = getDefaultFontCandidates(mockRoot);
    FONT_TEST_ASSERT(!prefixed.empty());
    for (const auto& path : prefixed) {
        FONT_TEST_ASSERT(path.rfind(mockRoot, 0) == 0);
    }

    std::cout << "[PASS] test_candidate_generation passed.\n";
}

void test_cascade_priority() {
    std::cout << "[TEST] Running test_cascade_priority...\n";

    fs::path tempRoot = fs::temp_directory_path() / "fizz_test_font_priority";
    std::error_code ec;
    fs::remove_all(tempRoot, ec);
    fs::create_directories(tempRoot, ec);

    fs::path adwaita = tempRoot / "usr/share/fonts/adwaita-sans-fonts/AdwaitaSans-Regular.ttf";
    fs::path cantarell = tempRoot / "usr/share/fonts/abattis-cantarell-fonts/Cantarell-Regular.otf";
    fs::path googleSans = tempRoot / "usr/share/fonts/google-sans/GoogleSans-Regular.ttf";
    fs::path dejavu = tempRoot / "usr/share/fonts/TTF/DejaVuSans.ttf";
    fs::path jetbrains = tempRoot / "usr/share/fonts/TTF/JetBrainsMono-Regular.ttf";

    createDummyFile(adwaita);
    createDummyFile(cantarell);
    createDummyFile(googleSans);
    createDummyFile(dejavu);
    createDummyFile(jetbrains);

    // 1. Adwaita Sans beats all others
    std::string res1 = resolveSystemFont(tempRoot.string());
    FONT_TEST_ASSERT(res1 == adwaita.lexically_normal().string());

    // 2. Cantarell beats Google Sans, DejaVu, JetBrains
    fs::remove(adwaita, ec);
    std::string res2 = resolveSystemFont(tempRoot.string());
    FONT_TEST_ASSERT(res2 == cantarell.lexically_normal().string());

    // 3. Google Sans beats DejaVu and JetBrains
    fs::remove(cantarell, ec);
    std::string res3 = resolveSystemFont(tempRoot.string());
    FONT_TEST_ASSERT(res3 == googleSans.lexically_normal().string());

    // 4. DejaVu Sans beats JetBrains Mono
    fs::remove(googleSans, ec);
    std::string res4 = resolveSystemFont(tempRoot.string());
    FONT_TEST_ASSERT(res4 == dejavu.lexically_normal().string());

    // 5. JetBrains Mono selected when it is the only one present
    fs::remove(dejavu, ec);
    std::string res5 = resolveSystemFont(tempRoot.string());
    FONT_TEST_ASSERT(res5 == jetbrains.lexically_normal().string());

    fs::remove_all(tempRoot, ec);
    std::cout << "[PASS] test_cascade_priority passed.\n";
}

void test_sandboxed_distro_filesystem_discovery() {
    std::cout << "[TEST] Running test_sandboxed_distro_filesystem_discovery...\n";

    fs::path tempRoot = fs::temp_directory_path() / "fizz_test_fonts";
    std::error_code ec;
    fs::remove_all(tempRoot, ec);
    fs::create_directories(tempRoot, ec);

    // 1. Missing fonts -> empty string
    std::string noneFound = resolveSystemFont(tempRoot.string());
    FONT_TEST_ASSERT(noneFound.empty());

    // 2. Simulated Fedora layout with Adwaita Sans and Cantarell
    fs::path fedoraAdwaita = tempRoot / "usr/share/fonts/adwaita-sans-fonts/AdwaitaSans-Regular.ttf";
    fs::path fedoraCantarell = tempRoot / "usr/share/fonts/abattis-cantarell-fonts/Cantarell-Regular.otf";
    createDummyFile(fedoraAdwaita);
    createDummyFile(fedoraCantarell);

    std::string fedoraFound = resolveSystemFont(tempRoot.string());
    FONT_TEST_ASSERT(fedoraFound == fedoraAdwaita.lexically_normal().string());

    // Remove Adwaita -> resolves Cantarell
    fs::remove(fedoraAdwaita, ec);
    std::string fedoraFound2 = resolveSystemFont(tempRoot.string());
    FONT_TEST_ASSERT(fedoraFound2 == fedoraCantarell.lexically_normal().string());

    fs::remove_all(tempRoot, ec);
    fs::create_directories(tempRoot, ec);

    // 3. Simulated Debian/Ubuntu layout with Cantarell and DejaVu
    fs::path debianCantarell = tempRoot / "usr/share/fonts/opentype/cantarell/Cantarell-Regular.otf";
    fs::path debianDejaVu = tempRoot / "usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
    createDummyFile(debianCantarell);
    createDummyFile(debianDejaVu);

    std::string debianFound = resolveSystemFont(tempRoot.string());
    FONT_TEST_ASSERT(debianFound == debianCantarell.lexically_normal().string());

    fs::remove_all(tempRoot, ec);
    fs::create_directories(tempRoot, ec);

    // 4. Simulated Arch layout with Cantarell VF and JetBrains Mono
    fs::path archCantarellVF = tempRoot / "usr/share/fonts/cantarell/Cantarell-VF.otf";
    fs::path archJetBrains = tempRoot / "usr/share/fonts/TTF/JetBrainsMono-Regular.ttf";
    createDummyFile(archCantarellVF);
    createDummyFile(archJetBrains);

    std::string archFound = resolveSystemFont(tempRoot.string());
    FONT_TEST_ASSERT(archFound == archCantarellVF.lexically_normal().string());

    // Remove Cantarell VF -> resolves JetBrains Mono
    fs::remove(archCantarellVF, ec);
    std::string archFound2 = resolveSystemFont(tempRoot.string());
    FONT_TEST_ASSERT(archFound2 == archJetBrains.lexically_normal().string());

    fs::remove_all(tempRoot, ec);
    std::cout << "[PASS] test_sandboxed_distro_filesystem_discovery passed.\n";
}

void test_host_system_font_discovery() {
    std::cout << "[TEST] Running test_host_system_font_discovery...\n";

    std::string hostFont = resolveSystemFont();
    auto candidates = getDefaultFontCandidates();

    bool hasHostCandidate = false;
    for (const auto& c : candidates) {
        if (fs::exists(c)) {
            hasHostCandidate = true;
            break;
        }
    }

    if (hasHostCandidate) {
        FONT_TEST_ASSERT(!hostFont.empty());
        FONT_TEST_ASSERT(fs::exists(hostFont));
        std::cout << "  Found host font: " << hostFont << "\n";
    } else {
        FONT_TEST_ASSERT(hostFont.empty());
        std::cout << "  No candidate font on host; graceful empty fallback verified.\n";
    }

    std::cout << "[PASS] test_host_system_font_discovery passed.\n";
}

void test_discovered_system_fonts_families() {
    std::cout << "[TEST] Running test_discovered_system_fonts_families...\n";

    fs::path tempRoot = fs::temp_directory_path() / "fizz_test_font_families";
    std::error_code ec;
    fs::remove_all(tempRoot, ec);
    fs::create_directories(tempRoot, ec);

    // Initial check: empty directory
    auto fontsEmpty = getDiscoveredSystemFonts(tempRoot.string());
    FONT_TEST_ASSERT(fontsEmpty.size() >= 6);
    FONT_TEST_ASSERT(fontsEmpty[0].familyName.find("Default") != std::string::npos);
    FONT_TEST_ASSERT(fontsEmpty[0].available);
    for (size_t i = 1; i < fontsEmpty.size(); ++i) {
        FONT_TEST_ASSERT(!fontsEmpty[i].available);
        FONT_TEST_ASSERT(fontsEmpty[i].path.empty());
    }

    // Populate Cantarell and JetBrains Mono
    fs::path cantarellPath = tempRoot / "usr/share/fonts/cantarell/Cantarell-Regular.otf";
    fs::path jbPath = tempRoot / "usr/share/fonts/jetbrains-mono-fonts/JetBrainsMono-Regular.ttf";
    createDummyFile(cantarellPath);
    createDummyFile(jbPath);

    auto fontsPopulated = getDiscoveredSystemFonts(tempRoot.string());
    bool foundCantarell = false;
    bool foundJb = false;
    bool adwaitaAvailable = false;

    for (const auto& f : fontsPopulated) {
        if (f.familyName == "Cantarell") {
            FONT_TEST_ASSERT(f.available);
            FONT_TEST_ASSERT(f.path == cantarellPath.lexically_normal().string());
            foundCantarell = true;
        } else if (f.familyName == "JetBrains Mono") {
            FONT_TEST_ASSERT(f.available);
            FONT_TEST_ASSERT(f.path == jbPath.lexically_normal().string());
            foundJb = true;
        } else if (f.familyName == "Adwaita Sans") {
            adwaitaAvailable = f.available;
        }
    }

    FONT_TEST_ASSERT(foundCantarell);
    FONT_TEST_ASSERT(foundJb);
    FONT_TEST_ASSERT(!adwaitaAvailable);

    fs::remove_all(tempRoot, ec);
    std::cout << "[PASS] test_discovered_system_fonts_families passed.\n";
}

void test_bundled_space_grotesk_default_resolution() {
    std::cout << "[TEST] Running test_bundled_space_grotesk_default_resolution...\n";

    // 1. resolveAssetFile directly
    std::string assetPath = resolveAssetFile("assets/fonts/SpaceGrotesk-VariableFont_wght.ttf");
    FONT_TEST_ASSERT(!assetPath.empty());
    FONT_TEST_ASSERT(fs::exists(assetPath));
    FONT_TEST_ASSERT(access(assetPath.c_str(), R_OK) == 0);
    FONT_TEST_ASSERT(assetPath.find("SpaceGrotesk-VariableFont_wght.ttf") != std::string::npos);

    // 2. getDiscoveredSystemFonts() has Space Grotesk (Bundled Default) at index 0
    auto fonts = getDiscoveredSystemFonts();
    FONT_TEST_ASSERT(!fonts.empty());
    FONT_TEST_ASSERT(fonts[0].familyName == "Space Grotesk (Bundled Default)");
    FONT_TEST_ASSERT(fonts[0].available);
    FONT_TEST_ASSERT(!fonts[0].path.empty());
    FONT_TEST_ASSERT(access(fonts[0].path.c_str(), R_OK) == 0);
    FONT_TEST_ASSERT(fonts[0].path.find("SpaceGrotesk-VariableFont_wght.ttf") != std::string::npos);

    // 3. Fallback when asset file does not exist
    std::string missing = resolveAssetFile("non_existent_asset_file.ttf");
    FONT_TEST_ASSERT(missing.empty());

    // 4. Custom root resolution
    fs::path tempRoot = fs::temp_directory_path() / "fizz_test_asset_resolution";
    std::error_code ec;
    fs::remove_all(tempRoot, ec);
    fs::path mockFont = tempRoot / "assets/fonts/SpaceGrotesk-VariableFont_wght.ttf";
    createDummyFile(mockFont);

    std::string customAsset = resolveAssetFile("assets/fonts/SpaceGrotesk-VariableFont_wght.ttf", tempRoot.string());
    FONT_TEST_ASSERT(!customAsset.empty());
    FONT_TEST_ASSERT(customAsset == mockFont.lexically_normal().string());

    auto customFonts = getDiscoveredSystemFonts(tempRoot.string());
    FONT_TEST_ASSERT(!customFonts.empty());
    FONT_TEST_ASSERT(customFonts[0].familyName == "Space Grotesk (Bundled Default)");
    FONT_TEST_ASSERT(customFonts[0].available);
    FONT_TEST_ASSERT(customFonts[0].path == mockFont.lexically_normal().string());

    fs::remove_all(tempRoot, ec);
    std::cout << "[PASS] test_bundled_space_grotesk_default_resolution passed.\n";
}

void test_fontconfig_enumeration_and_scalable_filtering() {
    std::cout << "[TEST] Running test_fontconfig_enumeration_and_scalable_filtering...\n";

    auto fonts = getDiscoveredSystemFonts();
    FONT_TEST_ASSERT(!fonts.empty());
    FONT_TEST_ASSERT(fonts[0].familyName == "Space Grotesk (Bundled Default)");

    // System fonts enumerated from Fontconfig (on Linux host with fonts installed)
    FONT_TEST_ASSERT(fonts.size() > 1);

    for (size_t i = 1; i < fonts.size(); ++i) {
        const auto& f = fonts[i];
        FONT_TEST_ASSERT(!f.familyName.empty());
        FONT_TEST_ASSERT(!f.path.empty());
        FONT_TEST_ASSERT(f.available);
        FONT_TEST_ASSERT(access(f.path.c_str(), R_OK) == 0);
        // scalable check: verify file extension is typically ttf, otf, or ttc
        std::string ext = fs::path(f.path).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
            return std::tolower(c);
        });
        FONT_TEST_ASSERT(ext == ".ttf" || ext == ".otf" || ext == ".ttc" || ext == ".woff" || ext == ".woff2");
    }

    std::cout << "[PASS] test_fontconfig_enumeration_and_scalable_filtering passed.\n";
}

void test_fontconfig_deduplication() {
    std::cout << "[TEST] Running test_fontconfig_deduplication...\n";

    auto fonts = getDiscoveredSystemFonts();
    FONT_TEST_ASSERT(fonts.size() > 1);

    std::unordered_set<std::string> seenFamilies;
    for (size_t i = 1; i < fonts.size(); ++i) {
        std::string lowerName = fonts[i].familyName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](unsigned char c) {
            return std::tolower(c);
        });

        bool inserted = seenFamilies.insert(lowerName).second;
        if (!inserted) {
            std::cerr << "Duplicate font family found: " << fonts[i].familyName << "\n";
        }
        FONT_TEST_ASSERT(inserted);
    }

    std::cout << "[PASS] test_fontconfig_deduplication passed.\n";
}

void test_fontconfig_alphabetical_sorting() {
    std::cout << "[TEST] Running test_fontconfig_alphabetical_sorting...\n";

    auto fonts = getDiscoveredSystemFonts();
    FONT_TEST_ASSERT(fonts.size() > 1);

    // Index 0 is always Space Grotesk (Bundled Default)
    FONT_TEST_ASSERT(fonts[0].familyName == "Space Grotesk (Bundled Default)");

    // System fonts (index 1 to N-1) must be sorted case-insensitively
    for (size_t i = 1; i + 1 < fonts.size(); ++i) {
        int cmp = strcasecmp(fonts[i].familyName.c_str(), fonts[i + 1].familyName.c_str());
        if (cmp > 0) {
            std::cerr << "Sorting violation: '" << fonts[i].familyName << "' came before '" << fonts[i + 1].familyName << "'\n";
        }
        FONT_TEST_ASSERT(cmp <= 0);
    }

    std::cout << "[PASS] test_fontconfig_alphabetical_sorting passed.\n";
}

void run_font_discovery_tests() {
    test_candidate_generation();
    test_cascade_priority();
    test_sandboxed_distro_filesystem_discovery();
    test_host_system_font_discovery();
    test_bundled_space_grotesk_default_resolution();
    test_fontconfig_enumeration_and_scalable_filtering();
    test_fontconfig_deduplication();
    test_fontconfig_alphabetical_sorting();
    test_discovered_system_fonts_families();
}
