#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

BIN_DIR="$HOME/.local/bin"
UNIT_DIR="$HOME/.config/systemd/user"
AUTOSTART_DIR="$HOME/.config/autostart"
HYPR_CONF="$HOME/.config/hypr/hyprland.conf"
HOTKEY_CMD="$BIN_DIR/fizz-search"

PKGS_PACMAN="base-devel cmake glfw-x11"
PKGS_APT="build-essential cmake libglfw3-dev libgl1-mesa-dev"
PKGS_DNF="gcc-c++ make cmake glfw-devel mesa-libGL-devel"
PKGS_ZYPPER="gcc-c++ make cmake glfw3-devel Mesa-libGL-devel"

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

deps_missing() {
    command -v cmake >/dev/null || return 0
    command -v c++   >/dev/null || return 0
    has_glfw || return 0
    return 1
}

if [ "${1:-}" = "--install-deps" ]; then
    cmd="$(deps_command)"
    [ -n "$cmd" ] || { echo "Unknown package manager; install dependencies manually."; exit 1; }
    echo "Running: $cmd"
    $cmd
fi

if deps_missing; then
    echo "Missing build dependencies. Install them with:"
    echo "  $(deps_command)"
    echo "Then re-run ./install.sh (or run ./install.sh --install-deps to do it for you, using sudo)."
    exit 1
fi

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)" --target main_gui fizzd

mkdir -p "$BIN_DIR"
install -m755 build/main_gui "$BIN_DIR/fizz-search"
install -m755 build/fizzd    "$BIN_DIR/fizzd"
echo "Installed: $BIN_DIR/fizz-search, $BIN_DIR/fizzd"

if command -v systemctl >/dev/null && systemctl --user show-environment >/dev/null 2>&1; then
    mkdir -p "$UNIT_DIR"
    cat > "$UNIT_DIR/fizzd.service" <<'EOF'
[Unit]
Description=Fizz file index daemon

[Service]
ExecStart=%h/.local/bin/fizzd
Restart=on-failure
RestartSec=5
Nice=19
IOSchedulingClass=idle
CPUSchedulingPolicy=idle

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

hotkey_hyprland() {
    [ -f "$HYPR_CONF" ] || return 1
    grep -q "fizz-search" "$HYPR_CONF" ||
        printf '\n# --- Fizz Search Hotkey ---\nbind = CTRL SUPER, SPACE, exec, %s\n' "$HOTKEY_CMD" >> "$HYPR_CONF"
    command -v hyprctl >/dev/null && hyprctl reload >/dev/null 2>&1 || true
    echo "Hyprland hotkey set: Ctrl+Super+Space"
}

hotkey_sway() {
    local conf="$HOME/.config/sway/config"
    [ -f "$conf" ] || return 1
    grep -q "fizz-search" "$conf" ||
        printf '\n# --- Fizz Search Hotkey ---\nbindsym Ctrl+Mod4+space exec %s\n' "$HOTKEY_CMD" >> "$conf"
    command -v swaymsg >/dev/null && swaymsg reload >/dev/null 2>&1 || true
    echo "Sway hotkey set: Ctrl+Super+Space"
}

hotkey_i3() {
    local conf="$HOME/.config/i3/config"
    [ -f "$conf" ] || return 1
    grep -q "fizz-search" "$conf" ||
        printf '\n# --- Fizz Search Hotkey ---\nbindsym Ctrl+Mod4+space exec %s\n' "$HOTKEY_CMD" >> "$conf"
    command -v i3-msg >/dev/null && i3-msg reload >/dev/null 2>&1 || true
    echo "i3 hotkey set: Ctrl+Super+Space"
}

hotkey_gnome() {
    command -v gsettings >/dev/null || return 1
    case "${XDG_CURRENT_DESKTOP:-}" in *GNOME*) ;; *) return 1 ;; esac

    local schema=org.gnome.settings-daemon.plugins.media-keys
    local path=/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/fizz/
    local cur new
    cur=$(gsettings get $schema custom-keybindings)
    case "$cur" in
        *"$path"*) ;;   
        *)  if [ "$cur" = "@as []" ] || [ "$cur" = "[]" ]; then
                new="['$path']"
            else
                new="${cur%]}, '$path']"   
            fi
            gsettings set $schema custom-keybindings "$new" ;;
    esac

    local key="$schema.custom-keybinding:$path"
    gsettings set "$key" name 'Fizz Search'
    gsettings set "$key" command "$HOTKEY_CMD"
    gsettings set "$key" binding '<Control><Super>space'
    echo "GNOME hotkey set: Ctrl+Super+Space"
}

hotkey_hyprland || hotkey_sway || hotkey_i3 || hotkey_gnome || {
    echo "Couldn't set a hotkey automatically for this desktop."
    echo "In your keyboard settings (KDE: System Settings -> Shortcuts), bind a key to:"
    echo "  $HOTKEY_CMD"
}

case ":$PATH:" in
    *":$BIN_DIR:"*) ;;
    *) echo "Note: $BIN_DIR is not on your PATH (the hotkey and service don't need it)." ;;
esac

echo
echo "Done."
