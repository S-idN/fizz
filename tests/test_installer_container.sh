#!/usr/bin/env bash
# test_installer_container.sh
# ---------------------------------------------------------------------------
# Containerized end-to-end test harness for Fizz install / service / uninstall
# lifecycle.  Runs the full lifecycle in a fresh, ephemeral container so that
# the host system is never touched.
#
# Requirements: podman (preferred) or docker.
# Usage:
#   bash tests/test_installer_container.sh
#   bash tests/test_installer_container.sh --engine docker
#   bash tests/test_installer_container.sh --image fedora:40
# ---------------------------------------------------------------------------
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

ENGINE=""
IMAGE="fedora:40"

usage() {
    echo "Usage: $0 [--engine podman|docker] [--image IMAGE]"
    exit 1
}

while [ $# -gt 0 ]; do
    case "$1" in
        --engine) ENGINE="$2"; shift 2 ;;
        --image)  IMAGE="$2";  shift 2 ;;
        -h|--help) usage ;;
        *) echo "Unknown option: $1" >&2; usage ;;
    esac
done

# Auto-detect container engine
if [ -z "$ENGINE" ]; then
    if   command -v podman >/dev/null 2>&1; then ENGINE="podman"
    elif command -v docker >/dev/null 2>&1; then ENGINE="docker"
    else
        echo "ERROR: neither podman nor docker found in PATH." >&2
        exit 1
    fi
fi

echo "=== Fizz Containerized Installer Test ==="
echo "Engine : $ENGINE"
echo "Image  : $IMAGE"
echo "Repo   : $REPO_ROOT"
echo ""

# ---------------------------------------------------------------------------
# Inline test script executed inside the container
# ---------------------------------------------------------------------------
read -r -d '' INNER_SCRIPT <<'INNER' || true
#!/usr/bin/env bash
set -euo pipefail

REPO=/fizz
PASS=0; FAIL=0

ok()   { echo "  [PASS] $1"; PASS=$((PASS+1)); }
fail() { echo "  [FAIL] $1: $2" >&2; FAIL=$((FAIL+1)); }

# ---------------------------------------------------------------------------
# 1. Install build dependencies (distro-specific)
# ---------------------------------------------------------------------------
echo "--- Step 1: Install build deps ---"
if   command -v dnf     >/dev/null; then
    dnf install -y gcc-c++ make cmake glfw-devel mesa-libGL-devel pkgconf-pkg-config dbus-devel python3 >/dev/null 2>&1
elif command -v apt-get >/dev/null; then
    apt-get update -qq && apt-get install -y build-essential cmake libglfw3-dev libgl1-mesa-dev pkg-config python3 >/dev/null 2>&1
elif command -v pacman  >/dev/null; then
    pacman -Sy --noconfirm base-devel cmake glfw-x11 pkgconf python >/dev/null 2>&1
elif command -v zypper  >/dev/null; then
    zypper install -y gcc-c++ make cmake glfw3-devel Mesa-libGL-devel pkgconf-pkg-config python3 >/dev/null 2>&1
else
    echo "ERROR: Unknown distro — cannot install dependencies." >&2; exit 1
fi
ok "Build dependencies installed"

# ---------------------------------------------------------------------------
# 2. Full installation (no hotkey in headless container)
# ---------------------------------------------------------------------------
echo "--- Step 2: Install ---"
bash "$REPO/install.sh" --no-hotkey
[ -x "$HOME/.local/bin/fizz-search" ] && ok "fizz-search binary installed" \
    || { fail "fizz-search binary" "not found or not executable"; }
[ -x "$HOME/.local/bin/fizzd" ] && ok "fizzd binary installed" \
    || { fail "fizzd binary" "not found or not executable"; }

# Service: containers may have no running systemd user bus; either the
# systemd unit file or the XDG autostart entry must exist.
HAS_SERVICE=0
[ -f "$HOME/.config/systemd/user/fizzd.service" ] && HAS_SERVICE=1
[ -f "$HOME/.config/autostart/fizzd.desktop" ]    && HAS_SERVICE=1
[ "$HAS_SERVICE" -eq 1 ] && ok "fizzd service or autostart entry installed" \
    || { fail "fizzd service" "neither systemd unit nor autostart desktop file found"; }

# ---------------------------------------------------------------------------
# 3. Idempotent re-installation
# ---------------------------------------------------------------------------
echo "--- Step 3: Idempotent re-install ---"
bash "$REPO/install.sh" --no-hotkey
[ -x "$HOME/.local/bin/fizz-search" ] && ok "Re-install leaves fizz-search present" \
    || { fail "Re-install" "fizz-search missing after second install"; }

# ---------------------------------------------------------------------------
# 4. Write user config & cache, then standard uninstall (--no-hotkey)
# ---------------------------------------------------------------------------
echo "--- Step 4: Standard uninstall (--no-hotkey) ---"
mkdir -p "$HOME/.config/fizz"
echo "auto_close_on_unfocus=false" > "$HOME/.config/fizz/fizz.conf"
echo "mock_cache"                  > "$HOME/.fizzLocalCache.bin"

bash "$REPO/uninstall.sh" --no-hotkey
[ ! -f "$HOME/.local/bin/fizz-search" ] && ok "fizz-search removed by uninstall" \
    || { fail "fizz-search" "still present after uninstall"; }
[ ! -f "$HOME/.local/bin/fizzd" ] && ok "fizzd removed by uninstall" \
    || { fail "fizzd" "still present after uninstall"; }
[ -f "$HOME/.config/fizz/fizz.conf" ] && ok "User config preserved by standard uninstall" \
    || { fail "User config" "removed by standard uninstall (should be preserved)"; }
[ -f "$HOME/.fizzLocalCache.bin" ] && ok "Binary cache preserved by standard uninstall" \
    || { fail "Binary cache" "removed by standard uninstall (should be preserved)"; }

# ---------------------------------------------------------------------------
# 5. Re-install, then purge
# ---------------------------------------------------------------------------
echo "--- Step 5: Re-install + purge ---"
bash "$REPO/install.sh" --no-hotkey
bash "$REPO/uninstall.sh" --purge --no-hotkey
[ ! -f "$HOME/.local/bin/fizz-search" ] && ok "fizz-search removed by purge" \
    || { fail "fizz-search" "still present after purge"; }
[ ! -d "$HOME/.config/fizz" ] && ok "Config dir purged" \
    || { fail "Config dir" "still present after purge"; }
[ ! -f "$HOME/.fizzLocalCache.bin" ] && ok "Binary cache purged" \
    || { fail "Binary cache" "still present after purge"; }

# ---------------------------------------------------------------------------
# 6. Zero-footprint assertion on the sandbox HOME
# ---------------------------------------------------------------------------
echo "--- Step 6: Zero-footprint check ---"
BAD_FILES=()
[ -f "$HOME/.local/bin/fizz-search" ]           && BAD_FILES+=("$HOME/.local/bin/fizz-search")
[ -f "$HOME/.local/bin/fizzd" ]                 && BAD_FILES+=("$HOME/.local/bin/fizzd")
[ -f "$HOME/.config/systemd/user/fizzd.service" ] && BAD_FILES+=("$HOME/.config/systemd/user/fizzd.service")
[ -d "$HOME/.config/fizz" ]                     && BAD_FILES+=("$HOME/.config/fizz")
[ -f "$HOME/.fizzLocalCache.bin" ]              && BAD_FILES+=("$HOME/.fizzLocalCache.bin")

if [ "${#BAD_FILES[@]}" -eq 0 ]; then
    ok "Zero footprint: no fizz files left after purge"
else
    fail "Zero footprint" "found leftover files: ${BAD_FILES[*]}"
fi

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
echo ""
echo "=== Container Test Summary: $PASS passed, $FAIL failed ==="
[ "$FAIL" -eq 0 ] || exit 1
INNER

# ---------------------------------------------------------------------------
# Run the inner script inside a fresh container
# ---------------------------------------------------------------------------
echo "Pulling image $IMAGE ..."
"$ENGINE" pull "$IMAGE" >/dev/null 2>&1 || true

echo "Launching container ..."
"$ENGINE" run --rm \
    --volume "$REPO_ROOT":/fizz:ro \
    --env HOME=/root \
    --workdir /root \
    "$IMAGE" \
    bash -c "$INNER_SCRIPT"

echo ""
echo "=== Containerized installer test PASSED ==="
