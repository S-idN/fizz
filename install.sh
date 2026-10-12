set -e 
cmake -S . -B build
cmake --build build -j --target main_gui

mkdir -p "$HOME/.local/bin"
cp ./build/main_gui "$HOME/.local/bin/fizz-search"

echo "Binary compiled and installed to: $HOME/.local/bin/fizz-search"

HYPR_CONF="$HOME/.config/hypr/hyprland.conf"

if [ -f "$HYPR_CONF" ]; then
    if ! grep -q "fizz-search" "$HYPR_CONF"; then
        echo -e "\n# --- Fizz Search Hotkey ---" >> "$HYPR_CONF"
        echo "bind = CTRL SUPER, SPACE, exec, $HOME/.local/bin/fizz-search" >> "$HYPR_CONF"
        echo "Hotkey set : (Ctrl + Super + Space) "
    else
        echo "Hotkey binding already exists in hyprland.conf, skipping."
    fi
else
    echo "Could not locate hyprland.conf at $HYPR_CONF."
    echo "   Try manually adding: 'bind = CTRL SUPER, SPACE, exec, /path/to/binary'"
fi

echo -e "\nInstallation Complete! Press Ctrl+Super+Space to open Fizz (or reload your Hyprland config)."

