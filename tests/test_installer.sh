#!/usr/bin/env bash
set -euo pipefail

# test_installer.sh - Sandboxed test suite for install.sh and uninstall.sh

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
INSTALL_SH="$REPO_ROOT/install.sh"
UNINSTALL_SH="$REPO_ROOT/uninstall.sh"
GNOME_HELPER="$REPO_ROOT/scripts/gnome_shortcut.py"

FAILED=0

pass() {
    echo "  [PASS] $1"
}

fail() {
    echo "  [FAIL] $1: $2"
    FAILED=$((FAILED + 1))
}

assert_eq() {
    local expected="$1"
    local actual="$2"
    local msg="${3:-Assertion failed}"
    if [ "$expected" != "$actual" ]; then
        fail "$msg" "expected '$expected', got '$actual'"
    else
        pass "$msg"
    fi
}

assert_contains() {
    local needle="$1"
    local haystack="$2"
    local msg="${3:-Assertion failed}"
    if [[ "$haystack" != *"$needle"* ]]; then
        fail "$msg" "expected to contain '$needle', got '$haystack'"
    else
        pass "$msg"
    fi
}

assert_not_contains() {
    local needle="$1"
    local haystack="$2"
    local msg="${3:-Assertion failed}"
    if [[ "$haystack" == *"$needle"* ]]; then
        fail "$msg" "expected NOT to contain '$needle', found in '$haystack'"
    else
        pass "$msg"
    fi
}

echo "=== Running Fizz Installer Tests ==="

# -----------------------------------------------------------------------------
# Seam 1: Dependency Verification Tests
# -----------------------------------------------------------------------------
echo "--- Seam 1: Dependency Verification ---"

test_deps_package_commands() {
    # Test Fedora
    local out
    out=$(bash -c "
        source '$INSTALL_SH'
        command() {
            if [ \"\$1\" = \"-v\" ]; then
                [ \"\$2\" = \"dnf\" ] && return 0 || return 1
            fi
            builtin command \"\$@\"
        }
        deps_command
    ")
    assert_contains "gcc-c++ make cmake glfw-devel mesa-libGL-devel pkgconf-pkg-config" "$out" "Fedora packages include pkgconf-pkg-config and required dev packages"

    # Test openSUSE
    out=$(bash -c "
        source '$INSTALL_SH'
        command() {
            if [ \"\$1\" = \"-v\" ]; then
                [ \"\$2\" = \"zypper\" ] && return 0 || return 1
            fi
            builtin command \"\$@\"
        }
        deps_command
    ")
    assert_contains "gcc-c++ make cmake glfw3-devel Mesa-libGL-devel pkgconf-pkg-config" "$out" "openSUSE packages include pkgconf-pkg-config and required dev packages"
}

test_deps_missing_detection() {
    # 1. Missing build tool (make or ninja)
    local ret
    ret=$(bash -c "
        source '$INSTALL_SH'
        command() {
            if [ \"\$1\" = \"-v\" ]; then
                case \"\$2\" in
                    cmake|c++) return 0 ;;
                    make|ninja) return 1 ;;
                    pkg-config) return 0 ;;
                    *) return 1 ;;
                esac
            fi
            builtin command \"\$@\"
        }
        has_cpp17() { return 0; }
        has_glfw() { return 0; }
        has_opengl() { return 0; }
        if deps_missing; then echo 'missing'; else echo 'ok'; fi
    ")
    assert_eq "missing" "$ret" "deps_missing detects missing make/ninja build engine"

    # 2. Missing OpenGL headers
    ret=$(bash -c "
        source '$INSTALL_SH'
        command() {
            if [ \"\$1\" = \"-v\" ]; then
                case \"\$2\" in
                    cmake|c++|make) return 0 ;;
                    pkg-config) return 0 ;;
                    *) return 1 ;;
                esac
            fi
            builtin command \"\$@\"
        }
        has_cpp17() { return 0; }
        has_glfw() { return 0; }
        has_opengl() { return 1; }
        if deps_missing; then echo 'missing'; else echo 'ok'; fi
    ")
    assert_eq "missing" "$ret" "deps_missing detects missing OpenGL headers"

    # 3. Missing C++17 compiler
    ret=$(bash -c "
        source '$INSTALL_SH'
        command() {
            if [ \"\$1\" = \"-v\" ]; then
                case \"\$2\" in
                    cmake|make) return 0 ;;
                    pkg-config) return 0 ;;
                    *) return 1 ;;
                esac
            fi
            builtin command \"\$@\"
        }
        has_cpp17() { return 1; }
        has_glfw() { return 0; }
        has_opengl() { return 0; }
        if deps_missing; then echo 'missing'; else echo 'ok'; fi
    ")
    assert_eq "missing" "$ret" "deps_missing detects missing C++17 compiler"

    # 4. Missing GLFW
    ret=$(bash -c "
        source '$INSTALL_SH'
        command() {
            if [ \"\$1\" = \"-v\" ]; then
                case \"\$2\" in
                    cmake|make|c++) return 0 ;;
                    pkg-config) return 0 ;;
                    *) return 1 ;;
                esac
            fi
            builtin command \"\$@\"
        }
        has_cpp17() { return 0; }
        has_glfw() { return 1; }
        has_opengl() { return 0; }
        if deps_missing; then echo 'missing'; else echo 'ok'; fi
    ")
    assert_eq "missing" "$ret" "deps_missing detects missing GLFW"

    # 5. All present
    ret=$(bash -c "
        source '$INSTALL_SH'
        command() {
            if [ \"\$1\" = \"-v\" ]; then
                case \"\$2\" in
                    cmake|make|c++) return 0 ;;
                    pkg-config) return 0 ;;
                    *) return 1 ;;
                esac
            fi
            builtin command \"\$@\"
        }
        has_cpp17() { return 0; }
        has_glfw() { return 0; }
        has_opengl() { return 0; }
        if deps_missing; then echo 'missing'; else echo 'ok'; fi
    ")
    assert_eq "ok" "$ret" "deps_missing returns ok when all dependencies are present"
}

test_deps_missing_non_interactive_exit() {
    local out
    local status=0
    out=$(bash -c "
        source '$INSTALL_SH'
        deps_missing() { return 0; }
        deps_command() { echo 'sudo dnf install dummy'; }
        main
    " </dev/null 2>&1) || status=$?
    assert_eq "1" "$status" "Non-interactive run with missing deps exits with 1"
    assert_contains "Missing build dependencies" "$out" "Non-interactive run prints missing dependencies notice"
}

test_deps_package_commands
test_deps_missing_detection
test_deps_missing_non_interactive_exit

# -----------------------------------------------------------------------------
# Seam 2: GNOME Shortcut Registration Tests
# -----------------------------------------------------------------------------
echo "--- Seam 2: GNOME Shortcut Helper ---"

test_gnome_helper_gvariant_parsing() {
    local out
    # 1. Append to empty list
    out=$(python3 "$GNOME_HELPER" append "@as []")
    assert_eq "['/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/fizz/']" "$out" "GVariant append to @as []"

    # 2. Append to existing custom0
    out=$(python3 "$GNOME_HELPER" append "['/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/custom0/']")
    assert_eq "['/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/custom0/', '/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/fizz/']" "$out" "GVariant append preserves existing custom0"

    # 3. Append when fizz already present (idempotent)
    out=$(python3 "$GNOME_HELPER" append "['/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/custom0/', '/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/fizz/']")
    assert_eq "['/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/custom0/', '/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/fizz/']" "$out" "GVariant append does not duplicate fizz"

    # 4. Remove fizz when other bindings exist
    out=$(python3 "$GNOME_HELPER" remove "['/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/custom0/', '/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/fizz/']")
    assert_eq "['/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/custom0/']" "$out" "GVariant remove removes fizz and preserves custom0"

    # 5. Remove fizz when only fizz exists -> returns @as []
    out=$(python3 "$GNOME_HELPER" remove "['/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/fizz/']")
    assert_eq "@as []" "$out" "GVariant remove returns @as [] when empty"
}

test_gnome_helper_mock_gsettings() {
    local tmp_dir
    tmp_dir=$(mktemp -d)
    local state_file="$tmp_dir/gsettings_state"
    local mock_gsettings="$tmp_dir/gsettings"

    # Create mock gsettings script
    cat > "$mock_gsettings" <<'EOF'
#!/usr/bin/env bash
STATE_FILE="$MOCK_STATE_FILE"
mkdir -p "$(dirname "$STATE_FILE")"
touch "$STATE_FILE"

cmd="$1"; shift
if [ "$cmd" = "get" ]; then
    key="$1.$2"
    if [ $# -eq 1 ]; then key="$1"; fi
    val=$(grep "^$key=" "$STATE_FILE" 2>/dev/null | cut -d'=' -f2-)
    if [ -n "$val" ]; then
        echo "$val"
    else
        case "$key" in
            *custom-keybindings) echo "@as []" ;;
            *) echo "''" ;;
        esac
    fi
elif [ "$cmd" = "set" ]; then
    key="$1.$2"; val="$3"
    if [ $# -eq 2 ]; then key="$1"; val="$2"; fi
    grep -v "^$key=" "$STATE_FILE" > "${STATE_FILE}.tmp" 2>/dev/null || true
    echo "$key=$val" >> "${STATE_FILE}.tmp"
    mv "${STATE_FILE}.tmp" "$STATE_FILE"
elif [ "$cmd" = "reset" ]; then
    key="$1.$2"
    if [ $# -eq 1 ]; then key="$1"; fi
    grep -v "^$key=" "$STATE_FILE" > "${STATE_FILE}.tmp" 2>/dev/null || true
    mv "${STATE_FILE}.tmp" "$STATE_FILE"
fi
EOF
    chmod +x "$mock_gsettings"

    # Test fresh installation
    MOCK_STATE_FILE="$state_file" python3 "$GNOME_HELPER" register --gsettings-bin "$mock_gsettings" --command "/home/user/.local/bin/fizz-search"
    local list_val binding_val
    list_val=$(MOCK_STATE_FILE="$state_file" "$mock_gsettings" get org.gnome.settings-daemon.plugins.media-keys custom-keybindings)
    assert_contains "/custom-keybindings/fizz/" "$list_val" "GNOME helper registers /fizz/ in custom-keybindings"
    binding_val=$(MOCK_STATE_FILE="$state_file" "$mock_gsettings" get org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/fizz/ binding)
    assert_eq "'<Control><Super>space'" "$binding_val" "GNOME helper assigns default binding <Control><Super>space"

    # Test update preserves custom binding
    MOCK_STATE_FILE="$state_file" "$mock_gsettings" set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/fizz/ binding "'<Super>space'"
    MOCK_STATE_FILE="$state_file" python3 "$GNOME_HELPER" register --gsettings-bin "$mock_gsettings" --command "/home/user/.local/bin/fizz-search"
    binding_val=$(MOCK_STATE_FILE="$state_file" "$mock_gsettings" get org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/fizz/ binding)
    assert_eq "'<Super>space'" "$binding_val" "GNOME helper preserves user-customized binding during updates"

    # Test collision warning
    MOCK_STATE_FILE="$state_file" "$mock_gsettings" set org.gnome.settings-daemon.plugins.media-keys custom-keybindings "['/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/custom0/']"
    MOCK_STATE_FILE="$state_file" "$mock_gsettings" set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/custom0/ name "'Other Tool'"
    MOCK_STATE_FILE="$state_file" "$mock_gsettings" set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/custom0/ binding "'<Control><Super>space'"
    local warn_out
    warn_out=$(MOCK_STATE_FILE="$state_file" python3 "$GNOME_HELPER" register --gsettings-bin "$mock_gsettings" --command "/home/user/.local/bin/fizz-search" 2>&1)
    assert_contains "already claimed" "$warn_out" "GNOME helper warns if <Control><Super>space is already claimed"

    # Test unregistration
    MOCK_STATE_FILE="$state_file" python3 "$GNOME_HELPER" unregister --gsettings-bin "$mock_gsettings"
    list_val=$(MOCK_STATE_FILE="$state_file" "$mock_gsettings" get org.gnome.settings-daemon.plugins.media-keys custom-keybindings)
    assert_not_contains "/fizz/" "$list_val" "GNOME helper unregisters /fizz/ from custom-keybindings"
    assert_contains "custom0" "$list_val" "GNOME helper retains custom0 after removing fizz"

    rm -rf "$tmp_dir"
}

test_gnome_helper_gvariant_parsing
test_gnome_helper_mock_gsettings

# -----------------------------------------------------------------------------
# Seam 3: Window Manager Sentinel Tests
# -----------------------------------------------------------------------------
echo "--- Seam 3: Window Manager Sentinels ---"

test_wm_sentinel_injection_and_removal() {
    local tmp_dir
    tmp_dir=$(mktemp -d)
    local hypr_conf="$tmp_dir/hyprland.conf"

    cat > "$hypr_conf" <<'EOF'
# User config
monitor=,preferred,auto,1
input {
    kb_layout = us
}
EOF

    # 1. Inject hotkey
    bash -c "
        source '$INSTALL_SH'
        inject_wm_hotkey '$hypr_conf' 'bind = CTRL SUPER, SPACE, exec, /mock/bin/fizz-search'
    "
    local content
    content=$(cat "$hypr_conf")
    assert_contains "# BEGIN FIZZ HOTKEY" "$content" "WM sentinel injects # BEGIN FIZZ HOTKEY"
    assert_contains "bind = CTRL SUPER, SPACE, exec, /mock/bin/fizz-search" "$content" "WM sentinel injects hotkey command"
    assert_contains "# END FIZZ HOTKEY" "$content" "WM sentinel injects # END FIZZ HOTKEY"

    # 2. Idempotency on update run - do not duplicate block
    bash -c "
        source '$INSTALL_SH'
        inject_wm_hotkey '$hypr_conf' 'bind = CTRL SUPER, SPACE, exec, /mock/bin/fizz-search'
    "
    local count
    count=$(grep -c "# BEGIN FIZZ HOTKEY" "$hypr_conf")
    assert_eq "1" "$count" "WM sentinel injection is idempotent"

    # 3. Preservation of customized binding during update
    # User edits binding to SUPER, SPACE
    sed -i 's/CTRL SUPER, SPACE/SUPER, SPACE/' "$hypr_conf"
    bash -c "
        source '$INSTALL_SH'
        inject_wm_hotkey '$hypr_conf' 'bind = CTRL SUPER, SPACE, exec, /mock/bin/fizz-search'
    "
    content=$(cat "$hypr_conf")
    assert_contains "bind = SUPER, SPACE, exec, /mock/bin/fizz-search" "$content" "WM sentinel preserves customized hotkey"

    # 4. Clean removal
    bash -c "
        source '$INSTALL_SH'
        remove_wm_hotkey '$hypr_conf'
    "
    content=$(cat "$hypr_conf")
    assert_not_contains "FIZZ HOTKEY" "$content" "WM sentinel cleanly removed on uninstallation"
    assert_not_contains "fizz-search" "$content" "WM hotkey line removed"
    assert_contains "kb_layout = us" "$content" "User config intact after hotkey removal"

    rm -rf "$tmp_dir"
}

test_desktop_detection_priority() {
    local out
    out=$(bash -c "
        source '$INSTALL_SH'
        export XDG_CURRENT_DESKTOP='GNOME'
        hotkey_gnome() { echo 'gnome_called'; return 0; }
        hotkey_hyprland() { echo 'hyprland_called'; return 0; }
        setup_hotkeys
    ")
    assert_eq "gnome_called" "$out" "GNOME desktop is prioritized when XDG_CURRENT_DESKTOP contains GNOME"
}

test_wm_sentinel_injection_and_removal
test_desktop_detection_priority

# -----------------------------------------------------------------------------
# Seam 4: Lifecycle & Sandbox Tests (Install, Update, Uninstall, Purge)
# -----------------------------------------------------------------------------
echo "--- Seam 4: Lifecycle & Sandbox ---"

test_sandbox_lifecycle() {
    local sandbox
    sandbox=$(mktemp -d)

    local mock_bin="$sandbox/mock_bin"
    mkdir -p "$mock_bin"

    # Mock systemctl to isolate from real user systemd session
    local systemctl_log="$sandbox/systemctl.log"
    cat > "$mock_bin/systemctl" <<EOF
#!/usr/bin/env bash
echo "\$@" >> "$systemctl_log"
if [ "\$1" = "--user" ] && [ "\$2" = "show-environment" ]; then
    exit 0
fi
exit 0
EOF
    chmod +x "$mock_bin/systemctl"

    # Run in sandbox subshell
    (
        export HOME="$sandbox"
        export PATH="$mock_bin:$PATH"

        # 1. Clean installation
        "$INSTALL_SH" --no-hotkey
        [ -x "$HOME/.local/bin/fizz-search" ] || exit 10
        [ -x "$HOME/.local/bin/fizzd" ] || exit 11
        [ -f "$HOME/.config/systemd/user/fizzd.service" ] || exit 12

        # 2. Idempotent re-installation
        "$INSTALL_SH" --no-hotkey
        [ -x "$HOME/.local/bin/fizz-search" ] || exit 20

        # Create user config and cache
        mkdir -p "$HOME/.config/fizz"
        echo "auto_close_on_unfocus=false" > "$HOME/.config/fizz/fizz.conf"
        echo "mock_cache" > "$HOME/.fizzLocalCache.bin"

        # 3. Standard uninstallation via uninstall.sh
        "$UNINSTALL_SH"
        [ ! -f "$HOME/.local/bin/fizz-search" ] || exit 30
        [ ! -f "$HOME/.local/bin/fizzd" ] || exit 31
        [ ! -f "$HOME/.config/systemd/user/fizzd.service" ] || exit 32
        # Config and cache must be preserved!
        [ -f "$HOME/.config/fizz/fizz.conf" ] || exit 33
        [ -f "$HOME/.fizzLocalCache.bin" ] || exit 34

        # 4. Re-install then test --purge
        "$INSTALL_SH" --no-hotkey
        "$UNINSTALL_SH" --purge
        [ ! -f "$HOME/.local/bin/fizz-search" ] || exit 40
        [ ! -d "$HOME/.config/fizz" ] || exit 41
        [ ! -f "$HOME/.fizzLocalCache.bin" ] || exit 42
    )
    local sub_ret=$?
    if [ "$sub_ret" -eq 0 ]; then
        pass "Sandbox installation, update, uninstall, and purge succeeded"
    else
        fail "Sandbox lifecycle failed with exit code $sub_ret" "Failed assertion code: $sub_ret"
    fi

    # Verify systemctl was called with daemon-reload, restart, stop, disable
    local sys_ops
    sys_ops=$(cat "$systemctl_log")
    assert_contains "daemon-reload" "$sys_ops" "systemctl daemon-reload was invoked"
    assert_contains "restart fizzd.service" "$sys_ops" "systemctl restart was invoked"
    assert_contains "stop fizzd.service" "$sys_ops" "systemctl stop was invoked on uninstall"
    assert_contains "disable fizzd.service" "$sys_ops" "systemctl disable was invoked on uninstall"

    rm -rf "$sandbox"
}

test_sandbox_autostart_fallback() {
    local sandbox
    sandbox=$(mktemp -d)

    local mock_bin="$sandbox/mock_bin"
    mkdir -p "$mock_bin"

    # Mock systemctl to fail show-environment, simulating non-systemd environment
    cat > "$mock_bin/systemctl" <<'EOF'
#!/usr/bin/env bash
exit 1
EOF
    chmod +x "$mock_bin/systemctl"

    # System without working user systemd session
    (
        export HOME="$sandbox"
        export PATH="$mock_bin:$PATH"

        "$INSTALL_SH" --no-hotkey
        [ -f "$HOME/.config/autostart/fizzd.desktop" ] || exit 50
        "$UNINSTALL_SH"
        [ ! -f "$HOME/.config/autostart/fizzd.desktop" ] || exit 51
    )
    local sub_ret=$?
    if [ "$sub_ret" -eq 0 ]; then
        pass "Autostart fallback creates desktop entry and uninstalls cleanly"
    else
        fail "Autostart fallback failed with exit code $sub_ret" "Failed assertion code: $sub_ret"
    fi

    rm -rf "$sandbox"
}

test_sandbox_lifecycle
test_sandbox_autostart_fallback

if [ "$FAILED" -ne 0 ]; then
    echo "Tests failed with $FAILED failure(s)."
    exit 1
fi
echo "All tests passed successfully!"
