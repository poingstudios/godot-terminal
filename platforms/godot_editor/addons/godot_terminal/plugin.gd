@tool
extends EditorPlugin

const BottomPanelScript := preload("res://addons/godot_terminal/internal/ui/terminal_bottom_panel.gd")

var bottom_panel: Control = null
var bottom_panel_button: Button = null

func _enter_tree() -> void:
	bottom_panel = BottomPanelScript.new()
	bottom_panel.is_editor_context = true
	bottom_panel.script_open_requested.connect(_on_script_open_requested)
	bottom_panel.toggle_dock_requested.connect(_toggle_bottom_panel)

	bottom_panel_button = add_control_to_bottom_panel(bottom_panel, "Terminal")

func _exit_tree() -> void:
	if bottom_panel != null:
		remove_control_from_bottom_panel(bottom_panel)
		bottom_panel.queue_free()
		bottom_panel = null

func _unhandled_key_input(event: InputEvent) -> void:
	_process_shortcut(event)

func _shortcut_input(event: InputEvent) -> void:
	_process_shortcut(event)

func _input(event: InputEvent) -> void:
	_process_shortcut(event)

func _process_shortcut(event: InputEvent) -> void:
	if not (event is InputEventKey):
		return
	var ke := event as InputEventKey
	if not ke.pressed or ke.echo:
		return

	if bottom_panel == null or bottom_panel_button == null:
		return

	var config: Variant = bottom_panel.session_manager.config
	if _matches_shortcut(ke, config):
		_toggle_bottom_panel()
		get_viewport().set_input_as_handled()

func _matches_shortcut(ke: InputEventKey, config) -> bool:
	var is_ctrl_down: bool = ke.ctrl_pressed or ke.meta_pressed
	var ctrl_matched: bool = (is_ctrl_down == config.shortcut_ctrl)
	var alt_matched: bool = (ke.alt_pressed == config.shortcut_alt)
	var shift_matched: bool = (ke.shift_pressed == config.shortcut_shift)

	var is_backtick_target: bool = (config.shortcut_keycode == KEY_QUOTELEFT or config.shortcut_keycode == 96 or config.shortcut_keycode == 4194306)
	var key_matched: bool = false
	if is_backtick_target:
		key_matched = (ke.keycode == KEY_QUOTELEFT or ke.physical_keycode == KEY_QUOTELEFT or ke.unicode == 96 or ke.keycode == 96 or ke.physical_keycode == 96 or ke.keycode == 4194306)
	else:
		key_matched = (ke.keycode == config.shortcut_keycode or ke.physical_keycode == config.shortcut_keycode)

	return ctrl_matched and alt_matched and shift_matched and key_matched

func _toggle_bottom_panel() -> void:
	if bottom_panel == null or bottom_panel_button == null:
		return

	if bottom_panel.is_visible_in_tree():
		if bottom_panel_button.button_pressed:
			bottom_panel_button.button_pressed = false
		else:
			hide_bottom_panel()
	else:
		if bottom_panel.session_manager.get_session_count() == 0:
			bottom_panel._on_new_tab_requested()
		make_bottom_panel_item_visible(bottom_panel)
		if bottom_panel.session_manager.active_session != null:
			var s: Variant = bottom_panel.session_manager.active_session
			if s.view != null:
				s.view.grab_focus()

func _on_script_open_requested(path: String, line: int, col: int) -> void:
	if ResourceLoader.exists(path):
		var res := ResourceLoader.load(path)
		if res is Script:
			get_editor_interface().edit_script(res, line, col)
		elif res is PackedScene:
			get_editor_interface().open_scene_from_path(path)
		elif res != null:
			get_editor_interface().edit_resource(res)
