extends ConfirmationDialog

signal settings_saved()

const TerminalConfigScript := preload("res://addons/godot_terminal/internal/core/terminal_config.gd")

var config: RefCounted = null

var font_size_spin := SpinBox.new()
var scrollback_spin := SpinBox.new()
var shell_override_line := LineEdit.new()
var shortcut_button := Button.new()

var is_recording_shortcut: bool = false
var temp_keycode: int = KEY_QUOTELEFT
var temp_ctrl: bool = true
var temp_alt: bool = false
var temp_shift: bool = false

func _ready() -> void:
	title = "Godot Terminal Settings"
	size = Vector2i(420, 260)

	var vb := VBoxContainer.new()
	vb.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	vb.size_flags_vertical = Control.SIZE_EXPAND_FILL
	add_child(vb)

	# Font Size
	var font_row := HBoxContainer.new()
	var font_label := Label.new()
	font_label.text = "Font Size:"
	font_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	font_row.add_child(font_label)

	font_size_spin.min_value = 9
	font_size_spin.max_value = 32
	font_size_spin.value = 13
	font_row.add_child(font_size_spin)
	vb.add_child(font_row)

	# Scrollback limit
	var sb_row := HBoxContainer.new()
	var sb_label := Label.new()
	sb_label.text = "Scrollback Limit (lines):"
	sb_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	sb_row.add_child(sb_label)

	scrollback_spin.min_value = 500
	scrollback_spin.max_value = 50000
	scrollback_spin.step = 500
	scrollback_spin.value = 5000
	sb_row.add_child(scrollback_spin)
	vb.add_child(sb_row)

	# Toggle Shortcut Row
	var sc_row := HBoxContainer.new()
	var sc_label := Label.new()
	sc_label.text = "Toggle Terminal Shortcut:"
	sc_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	sc_row.add_child(sc_label)

	shortcut_button.text = "Ctrl + `"
	shortcut_button.pressed.connect(_on_shortcut_button_pressed)
	sc_row.add_child(shortcut_button)
	vb.add_child(sc_row)

	# Default shell override
	var shell_row := VBoxContainer.new()
	var shell_label := Label.new()
	shell_label.text = "Custom Shell Executable (optional):"
	shell_row.add_child(shell_label)
	shell_override_line.placeholder_text = "/bin/zsh, /bin/bash, pwsh.exe"
	shell_row.add_child(shell_override_line)
	vb.add_child(shell_row)

	confirmed.connect(_on_confirmed)
	canceled.connect(_on_canceled)

func load_values(p_config) -> void:
	config = p_config
	if config == null:
		return

	font_size_spin.value = config.font_size
	scrollback_spin.value = config.scrollback_limit
	shell_override_line.text = config.default_shell_override

	temp_keycode = config.shortcut_keycode
	temp_ctrl = config.shortcut_ctrl
	temp_alt = config.shortcut_alt
	temp_shift = config.shortcut_shift
	_update_shortcut_button_text()

func _on_shortcut_button_pressed() -> void:
	is_recording_shortcut = true
	shortcut_button.text = "Press key combination..."

func _input(event: InputEvent) -> void:
	if not is_recording_shortcut or not (event is InputEventKey):
		return

	var ke := event as InputEventKey
	if not ke.pressed:
		return

	if ke.keycode == KEY_CTRL or ke.keycode == KEY_ALT or ke.keycode == KEY_SHIFT or ke.keycode == KEY_META:
		return

	temp_keycode = ke.physical_keycode if ke.keycode == 0 else ke.keycode
	temp_ctrl = ke.ctrl_pressed or ke.meta_pressed
	temp_alt = ke.alt_pressed
	temp_shift = ke.shift_pressed

	is_recording_shortcut = false
	_update_shortcut_button_text()
	get_viewport().set_input_as_handled()

func _update_shortcut_button_text() -> void:
	var parts: PackedStringArray = []
	if temp_ctrl:
		parts.append("Ctrl" if OS.get_name() != "macOS" else "Ctrl / Cmd")
	if temp_alt:
		parts.append("Alt")
	if temp_shift:
		parts.append("Shift")

	var key_name := OS.get_keycode_string(temp_keycode)
	if temp_keycode == KEY_QUOTELEFT or temp_keycode == 96:
		key_name = "`"
	parts.append(key_name)

	shortcut_button.text = " + ".join(parts)

func _on_canceled() -> void:
	is_recording_shortcut = false

func _on_confirmed() -> void:
	if config == null:
		return

	is_recording_shortcut = false
	config.font_size = int(font_size_spin.value)
	config.scrollback_limit = int(scrollback_spin.value)
	config.default_shell_override = shell_override_line.text.strip_edges()

	config.shortcut_keycode = temp_keycode
	config.shortcut_ctrl = temp_ctrl
	config.shortcut_alt = temp_alt
	config.shortcut_shift = temp_shift

	config.save_to_file()
	settings_saved.emit()
