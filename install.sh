#!/usr/bin/env bash

# Exit immediately if a command exits with a non-zero status
set -e

clear
echo "===================================================="
echo "    FIZZ FUZZY SEARCH - ARCH LINUX BACKEND ENGINE   "
echo "===================================================="

echo "[1/4] Verifying Arch package requirements..."
DEPENDENCIES=(gcc)
MISSING_DEPS=()

for pkg in "${DEPENDENCIES[@]}"; do
    if ! pacman -Qi "$pkg" &> /dev/null && ! pacman -Qg "$pkg" &> /dev/null; then
        MISSING_DEPS+=("$pkg")
    fi
done

if [ ${#MISSING_DEPS[@]} -ne 0 ]; then
    echo "[ERROR] Missing required Arch packages: ${MISSING_DEPS[*]}"
    echo "Please install them via: sudo pacman -S base-devel"
    exit 1
fi

INSTALL_DIR="$HOME/.local/bin"
SYSTEMD_DIR="$HOME/.config/systemd/user"
mkdir -p "$INSTALL_DIR"
mkdir -p "$SYSTEMD_DIR"

echo "[2/4] Running raw Make compilation pipeline..."
make clean || true
make -j$(nproc)
make install

echo "[3/4] Registering and launching background systemd user service..."
cat << EOF > "$SYSTEMD_DIR/fizz-daemon.service"
[Unit]
Description=Fizz Fuzzy Search Filesystem Monitor Daemon
After=local-fs.target

[Service]
Type=simple
ExecStart=$INSTALL_DIR/fizz-daemon
Restart=on-failure
RestartSec=3

[Install]
WantedBy=default.target
EOF

systemctl --user daemon-reload
systemctl --user enable fizz-daemon.service
systemctl --user start fizz-daemon.service

echo "[4/4] Configuring dynamic hotkey hooks..."
MANUAL_SHORTCUT_NEEDED=false

if [ -n "$HYPRLAND_INSTANCE_SIGNATURE" ]; then
    echo " -> Arch Hyprland environment caught!"
    # Binds hotkey to launch fizz-search natively inside your default system terminal environment
    hyprctl keyword bind "SUPER, SPACE, exec, alacritty -e $INSTALL_DIR/fizz-search" &> /dev/null || \
    hyprctl keyword bind "SUPER, SPACE, exec, kitty -e $INSTALL_DIR/fizz-search" &> /dev/null || \
    hyprctl keyword bind "SUPER, SPACE, exec, foot -e $INSTALL_DIR/fizz-search" &> /dev/null || true
    
    MANUAL_SHORTCUT_NEEDED=true
    SHORTCUT_MSG="Append this to your ~/.config/hypr/hyprland.conf for persistence:\n    bind = \$mainMod, SPACE, exec, kitty -e $INSTALL_DIR/fizz-search"
fi

echo "----------------------------------------------------"
echo " SUCCESS: Fizz Backend Engine is fully operational!"
echo "===================================================="
if [ "$MANUAL_SHORTCUT_NEEDED" = true ]; then
    echo -e "\n[IMPORTANT NOTE]\n$SHORTCUT_MSG"
fi

