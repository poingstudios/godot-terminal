extends RefCounted

const SETTINGS_FILE := "user://godot_terminal_settings.json"

var font_size: int = 13
var font_name: String = ""
var theme_name: String = "vscode_dark"
var cursor_shape: int = 0
var cursor_blink: bool = true
var scrollback_limit: int = 5000
var default_shell_override: String = ""
var shell_args: PackedStringArray = []
var custom_env: Dictionary = {}
var working_dir_mode: String = "project"

var shortcut_keycode: int = KEY_QUOTELEFT
var shortcut_ctrl: bool = true
var shortcut_alt: bool = false
var shortcut_shift: bool = false

func get_default_shell() -> String:
	if not default_shell_override.is_empty():
		return default_shell_override

	var os_name := OS.get_name()
	if os_name == "Windows":
		return "powershell.exe"
	elif os_name == "macOS":
		var shell_env := OS.get_environment("SHELL")
		if not shell_env.is_empty():
			return shell_env
		return "/bin/zsh"
	else:
		var shell_env := OS.get_environment("SHELL")
		if not shell_env.is_empty():
			return shell_env
		return "/bin/bash"

func get_default_working_directory() -> String:
	if working_dir_mode == "project":
		return ProjectSettings.globalize_path("res://")
	elif working_dir_mode == "home":
		return OS.get_system_dir(OS.SYSTEM_DIR_DOCUMENTS)
	return ProjectSettings.globalize_path("res://")

func get_available_shells() -> Array:
	var list: Array = []
	var os_name := OS.get_name()

	if os_name == "Windows":
		var candidates: Array = [
			{"name": "PowerShell", "path": "powershell.exe", "args": []},
			{"name": "PowerShell 7", "path": "pwsh.exe", "args": []},
			{"name": "Command Prompt", "path": "cmd.exe", "args": []},
			{"name": "Git Bash", "path": "C:\\Program Files\\Git\\bin\\bash.exe", "args": ["--login", "-i"]},
			{"name": "WSL", "path": "wsl.exe", "args": []}
		]
		for c in candidates:
			list.append(c)
	elif os_name == "macOS":
		var candidates: Array = [
			{"name": "Zsh", "path": "/bin/zsh", "args": ["-l"]},
			{"name": "Bash", "path": "/bin/bash", "args": ["-l"]},
			{"name": "Fish", "path": "/opt/homebrew/bin/fish", "args": ["-l"]},
			{"name": "Fish (Local)", "path": "/usr/local/bin/fish", "args": ["-l"]}
		]
		for c in candidates:
			if FileAccess.file_exists(str(c["path"])):
				list.append(c)
	else:
		var candidates: Array = [
			{"name": "Bash", "path": "/bin/bash", "args": ["-l"]},
			{"name": "Zsh", "path": "/usr/bin/zsh", "args": ["-l"]},
			{"name": "Fish", "path": "/usr/bin/fish", "args": ["-l"]}
		]
		for c in candidates:
			if FileAccess.file_exists(str(c["path"])):
				list.append(c)

	return list

func save_to_file() -> void:
	var data := {
		"font_size": font_size,
		"cursor_shape": cursor_shape,
		"cursor_blink": cursor_blink,
		"scrollback_limit": scrollback_limit,
		"default_shell_override": default_shell_override,
		"working_dir_mode": working_dir_mode,
		"shortcut_keycode": shortcut_keycode,
		"shortcut_ctrl": shortcut_ctrl,
		"shortcut_alt": shortcut_alt,
		"shortcut_shift": shortcut_shift
	}
	var file := FileAccess.open(SETTINGS_FILE, FileAccess.WRITE)
	if file != null:
		file.store_string(JSON.stringify(data, "\t"))

func load_from_file() -> void:
	if not FileAccess.file_exists(SETTINGS_FILE):
		return
	var file := FileAccess.open(SETTINGS_FILE, FileAccess.READ)
	if file == null:
		return
	var text := file.get_as_text()
	var parsed: Variant = JSON.parse_string(text)
	if parsed is Dictionary:
		var d := parsed as Dictionary
		if d.has("font_size"): font_size = int(d["font_size"])
		if d.has("cursor_shape"): cursor_shape = int(d["cursor_shape"])
		if d.has("cursor_blink"): cursor_blink = bool(d["cursor_blink"])
		if d.has("scrollback_limit"): scrollback_limit = int(d["scrollback_limit"])
		if d.has("default_shell_override"): default_shell_override = str(d["default_shell_override"])
		if d.has("working_dir_mode"): working_dir_mode = str(d["working_dir_mode"])
		if d.has("shortcut_keycode"): shortcut_keycode = int(d["shortcut_keycode"])
		if d.has("shortcut_ctrl"): shortcut_ctrl = bool(d["shortcut_ctrl"])
		if d.has("shortcut_alt"): shortcut_alt = bool(d["shortcut_alt"])
		if d.has("shortcut_shift"): shortcut_shift = bool(d["shortcut_shift"])
