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

void run_font_discovery_tests() {
    test_candidate_generation();
    test_cascade_priority();
    test_sandboxed_distro_filesystem_discovery();
    test_host_system_font_discovery();
}
