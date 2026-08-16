extends VBoxContainer

signal script_open_requested(path: String, line: int, col: int)
signal toggle_dock_requested()

const SessionManagerScript := preload("res://addons/godot_terminal/internal/core/terminal_session_manager.gd")
const TabBarScript := preload("res://addons/godot_terminal/internal/ui/terminal_tab_bar.gd")
const SettingsDialogScript := preload("res://addons/godot_terminal/internal/ui/terminal_settings_dialog.gd")

var session_manager := SessionManagerScript.new()
var tab_bar := TabBarScript.new()
var settings_dialog := SettingsDialogScript.new()

var view_container := MarginContainer.new()
var is_editor_context: bool = false

func _ready() -> void:
	size_flags_horizontal = Control.SIZE_EXPAND_FILL
	size_flags_vertical = Control.SIZE_EXPAND_FILL
	custom_minimum_size = Vector2(0, 220)

	# Add TabBar at top
	add_child(tab_bar)

	# Add View Container
	view_container.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	view_container.size_flags_vertical = Control.SIZE_EXPAND_FILL
	add_child(view_container)

	# Add Settings Dialog
	add_child(settings_dialog)
	settings_dialog.load_values(session_manager.config)

	# Wire signals
	tab_bar.setup_shell_menu(session_manager.config.get_available_shells())
	tab_bar.new_tab_requested.connect(_on_new_tab_requested)
	tab_bar.shell_selected.connect(_on_shell_selected)
	tab_bar.split_horizontal_requested.connect(_on_split_h_requested)
	tab_bar.split_vertical_requested.connect(_on_split_v_requested)
	tab_bar.clear_buffer_requested.connect(_on_clear_buffer_requested)
	tab_bar.kill_session_requested.connect(_on_kill_session_requested)
	tab_bar.settings_requested.connect(_on_settings_requested)
	tab_bar.tab_changed.connect(_on_tab_changed)
	tab_bar.tab_closed.connect(_on_tab_closed)

	session_manager.session_created.connect(_on_session_created)
	session_manager.session_closed.connect(_on_session_closed)
	session_manager.active_session_changed.connect(_on_active_session_changed)

	settings_dialog.settings_saved.connect(_on_settings_saved)

	# Automatically spawn the default session if none exists
	if session_manager.get_session_count() == 0:
		_on_new_tab_requested()

func _on_new_tab_requested() -> void:
	var session: Variant = session_manager.create_session()
	if session != null:
		_start_session_with_view_size(session)

func _on_shell_selected(command: String, args: PackedStringArray) -> void:
	var session: Variant = session_manager.create_session(command, args)
	if session != null:
		_start_session_with_view_size(session)

func _start_session_with_view_size(session) -> void:
	var w: float = maxf(0.0, view_container.size.x - 12.0)
	var h: float = maxf(0.0, view_container.size.y - 8.0)
	if w > 50.0 and h > 50.0 and session.view != null:
		session.view._update_font_metrics()
		var c := maxi(10, int(w / session.view.char_width))
		var r := maxi(2, int(h / session.view.line_height))
		session.start(c, r)
	else:
		session.start(80, 24)

func _on_session_created(session) -> void:
	tab_bar.add_tab(session.title)
	session.title_changed.connect(_on_session_title_changed)
	if session.view != null:
		session.view.link_clicked.connect(_on_link_clicked)
		session.view.toggle_dock_requested.connect(func(): toggle_dock_requested.emit())

func _notification(what: int) -> void:
	if what == NOTIFICATION_VISIBILITY_CHANGED:
		if is_visible_in_tree() and session_manager.get_session_count() == 0:
			_on_new_tab_requested()

func _on_session_closed(_session, idx: int) -> void:
	if idx != -1:
		tab_bar.remove_tab(idx)
	if session_manager.get_session_count() == 0:
		toggle_dock_requested.emit()

func _on_active_session_changed(session) -> void:
	for child in view_container.get_children():
		view_container.remove_child(child)

	if session != null and session.view != null:
		view_container.add_child(session.view)
		session.view.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		session.view.size_flags_vertical = Control.SIZE_EXPAND_FILL
		session.view.grab_focus()

	var idx := session_manager.sessions.find(session)
	if idx != -1:
		tab_bar.set_current_tab(idx)

func _on_session_title_changed(session, new_title: String) -> void:
	var idx := session_manager.sessions.find(session)
	if idx != -1:
		tab_bar.set_tab_title(idx, new_title)

func _on_tab_changed(index: int) -> void:
	if index >= 0 and index < session_manager.sessions.size():
		session_manager.set_active_session(session_manager.sessions[index])

func _on_tab_closed(index: int) -> void:
	if index >= 0 and index < session_manager.sessions.size():
		session_manager.close_session(session_manager.sessions[index])

func _on_split_h_requested() -> void:
	# Horizontal split with a new shell session
	var session: Variant = session_manager.create_session()
	if session != null:
		session.start()

func _on_split_v_requested() -> void:
	# Vertical split with a new shell session
	var session: Variant = session_manager.create_session()
	if session != null:
		session.start()

func _on_clear_buffer_requested() -> void:
	if session_manager.active_session != null:
		var s: Variant = session_manager.active_session
		if s.emulator != null:
			s.emulator.clear_scrollback()
		if s.pty != null:
			s.pty.write_data(PackedByteArray([12])) # Ctrl+L
		if s.view != null:
			s.view.queue_redraw()

func _on_kill_session_requested() -> void:
	session_manager.close_active_session()

func _on_settings_requested() -> void:
	settings_dialog.load_values(session_manager.config)
	settings_dialog.popup_centered()

func _on_settings_saved() -> void:
	for s in session_manager.sessions:
		if s.view != null:
			s.view.font_size = session_manager.config.font_size
			s.view._recalculate_dimensions()
			s.view.queue_redraw()

func _on_link_clicked(type: String, target: String, line: int, col: int) -> void:
	if type == "file":
		script_open_requested.emit(target, line, col)
