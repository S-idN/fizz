set -e

clear
echo "===================================================="
echo "    FIZZ FUZZY SEARCH - ARCH LINUX PLUGIN INSTALLER   "
echo "===================================================="

DEPENDENCIES=(gcc glfw imgui opengl)
MISSING_DEPS=()

for pkg in "${DEPENDENCIES[@]}"; do
    if ! pacman -Qi "$pkg" &> /dev/null && ! pacman -Qg "$pkg" &> /dev/null; then
        MISSING_DEPS+=("$pkg")
    fi
done

if [ ${#MISSING_DEPS[@]} -ne 0 ]; then
    echo "[ERROR] Missing required Arch packages: ${MISSING_DEPS[*]}"
    echo "Please install them via: sudo pacman -S base-devel glfw-x11 imgui"
    exit 1
fi

INSTALL_DIR="$HOME/.local/bin"
SYSTEMD_DIR="$HOME/.config/systemd/user"
mkdir -p "$INSTALL_DIR"
mkdir -p "$SYSTEMD_DIR"

echo "[2/4] Compiling optimized standalone binaries..."
echo " -> Building background daemon..."
g++ -std=c++17 -O3 -o "$INSTALL_DIR/fizz-daemon" services/FileCacheBuilder.cpp services/FileSearch.cpp services/InvertedSearchIndex.cpp

echo " -> Building Dear ImGui Spotlight UI launcher..."
g++ -std=c++17 -pthread -O3 -o "$INSTALL_DIR/fizz-search" main_gui.cpp services/QueryOps.cpp services/LoadFile.cpp -lglfw -lGL -limgui

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

if [ "$XDG_CURRENT_DESKTOP" = "GNOME" ]; then
    echo " -> Arch GNOME Desktop detected. Mapping Super+Space..."
    PATH_ID="fizz-search-shortcut"
    KEY_PATH="/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/$PATH_ID/"
    
    dconf write "${KEY_PATH}name" "'Fizz Search'"
    dconf write "${KEY_PATH}command" "'$INSTALL_DIR/fizz-search'"
    dconf write "${KEY_PATH}binding" "'<Super>space'"
    
    CURRENT_BINDINGS=$(dconf read /org/gnome/settings-daemon/plugins/media-keys/custom-keybindings)
    if [[ "$CURRENT_BINDINGS" == "" || "$CURRENT_BINDINGS" == "[]" ]]; then
        dconf write /org/gnome/settings-daemon/plugins/media-keys/custom-keybindings "['$KEY_PATH']"
    elif [[ "$CURRENT_BINDINGS" != *"$PATH_ID"* ]]; then
        NEW_BINDINGS="${CURRENT_BINDINGS%]*}, '$KEY_PATH']"
        dconf write /org/gnome/settings-daemon/plugins/media-keys/custom-keybindings "$NEW_BINDINGS"
    fi

elif [ "$XDG_CURRENT_DESKTOP" = "KDE" ]; then
    echo " -> Arch KDE Plasma detected. Mapping Meta+Space..."
    if command -v kwriteconfig6 &> /dev/null; then
        kwriteconfig6 --file kglobalshortcutsrc --group "$INSTALL_DIR/fizz-search" --key "_launch" "Meta+Space,none,Launch Fizz Search"
        qdbus6 org.kde.kglobalaccel /component/krunner org.kde.kglobalaccel.Component.reconfigure &> /dev/null || true
    else
        kwriteconfig5 --file kglobalshortcutsrc --group "$INSTALL_DIR/fizz-search" --key "_launch" "Meta+Space,none,Launch Fizz Search"
        qdbus org.kde.kglobalaccel /component/krunner org.kde.kglobalaccel.Component.reconfigure &> /dev/null || true
    fi

elif [ -n "$HYPRLAND_INSTANCE_SIGNATURE" ]; then
    echo " -> Arch Hyprland environment caught!"
    echo " -> Dynamically registering keybind for current live session..."
    
    hyprctl keyword bind "SUPER, SPACE, exec, $INSTALL_DIR/fizz-search" &> /dev/null || \
    hyprctl keyword hl.bind "SUPER + SPACE", "hl.dsp.exec_cmd('$INSTALL_DIR/fizz-search')" &> /dev/null || true
    
    MANUAL_SHORTCUT_NEEDED=true
    SHORTCUT_MSG="Append this to your ~/.config/hypr/hyprland.conf for persistence:\n    bind = \$mainMod, SPACE, exec, $INSTALL_DIR/fizz-search"

elif [ -f "$HOME/.config/i3/config" ]; then
    MANUAL_SHORTCUT_NEEDED=true
    SHORTCUT_MSG="Append this to your ~/.config/i3/config for persistence:\n    bindsym \$mod+space exec --no-startup-id $INSTALL_DIR/fizz-search"
else
    MANUAL_SHORTCUT_NEEDED=true
    SHORTCUT_MSG="Bind a global hotkey inside your window manager config to run:\n    $INSTALL_DIR/fizz-search"
fi

echo " SUCCESS: Fizz Plugin is fully operational!"

if [ "$MANUAL_SHORTCUT_NEEDED" = true ]; then
    echo -e "\n[IMPORTANT NOTE]\n$SHORTCUT_MSG"
else
    echo " Hotkey mapped automatically. Press Super+Space / Alt+Space to open!"
fi
echo "===================================================="

