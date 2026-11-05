#!/usr/bin/env python3
"""GNOME custom keybinding manager for Fizz.

Provides robust GVariant array parsing and updates for
org.gnome.settings-daemon.plugins.media-keys custom-keybindings.
"""

import argparse
import ast
import re
import shutil
import subprocess
import sys
from typing import List

FIZZ_PATH = "/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/fizz/"
DEFAULT_BINDING = "<Control><Super>space"
DEFAULT_NAME = "Fizz Search"
SCHEMA_MEDIA_KEYS = "org.gnome.settings-daemon.plugins.media-keys"
RELOCATABLE_SCHEMA = "org.gnome.settings-daemon.plugins.media-keys.custom-keybinding"


def parse_gvariant_str_list(raw: str) -> List[str]:
    """Parse a GVariant string array (e.g. '@as []', '[]', or "['/path1/', '/path2/']")."""
    s = raw.strip()
    if not s:
        return []
    if s.startswith("@as"):
        s = s[3:].strip()
    if s == "[]" or not s:
        return []
    try:
        val = ast.literal_eval(s)
        if isinstance(val, (list, tuple)):
            return [str(item) for item in val]
    except Exception:
        pass
    # Fallback to regex extraction of single-quoted strings
    return re.findall(r"'([^']*)'", s)


def format_gvariant_str_list(items: List[str]) -> str:
    """Format a Python list of strings into a GVariant string representation."""
    if not items:
        return "@as []"
    inner = ", ".join(f"'{item}'" for item in items)
    return f"[{inner}]"


def append_fizz_path(raw: str, fizz_path: str = FIZZ_PATH) -> str:
    """Safely append fizz_path to GVariant array if not present."""
    items = parse_gvariant_str_list(raw)
    if fizz_path not in items:
        items.append(fizz_path)
    return format_gvariant_str_list(items)


def remove_fizz_path(raw: str, fizz_path: str = FIZZ_PATH) -> str:
    """Safely remove fizz_path from GVariant array."""
    items = parse_gvariant_str_list(raw)
    items = [item for item in items if item != fizz_path]
    return format_gvariant_str_list(items)


def run_gsettings(gsettings_bin: str, *args) -> str:
    """Execute a gsettings command and return its stdout."""
    res = subprocess.run(
        [gsettings_bin, *args],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    return res.stdout.strip()


def strip_quotes(s: str) -> str:
    s = s.strip()
    if (s.startswith("'") and s.endswith("'")) or (s.startswith('"') and s.endswith('"')):
        return s[1:-1]
    return s


def cmd_register(args: argparse.Namespace) -> int:
    gsettings = args.gsettings_bin
    target_path = args.path
    command = args.command
    default_binding = args.binding
    name = args.name

    # 1. Read existing custom-keybindings list
    raw_list = run_gsettings(gsettings, "get", SCHEMA_MEDIA_KEYS, "custom-keybindings")
    existing_paths = parse_gvariant_str_list(raw_list)

    # 2. Check collisions with default binding on other custom shortcuts
    for p in existing_paths:
        if p == target_path:
            continue
        existing_bind = strip_quotes(
            run_gsettings(gsettings, "get", f"{RELOCATABLE_SCHEMA}:{p}", "binding")
        )
        if existing_bind == default_binding:
            existing_name = strip_quotes(
                run_gsettings(gsettings, "get", f"{RELOCATABLE_SCHEMA}:{p}", "name")
            )
            print(
                f"Warning: Keybinding '{default_binding}' is already claimed by "
                f"custom shortcut '{existing_name}' ({p}).",
                file=sys.stderr,
            )

    # 3. Check if target_path already has a configured keybinding
    current_binding = strip_quotes(
        run_gsettings(gsettings, "get", f"{RELOCATABLE_SCHEMA}:{target_path}", "binding")
    )
    final_binding = default_binding
    if current_binding and current_binding != "@as []":
        # Preserve user's configured shortcut
        final_binding = current_binding

    # 4. Set schema properties
    fizz_schema = f"{RELOCATABLE_SCHEMA}:{target_path}"
    run_gsettings(gsettings, "set", fizz_schema, "name", f"'{name}'")
    run_gsettings(gsettings, "set", fizz_schema, "command", f"'{command}'")
    run_gsettings(gsettings, "set", fizz_schema, "binding", f"'{final_binding}'")

    # 5. Append target path to custom-keybindings array if needed
    new_list_str = append_fizz_path(raw_list, target_path)
    if new_list_str != raw_list:
        run_gsettings(gsettings, "set", SCHEMA_MEDIA_KEYS, "custom-keybindings", new_list_str)

    print(f"GNOME hotkey configured ({final_binding}): {command}")
    return 0


def cmd_unregister(args: argparse.Namespace) -> int:
    gsettings = args.gsettings_bin
    target_path = args.path

    # 1. Remove from custom-keybindings
    raw_list = run_gsettings(gsettings, "get", SCHEMA_MEDIA_KEYS, "custom-keybindings")
    existing_paths = parse_gvariant_str_list(raw_list)

    if target_path in existing_paths:
        new_list_str = remove_fizz_path(raw_list, target_path)
        run_gsettings(gsettings, "set", SCHEMA_MEDIA_KEYS, "custom-keybindings", new_list_str)

    # 2. Reset keys in gsettings
    fizz_schema = f"{RELOCATABLE_SCHEMA}:{target_path}"
    run_gsettings(gsettings, "reset", fizz_schema, "name")
    run_gsettings(gsettings, "reset", fizz_schema, "command")
    run_gsettings(gsettings, "reset", fizz_schema, "binding")

    # 3. If dconf is available, reset the entire path
    dconf_bin = getattr(args, "dconf_bin", "dconf")
    if shutil.which(dconf_bin):
        try:
            subprocess.run([dconf_bin, "reset", "-f", target_path], capture_output=True)
        except Exception:
            pass

    print(f"GNOME hotkey removed: {target_path}")
    return 0


def main():
    parser = argparse.ArgumentParser(description="GNOME shortcut helper for Fizz")
    subparsers = parser.add_subparsers(dest="action", required=True)

    # Subcommand: append
    p_append = subparsers.add_parser("append")
    p_append.add_argument("gvariant_str", help="Current GVariant array string")
    p_append.add_argument("--path", default=FIZZ_PATH, help="Path to append")

    # Subcommand: remove
    p_remove = subparsers.add_parser("remove")
    p_remove.add_argument("gvariant_str", help="Current GVariant array string")
    p_remove.add_argument("--path", default=FIZZ_PATH, help="Path to remove")

    # Subcommand: register
    p_reg = subparsers.add_parser("register")
    p_reg.add_argument("--gsettings-bin", default="gsettings", help="Path to gsettings executable")
    p_reg.add_argument("--command", required=True, help="Command to run on hotkey trigger")
    p_reg.add_argument("--binding", default=DEFAULT_BINDING, help="Default key binding")
    p_reg.add_argument("--name", default=DEFAULT_NAME, help="Shortcut name")
    p_reg.add_argument("--path", default=FIZZ_PATH, help="GSettings path")

    # Subcommand: unregister
    p_unreg = subparsers.add_parser("unregister")
    p_unreg.add_argument("--gsettings-bin", default="gsettings", help="Path to gsettings executable")
    p_unreg.add_argument("--dconf-bin", default="dconf", help="Path to dconf executable")
    p_unreg.add_argument("--path", default=FIZZ_PATH, help="GSettings path")

    args = parser.parse_args()

    if args.action == "append":
        print(append_fizz_path(args.gvariant_str, args.path))
    elif args.action == "remove":
        print(remove_fizz_path(args.gvariant_str, args.path))
    elif args.action == "register":
        sys.exit(cmd_register(args))
    elif args.action == "unregister":
        sys.exit(cmd_unregister(args))


if __name__ == "__main__":
    main()
