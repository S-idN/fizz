set -euo pipefail

cd "$(dirname "$0")"

BIN_DIR="$HOME/.local/bin"
UNIT_DIR="$HOME/.config/systemd/user"
AUTOSTART_DIR="$HOME/.config/autostart"
HYPR_CONF="$HOME/.config/hypr/hyprland.conf"

PKGS_PACMAN="base-devel cmake pkgconf re2"
PKGS_APT="build-essential cmake pkg-config libre2-dev"
PKGS_DNF="gcc-c++ make cmake pkgconf-pkg-config re2-devel"
PKGS_ZYPPER="gcc-c++ make cmake pkg-config re2-devel"

deps_command() {
    if   command -v pacman  >/dev/null; then echo "sudo pacman -S --needed $PKGS_PACMAN"
    elif command -v apt-get >/dev/null; then echo "sudo apt-get install $PKGS_APT"
    elif command -v dnf     >/dev/null; then echo "sudo dnf install $PKGS_DNF"
    elif command -v zypper  >/dev/null; then echo "sudo zypper install $PKGS_ZYPPER"
    fi
}

deps_missing() {
    command -v cmake >/dev/null || return 0
    command -v c++   >/dev/null || return 0
    command -v pkg-config >/dev/null || return 0
    pkg-config --exists re2 || return 0      
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

if [ -f "$HYPR_CONF" ]; then
    if ! grep -q "fizz-search" "$HYPR_CONF"; then
        {
            echo
            echo "# --- Fizz Search Hotkey ---"
            echo "bind = CTRL SUPER, SPACE, exec, $BIN_DIR/fizz-search"
        } >> "$HYPR_CONF"
        echo "Hyprland hotkey set: Ctrl+Super+Space"
    else
        echo "Hyprland hotkey already present, skipping."
    fi
    command -v hyprctl >/dev/null && hyprctl reload >/dev/null 2>&1 || true
else
    echo "Hyprland config not found. Set a keyboard shortcut in your desktop's settings"
    echo "(GNOME/KDE: Settings -> Keyboard -> Shortcuts) that runs:"
    echo "  $BIN_DIR/fizz-search"
fi

case ":$PATH:" in
    *":$BIN_DIR:"*) ;;
    *) echo "Note: $BIN_DIR is not on your PATH." ;;
esac

echo
echo "Done."
