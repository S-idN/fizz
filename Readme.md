# Fizz - System Wide Fuzzy Search!
* A super quick system wide fuzzy search app.
* Specialises in finding files

## Demo Screenshot
![Descriptive Alt Text](assets/DearimGUIV1.png)

## V1
* I cannot speak to this thing's reliability but it seems to work on my PC™ for now (Arch btw).

## Prerequisites
Fizz needs a C++17 compiler, CMake, and GLFW + OpenGL development headers.
(Dear ImGui is bundled in `imgui/`, so there's nothing to install for it.)

| Distro | Command |
| --- | --- |
| Arch | `sudo pacman -S --needed base-devel cmake glfw-x11 pkgconf` |
| Fedora | `sudo dnf install gcc-c++ make cmake glfw-devel mesa-libGL-devel pkgconf-pkg-config` |
| Debian/Ubuntu | `sudo apt install build-essential cmake libglfw3-dev libgl1-mesa-dev pkg-config` |
| openSUSE | `sudo zypper install gcc-c++ make cmake glfw3-devel Mesa-libGL-devel pkgconf-pkg-config` |

Only Arch (Hyprland) has been tested so far. The other package names are thoughts and prayers, so please open an issue if one is wrong.

## Install
```bash
git clone https://github.com/S-idN/fizz.git
cd fizz
chmod +x install.sh
./install.sh
```

It installs `fizz-search` and `fizzd` into `~/.local/bin` and starts the index daemon as a systemd user service (falling back to XDG autostart if systemd user services are unavailable).

### Installer Options
* `./install.sh --install-deps`: Automatically install missing toolchain packages using `sudo`.
* `./install.sh --no-hotkey`: Skip desktop hotkey registration.
* `./install.sh --uninstall` (or `./uninstall.sh`): Completely remove installed binaries, systemd/autostart services, and configured hotkeys. Preserves your configuration (`~/.config/fizz/`) and binary cache (`~/.fizzLocalCache.bin`).
* `./install.sh --purge` (or `./uninstall.sh --purge`): Perform a full uninstallation and also remove `~/.config/fizz/` and `~/.fizzLocalCache.bin`.

## Hotkey
The installer automatically sets up a default shortcut (`Ctrl+Super+Space`) on supported desktops and preserves any user customizations during updates:
* **GNOME:** Registered cleanly under `/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/fizz/` without clobbering other custom keybindings.
* **Hyprland, Sway, i3:** Managed with structured sentinel comments (`# BEGIN FIZZ HOTKEY` ... `# END FIZZ HOTKEY`) for safe updates and clean uninstallation.
* **Other desktops (KDE, XFCE, ...):** Bind `~/.local/bin/fizz-search` in your desktop keyboard shortcut settings.

## TL;DR of working
* I wanted to make a spotlight style file finder tool that was relatively quick and worked sorta like Spotlight or the Windows PowerToys search tool.
* **Inotify** detects file changes which then causes the daemon to generate a binary cache file containing structs (with file info) and a character array (of file names).
* As the user enters the query the search function is called, it ranks the characters and gives them scores.
* **Bounded Levenshtein distance** is used with further score boosts for exact filename matches and a lesser boost for a substring match.

## Up Next
* Add a content parsing step to match not only filenames but match the contents within the files
* Add an intermediary binary cache, a transient cache that contains recently updated files
* Improve the frontend, I don't have much experience using Dear imGUI so that needs to improved

