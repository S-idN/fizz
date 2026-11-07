#!/usr/bin/env bash
set -euo pipefail

BIN_DIR="${FIZZ_BIN_DIR:-$HOME/.local/bin}"
UNIT_DIR="${FIZZ_UNIT_DIR:-$HOME/.config/systemd/user}"
AUTOSTART_DIR="${FIZZ_AUTOSTART_DIR:-$HOME/.config/autostart}"
CONFIG_DIR="${FIZZ_CONFIG_DIR:-$HOME/.config/fizz}"
BINARY_CACHE_PATH="${FIZZ_BINARY_CACHE:-$HOME/.fizzLocalCache.bin}"
HYPR_CONF="${FIZZ_HYPR_CONF:-$HOME/.config/hypr/hyprland.conf}"
SWAY_CONF="${FIZZ_SWAY_CONF:-$HOME/.config/sway/config}"
I3_CONF="${FIZZ_I3_CONF:-$HOME/.config/i3/config}"
HOTKEY_CMD="$BIN_DIR/fizz-search"

SENTINEL_BEGIN="# BEGIN FIZZ HOTKEY"
SENTINEL_END="# END FIZZ HOTKEY"

PKGS_PACMAN="base-devel cmake glfw-x11 pkgconf"
PKGS_APT="build-essential cmake libglfw3-dev libgl1-mesa-dev pkg-config"
PKGS_DNF="gcc-c++ make cmake glfw-devel mesa-libGL-devel pkgconf-pkg-config"
PKGS_ZYPPER="gcc-c++ make cmake glfw3-devel Mesa-libGL-devel pkgconf-pkg-config"

deps_command() {
    if   command -v pacman  >/dev/null; then echo "sudo pacman -S --needed $PKGS_PACMAN"
    elif command -v apt-get >/dev/null; then echo "sudo apt-get install $PKGS_APT"
    elif command -v dnf     >/dev/null; then echo "sudo dnf install $PKGS_DNF"
    elif command -v zypper  >/dev/null; then echo "sudo zypper install $PKGS_ZYPPER"
    fi
}

has_glfw() {
    local f
    for f in /usr/lib*/cmake/glfw3/glfw3Config.cmake \
             /usr/lib/*/cmake/glfw3/glfw3Config.cmake \
             /usr/local/lib*/cmake/glfw3/glfw3Config.cmake; do
        [ -f "$f" ] && return 0
    done
    command -v pkg-config >/dev/null && pkg-config --exists glfw3 && return 0
    return 1
}

has_opengl() {
    command -v pkg-config >/dev/null && pkg-config --exists gl && return 0
    local f
    for f in /usr/include/GL/gl.h \
             /usr/local/include/GL/gl.h \
             /usr/include/*/GL/gl.h; do
        [ -f "$f" ] && return 0
    done
    return 1
}

has_cpp17() {
    local compiler
    for compiler in c++ g++ clang++; do
        if command -v "$compiler" >/dev/null; then
            if "$compiler" -std=c++17 -E -x c++ /dev/null -o /dev/null >/dev/null 2>&1; then
                return 0
            fi
        fi
    done
    return 1
}

deps_missing() {
    command -v cmake >/dev/null || return 0
    has_cpp17        || return 0
    (command -v make >/dev/null || command -v ninja >/dev/null) || return 0
    has_glfw || return 0
    has_opengl || return 0
    return 1
}

inject_wm_hotkey() {
    local conf="$1"
    local hotkey_line="$2"
    [ -f "$conf" ] || return 1

    if grep -q "$SENTINEL_BEGIN" "$conf" 2>/dev/null; then
        return 0
    fi

    if grep -q "fizz-search" "$conf" 2>/dev/null; then
        return 0
    fi

    printf '\n%s\n%s\n%s\n' "$SENTINEL_BEGIN" "$hotkey_line" "$SENTINEL_END" >> "$conf"
}

remove_wm_hotkey() {
    local conf="$1"
    [ -f "$conf" ] || return 0

    sed -i "/$SENTINEL_BEGIN/,/$SENTINEL_END/d" "$conf"
}

hotkey_hyprland() {
    [ -f "$HYPR_CONF" ] || return 1
    inject_wm_hotkey "$HYPR_CONF" "bind = CTRL SUPER, SPACE, exec, $HOTKEY_CMD"
    command -v hyprctl >/dev/null && hyprctl reload >/dev/null 2>&1 || true
    echo "Hyprland hotkey configured: Ctrl+Super+Space"
}

hotkey_sway() {
    [ -f "$SWAY_CONF" ] || return 1
    inject_wm_hotkey "$SWAY_CONF" "bindsym Ctrl+Mod4+space exec $HOTKEY_CMD"
    command -v swaymsg >/dev/null && swaymsg reload >/dev/null 2>&1 || true
    echo "Sway hotkey configured: Ctrl+Super+Space"
}

hotkey_i3() {
    [ -f "$I3_CONF" ] || return 1
    inject_wm_hotkey "$I3_CONF" "bindsym Ctrl+Mod4+space exec $HOTKEY_CMD"
    command -v i3-msg >/dev/null && i3-msg reload >/dev/null 2>&1 || true
    echo "i3 hotkey configured: Ctrl+Super+Space"
}

hotkey_gnome() {
    command -v gsettings >/dev/null || return 1
    case "${XDG_CURRENT_DESKTOP:-}" in *GNOME*) ;; *) return 1 ;; esac
    command -v python3 >/dev/null || return 1

    local script_dir
    script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    python3 "$script_dir/scripts/gnome_shortcut.py" register --command "$HOTKEY_CMD"
}

setup_hotkeys() {
    local configured=0
    case "${XDG_CURRENT_DESKTOP:-}" in
        *GNOME*)
            hotkey_gnome && configured=1 ;;
        *Hyprland*)
            hotkey_hyprland && configured=1 ;;
        *sway*)
            hotkey_sway && configured=1 ;;
        *i3*)
            hotkey_i3 && configured=1 ;;
    esac

    if [ "$configured" -eq 0 ]; then
        hotkey_hyprland || hotkey_sway || hotkey_i3 || hotkey_gnome || {
            echo "Couldn't set a hotkey automatically for this desktop."
            echo "In your keyboard settings (KDE: System Settings -> Shortcuts), bind a key to:"
            echo "  $HOTKEY_CMD"
        }
    fi
}

unhotkey_all() {
    [ -f "$HYPR_CONF" ] && remove_wm_hotkey "$HYPR_CONF" && (command -v hyprctl >/dev/null && hyprctl reload >/dev/null 2>&1 || true)
    [ -f "$SWAY_CONF" ] && remove_wm_hotkey "$SWAY_CONF" && (command -v swaymsg >/dev/null && swaymsg reload >/dev/null 2>&1 || true)
    [ -f "$I3_CONF" ] && remove_wm_hotkey "$I3_CONF" && (command -v i3-msg >/dev/null && i3-msg reload >/dev/null 2>&1 || true)
    if command -v gsettings >/dev/null && command -v python3 >/dev/null; then
        local script_dir
        script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
        python3 "$script_dir/scripts/gnome_shortcut.py" unregister 2>/dev/null || true
    fi
}

install_service() {
    local exec_path="$BIN_DIR/fizzd"
    if [ "$BIN_DIR" = "$HOME/.local/bin" ]; then
        exec_path="%h/.local/bin/fizzd"
    fi

    if command -v systemctl >/dev/null && systemctl --user show-environment >/dev/null 2>&1; then
        mkdir -p "$UNIT_DIR"
        cat > "$UNIT_DIR/fizzd.service" <<EOF
[Unit]
Description=Fizz file index daemon

[Service]
ExecStart=$exec_path
Restart=on-failure
RestartSec=5
Nice=19
IOSchedulingClass=idle

[Install]
WantedBy=default.target
EOF
        systemctl --user daemon-reload
        systemctl --user enable fizzd.service
        systemctl --user restart fizzd.service
        echo "fizzd systemd user service enabled and running."
    else
        mkdir -p "$AUTOSTART_DIR"
        cat > "$AUTOSTART_DIR/fizzd.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Fizz index daemon
Exec=$BIN_DIR/fizzd
X-GNOME-Autostart-enabled=true
EOF
        echo "No systemd user session; installed XDG autostart entry (starts at next login)."
        echo "To start it now:  nohup $BIN_DIR/fizzd >/dev/null 2>&1 &"
    fi
}

uninstall_service() {
    if command -v systemctl >/dev/null && systemctl --user show-environment >/dev/null 2>&1; then
        systemctl --user stop fizzd.service 2>/dev/null || true
        systemctl --user disable fizzd.service 2>/dev/null || true
        rm -f "$UNIT_DIR/fizzd.service"
        systemctl --user daemon-reload 2>/dev/null || true
        echo "Stopped and disabled fizzd systemd user service."
    fi
    if [ -f "$AUTOSTART_DIR/fizzd.desktop" ]; then
        rm -f "$AUTOSTART_DIR/fizzd.desktop"
        echo "Removed XDG autostart entry: $AUTOSTART_DIR/fizzd.desktop"
    fi
}

uninstall_binaries() {
    rm -f "$BIN_DIR/fizz-search" "$BIN_DIR/fizzd"
    echo "Removed binaries: $BIN_DIR/fizz-search, $BIN_DIR/fizzd"
}

uninstall_fizz() {
    local purge="${1:-0}"
    local no_hotkey="${2:-0}"
    echo "Uninstalling Fizz..."
    uninstall_service
    if [ "$no_hotkey" -eq 0 ]; then
        unhotkey_all
    fi
    uninstall_binaries

    if [ "$purge" -eq 1 ]; then
        echo "Purging configuration and Binary Cache..."
        rm -rf "$CONFIG_DIR"
        rm -f "$BINARY_CACHE_PATH"
        echo "Purged $CONFIG_DIR and $BINARY_CACHE_PATH."
    else
        echo "Preserved configuration ($CONFIG_DIR) and Binary Cache ($BINARY_CACHE_PATH)."
        echo "Pass --purge to remove them."
    fi
    echo "Fizz uninstalled successfully."
}

build_and_install() {
    local src_dir="$1"
    cmake -S "$src_dir" -B "$src_dir/build" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$src_dir/build" -j"$(nproc)" --target main_gui fizzd

    mkdir -p "$BIN_DIR"
    install -m755 "$src_dir/build/main_gui" "$BIN_DIR/fizz-search"
    install -m755 "$src_dir/build/fizzd"    "$BIN_DIR/fizzd"
    echo "Installed: $BIN_DIR/fizz-search, $BIN_DIR/fizzd"
}

run_install_deps() {
    local cmd
    cmd="$(deps_command)"
    [ -n "$cmd" ] || { echo "Unknown package manager; install dependencies manually."; exit 1; }
    echo "Running: $cmd"
    $cmd
}

main() {
    local src_dir
    src_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    cd "$src_dir"

    local install_deps=0
    local no_hotkey=0
    local uninstall=0
    local purge=0

    if [[ "$(basename "$0")" == "uninstall"* ]]; then
        uninstall=1
    fi

    while [ $# -gt 0 ]; do
        case "$1" in
            --install-deps) install_deps=1 ;;
            --no-hotkey) no_hotkey=1 ;;
            --uninstall) uninstall=1 ;;
            --purge) uninstall=1; purge=1 ;;
            *) echo "Unknown option: $1" >&2; exit 1 ;;
        esac
        shift
    done

    if [ "$uninstall" -eq 1 ]; then
        uninstall_fizz "$purge" "$no_hotkey"
        return 0
    fi

    if [ "$install_deps" -eq 1 ]; then
        run_install_deps
    fi

    if deps_missing; then
        local cmd
        cmd="$(deps_command)"
        echo "Missing build dependencies."
        if [ -n "$cmd" ]; then
            echo "Install them with:"
            echo "  $cmd"
            if [ -t 0 ] && [ "$install_deps" -eq 0 ]; then
                printf "Install missing dependencies using sudo? [y/N] "
                read -r reply
                if [[ "$reply" =~ ^[Yy]$ ]]; then
                    run_install_deps
                    if deps_missing; then
                        echo "Dependencies are still missing after package installation."
                        exit 1
                    fi
                else
                    exit 1
                fi
            else
                echo "Then re-run ./install.sh (or run ./install.sh --install-deps to do it for you, using sudo)."
                exit 1
            fi
        else
            echo "Unknown package manager; please install dependencies manually."
            exit 1
        fi
    fi

    build_and_install "$src_dir"
    install_service

    if [ "$no_hotkey" -eq 0 ]; then
        setup_hotkeys
    fi

    case ":$PATH:" in
        *":$BIN_DIR:"*) ;;
        *) echo "Note: $BIN_DIR is not on your PATH (the hotkey and service don't need it)." ;;
    esac

    echo
    echo "Done."
}

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
