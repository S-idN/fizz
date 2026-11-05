#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include "services/FontDiscovery.h"

namespace fs = std::filesystem;

#define FONT_TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed: " #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while (0)

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

    // Must probe Cantarell on Fedora/Arch/Debian
    FONT_TEST_ASSERT(containsSubstr("Cantarell-Regular.otf") ||
                     containsSubstr("Cantarell-Regular.ttf"));

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

void test_font_resolution_fallback_and_predicate() {
    std::cout << "[TEST] Running test_font_resolution_fallback_and_predicate...\n";

    // Fallback when nonexistent sysroot
    std::string noneFound = resolveSystemFont("/nonexistent_sysroot_path_fizz_test");
    FONT_TEST_ASSERT(noneFound.empty());

    // Fallback when predicate returns false for everything
    std::vector<std::string> candidates = {"/fake/font1.ttf", "/fake/font2.ttf"};
    std::string fallback = resolveSystemFont("", candidates, [](const std::string&) { return false; });
    FONT_TEST_ASSERT(fallback.empty());

    // Resolves when candidate is accepted by predicate
    std::string resolved = resolveSystemFont("", candidates, [](const std::string& p) {
        return p == "/fake/font2.ttf";
    });
    FONT_TEST_ASSERT(resolved == "/fake/font2.ttf");

    std::cout << "[PASS] test_font_resolution_fallback_and_predicate passed.\n";
}

void test_cascade_priority() {
    std::cout << "[TEST] Running test_cascade_priority...\n";

    std::vector<std::string> activeFonts;
    auto mockChecker = [&](const std::string& path) {
        return std::any_of(activeFonts.begin(), activeFonts.end(), [&](const std::string& f) {
            return path.find(f) != std::string::npos;
        });
    };

    // 1. Adwaita Sans beats Cantarell, DejaVu, and JetBrains
    activeFonts = {"AdwaitaSans-Regular.ttf", "Cantarell-Regular.otf", "DejaVuSans.ttf", "JetBrainsMono-Regular.ttf"};
    std::string res1 = resolveSystemFont("", {}, mockChecker);
    FONT_TEST_ASSERT(res1.find("AdwaitaSans-Regular.ttf") != std::string::npos);

    // 2. Cantarell beats Google Sans and DejaVu
    activeFonts = {"Cantarell-Regular.otf", "GoogleSans-Regular.ttf", "DejaVuSans.ttf"};
    std::string res2 = resolveSystemFont("", {}, mockChecker);
    FONT_TEST_ASSERT(res2.find("Cantarell-Regular.otf") != std::string::npos);

    // 3. Google Sans beats DejaVu and JetBrains
    activeFonts = {"GoogleSans-Regular.ttf", "DejaVuSans.ttf", "JetBrainsMono-Regular.ttf"};
    std::string res3 = resolveSystemFont("", {}, mockChecker);
    FONT_TEST_ASSERT(res3.find("GoogleSans-Regular.ttf") != std::string::npos);

    // 4. DejaVu Sans beats JetBrains Mono
    activeFonts = {"DejaVuSans.ttf", "JetBrainsMono-Regular.ttf"};
    std::string res4 = resolveSystemFont("", {}, mockChecker);
    FONT_TEST_ASSERT(res4.find("DejaVuSans.ttf") != std::string::npos);

    // 5. JetBrains Mono selected when it is the only one present
    activeFonts = {"JetBrainsMono-Regular.ttf"};
    std::string res5 = resolveSystemFont("", {}, mockChecker);
    FONT_TEST_ASSERT(res5.find("JetBrainsMono-Regular.ttf") != std::string::npos);

    std::cout << "[PASS] test_cascade_priority passed.\n";
}

static void createDummyFile(const fs::path& p) {
    fs::create_directories(p.parent_path());
    std::ofstream out(p);
    out << "fake font data";
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

    // Remove Adwaita, should resolve Cantarell
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

    // 4. Simulated Arch layout with DejaVu Sans and JetBrains Mono
    fs::path archDejaVu = tempRoot / "usr/share/fonts/TTF/DejaVuSans.ttf";
    fs::path archJetBrains = tempRoot / "usr/share/fonts/TTF/JetBrainsMono-Regular.ttf";
    createDummyFile(archDejaVu);
    createDummyFile(archJetBrains);

    std::string archFound = resolveSystemFont(tempRoot.string());
    FONT_TEST_ASSERT(archFound == archDejaVu.lexically_normal().string());

    // Remove DejaVu -> resolves JetBrains Mono
    fs::remove(archDejaVu, ec);
    std::string archFound2 = resolveSystemFont(tempRoot.string());
    FONT_TEST_ASSERT(archFound2 == archJetBrains.lexically_normal().string());

    fs::remove_all(tempRoot, ec);
    std::cout << "[PASS] test_sandboxed_distro_filesystem_discovery passed.\n";
}

void test_host_system_font_discovery() {
    std::cout << "[TEST] Running test_host_system_font_discovery...\n";

    // On Linux systems with desktop fonts installed (such as this Fedora host),
    // resolveSystemFont() must return an existing readable font.
    std::string hostFont = resolveSystemFont();
    if (fs::exists("/usr/share/fonts")) {
        FONT_TEST_ASSERT(!hostFont.empty());
        FONT_TEST_ASSERT(fs::exists(hostFont));
        std::cout << "  Found host font: " << hostFont << "\n";
    }

    std::cout << "[PASS] test_host_system_font_discovery passed.\n";
}

void run_font_discovery_tests() {
    test_candidate_generation();
    test_font_resolution_fallback_and_predicate();
    test_cascade_priority();
    test_sandboxed_distro_filesystem_discovery();
    test_host_system_font_discovery();
}
