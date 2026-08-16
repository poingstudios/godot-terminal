extends HBoxContainer

signal new_tab_requested()
signal shell_selected(command: String, args: PackedStringArray)
signal split_horizontal_requested()
signal split_vertical_requested()
signal clear_buffer_requested()
signal kill_session_requested()
signal settings_requested()
signal tab_changed(index: int)
signal tab_closed(index: int)

var tab_bar := TabBar.new()
var shell_menu_button := MenuButton.new()
var new_tab_button := Button.new()
var split_h_button := Button.new()
var split_v_button := Button.new()
var clear_button := Button.new()
var kill_button := Button.new()
var settings_button := Button.new()

func _ready() -> void:
	tab_bar.tab_close_display_policy = TabBar.CLOSE_BUTTON_SHOW_ACTIVE_ONLY
	tab_bar.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	tab_bar.tab_changed.connect(_on_tab_changed)
	tab_bar.tab_close_pressed.connect(_on_tab_close_pressed)
	add_child(tab_bar)

	# Action buttons toolbar
	new_tab_button.text = "+"
	new_tab_button.tooltip_text = "New Terminal"
	new_tab_button.flat = true
	new_tab_button.pressed.connect(func(): new_tab_requested.emit())
	add_child(new_tab_button)

	shell_menu_button.text = "▼"
	shell_menu_button.tooltip_text = "Select Shell Profile"
	shell_menu_button.flat = true
	add_child(shell_menu_button)

	clear_button.text = "🗑"
	clear_button.tooltip_text = "Clear Terminal Buffer"
	clear_button.flat = true
	clear_button.pressed.connect(func(): clear_buffer_requested.emit())
	add_child(clear_button)

	settings_button.text = "⚙"
	settings_button.tooltip_text = "Terminal Settings"
	settings_button.flat = true
	settings_button.pressed.connect(func(): settings_requested.emit())
	add_child(settings_button)

func setup_shell_menu(shells: Array) -> void:
	var popup := shell_menu_button.get_popup()
	popup.clear()

	for i in range(shells.size()):
		var shell := shells[i] as Dictionary
		popup.add_item(str(shell["name"]), i)

	if not popup.id_pressed.is_connected(_on_shell_menu_item_selected):
		popup.id_pressed.connect(_on_shell_menu_item_selected.bind(shells))

func _on_shell_menu_item_selected(id: int, shells: Array) -> void:
	if id >= 0 and id < shells.size():
		var shell := shells[id] as Dictionary
		var args: PackedStringArray = []
		if shell.has("args") and shell["args"] is Array:
			for a in shell["args"]:
				args.append(str(a))
		shell_selected.emit(str(shell["path"]), args)

func add_tab(title: String) -> void:
	tab_bar.add_tab(title)

func set_tab_title(index: int, title: String) -> void:
	if index >= 0 and index < tab_bar.tab_count:
		tab_bar.set_tab_title(index, title)

func remove_tab(index: int) -> void:
	if index >= 0 and index < tab_bar.tab_count:
		tab_bar.remove_tab(index)

func set_current_tab(index: int) -> void:
	if index >= 0 and index < tab_bar.tab_count:
		tab_bar.current_tab = index

func _on_tab_changed(index: int) -> void:
	tab_changed.emit(index)

func _on_tab_close_pressed(index: int) -> void:
	tab_closed.emit(index)
