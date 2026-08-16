extends Control

signal terminal_resized(cols: int, rows: int)
signal link_clicked(type: String, target: String, line: int, col: int)
signal bell_triggered()
signal session_title_changed(new_title: String)
signal toggle_dock_requested()

const LinkMatcher := preload("res://addons/godot_terminal/internal/parser/terminal_link_matcher.gd")

var emulator: Object = null
var pty: Object = null

var link_matcher := LinkMatcher.new()

var font: Font = null
var font_size: int = 13
var char_width: float = 8.0
var line_height: float = 16.0
var ascent: float = 12.0

var cols: int = 80
var rows: int = 24

var scroll_offset: int = 0
var is_scrolled_to_bottom: bool = true

var selection_start: Vector2i = Vector2i(-1, -1)
var selection_end: Vector2i = Vector2i(-1, -1)
var is_selecting: bool = false

var cursor_blink_timer: float = 0.0
var is_cursor_blink_visible: bool = true

var hovered_link: Dictionary = {}
var custom_font_path: String = ""

func _ready() -> void:
	focus_mode = Control.FOCUS_ALL
	mouse_filter = Control.MOUSE_FILTER_STOP
	clip_contents = true
	_update_font_metrics()
	set_process(true)

func set_session(p_emulator, p_pty) -> void:
	emulator = p_emulator
	pty = p_pty

	if emulator != null:
		if not emulator.damage.is_connected(_on_emulator_damage):
			emulator.damage.connect(_on_emulator_damage)
		if not emulator.cursor_moved.is_connected(_on_emulator_cursor_moved):
			emulator.cursor_moved.connect(_on_emulator_cursor_moved)
		if not emulator.title_changed.is_connected(_on_emulator_title_changed):
			emulator.title_changed.connect(_on_emulator_title_changed)
		if not emulator.bell.is_connected(_on_emulator_bell):
			emulator.bell.connect(_on_emulator_bell)
		if emulator.has_signal("alt_screen_changed") and not emulator.alt_screen_changed.is_connected(_on_emulator_alt_screen_changed):
			emulator.alt_screen_changed.connect(_on_emulator_alt_screen_changed)

	_recalculate_dimensions()
	queue_redraw()

func _on_emulator_alt_screen_changed(_active: bool) -> void:
	scroll_offset = 0
	is_scrolled_to_bottom = true
	queue_redraw()

func _update_font_metrics() -> void:
	if font == null:
		var sys_font := SystemFont.new()
		sys_font.font_names = PackedStringArray([
			"SF Mono",
			"Menlo",
			"Monaco",
			"Cascadia Code",
			"Consolas",
			"DejaVu Sans Mono",
			"Courier New",
			"monospace"
		])
		sys_font.font_weight = 400
		sys_font.subpixel_positioning = TextServer.SUBPIXEL_POSITIONING_AUTO
		font = sys_font

	char_width = font.get_string_size("X", HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x
	line_height = font.get_height(font_size) + 2.0
	ascent = font.get_ascent(font_size)
	if char_width <= 0.0:
		char_width = 8.0
	if line_height <= 0.0:
		line_height = 16.0

var _pending_resize: bool = false
var _target_cols: int = 80
var _target_rows: int = 24

func _process(delta: float) -> void:
	if _pending_resize:
		_pending_resize = false
		if _target_cols != cols or _target_rows != rows:
			cols = _target_cols
			rows = _target_rows
			if emulator != null:
				emulator.set_size(cols, rows)
			if pty != null:
				pty.resize(cols, rows)
			terminal_resized.emit(cols, rows)
			queue_redraw()

	if pty != null:
		var incoming: PackedByteArray = pty.poll()
		if incoming.size() > 0 and emulator != null:
			emulator.feed_data(incoming)
			if is_scrolled_to_bottom:
				scroll_offset = 0
			queue_redraw()

	cursor_blink_timer += delta
	if cursor_blink_timer >= 0.5:
		cursor_blink_timer = 0.0
		is_cursor_blink_visible = not is_cursor_blink_visible
		queue_redraw()

func _notification(what: int) -> void:
	if what == NOTIFICATION_RESIZED:
		_recalculate_dimensions()
	elif what == NOTIFICATION_THEME_CHANGED:
		_update_font_metrics()
		queue_redraw()

const PADDING_LEFT: float = 6.0
const PADDING_TOP: float = 4.0

func _recalculate_dimensions() -> void:
	_update_font_metrics()
	if size.x <= 0 or size.y <= 0:
		return

	var available_w: float = maxf(0.0, size.x - PADDING_LEFT * 2.0)
	var available_h: float = maxf(0.0, size.y - PADDING_TOP * 2.0)

	var new_cols := int(available_w / char_width)
	var new_rows := int(available_h / line_height)

	new_cols = maxi(10, new_cols)
	new_rows = maxi(2, new_rows)

	if new_cols != cols or new_rows != rows:
		_target_cols = new_cols
		_target_rows = new_rows
		_pending_resize = true

func _on_emulator_damage(_rect: Rect2i) -> void:
	queue_redraw()

func _on_emulator_cursor_moved(_pos: Vector2i) -> void:
	queue_redraw()

func _on_emulator_title_changed(new_title: String) -> void:
	session_title_changed.emit(new_title)

func _on_emulator_bell() -> void:
	bell_triggered.emit()

func _draw() -> void:
	var bg_color := Color(0.08, 0.08, 0.09, 1.0)
	var default_fg := Color(0.88, 0.88, 0.88, 1.0)
	var selection_color := Color(0.16, 0.35, 0.58, 0.5)
	var cursor_color := Color(0.9, 0.9, 0.9, 0.85)

	draw_rect(Rect2(Vector2.ZERO, size), bg_color)

	if emulator == null or font == null:
		return

	var is_alt: bool = emulator.is_alt_screen()
	var scrollback_count: int = 0 if is_alt else emulator.get_scrollback_count()
	var effective_scroll: int = 0 if is_alt else clampi(scroll_offset, 0, scrollback_count)

	# Draw lines
	for r in range(rows):
		var y := PADDING_TOP + float(r) * line_height
		var line_y := y + ascent

		var chars: PackedInt32Array
		var fg_colors: Array
		var bg_colors: Array
		var flags: PackedInt32Array
		var widths: PackedInt32Array

		var v_idx := scrollback_count - effective_scroll + r

		if v_idx < scrollback_count and v_idx >= 0:
			chars = emulator.get_scrollback_line_chars(v_idx)
			fg_colors = emulator.get_scrollback_line_fg_colors(v_idx)
			bg_colors = emulator.get_scrollback_line_bg_colors(v_idx)
			flags = emulator.get_scrollback_line_flags(v_idx)
			widths = emulator.get_scrollback_line_widths(v_idx)
		else:
			var actual_row := v_idx - scrollback_count
			if actual_row >= 0 and actual_row < rows:
				chars = emulator.get_line_chars(actual_row)
				fg_colors = emulator.get_line_fg_colors(actual_row)
				bg_colors = emulator.get_line_bg_colors(actual_row)
				flags = emulator.get_line_flags(actual_row)
				widths = emulator.get_line_widths(actual_row)

		if chars.is_empty():
			continue

		var c_count := mini(cols, chars.size())
		var x := PADDING_LEFT

		for c in range(c_count):
			var ch: int = chars[c]
			var w: int = widths[c] if c < widths.size() else 1
			var cell_width := char_width * float(w)
			var cell_rect := Rect2(x, y, cell_width, line_height)

			var cell_bg: Color = bg_colors[c] if c < bg_colors.size() else Color.TRANSPARENT
			var cell_fg: Color = fg_colors[c] if c < fg_colors.size() else default_fg
			var flag_val: int = flags[c] if c < flags.size() else 0

			# Reverse video attribute
			if (flag_val & 8) != 0:
				var tmp := cell_bg
				cell_bg = cell_fg
				cell_fg = tmp

			if cell_bg.a > 0.0:
				draw_rect(cell_rect, cell_bg)

			# Selection highlight
			if _is_cell_selected(c, r, effective_scroll, scrollback_count):
				draw_rect(cell_rect, selection_color)

			if ch > 32:
				draw_char(font, Vector2(x, line_y), String.chr(ch), font_size, cell_fg)

			# Underline attribute
			if (flag_val & 4) != 0:
				draw_line(Vector2(x, y + line_height - 1), Vector2(x + cell_width, y + line_height - 1), cell_fg, 1.0)

			x += cell_width

	# Draw Cursor
	if effective_scroll == 0 and emulator.is_cursor_visible():
		var cursor_pos: Vector2i = emulator.get_cursor_pos()
		if cursor_pos.y >= 0 and cursor_pos.y < rows and cursor_pos.x >= 0 and cursor_pos.x < cols:
			var cx := PADDING_LEFT + float(cursor_pos.x) * char_width
			var cy := PADDING_TOP + float(cursor_pos.y) * line_height
			var c_shape: int = emulator.get_cursor_shape()

			if not emulator.is_cursor_blink() or is_cursor_blink_visible or has_focus():
				if c_shape == 0 or c_shape == 1: # Block
					draw_rect(Rect2(cx, cy, char_width, line_height), cursor_color, not has_focus())
				elif c_shape == 2: # Underline
					draw_line(Vector2(cx, cy + line_height - 2), Vector2(cx + char_width, cy + line_height - 2), cursor_color, 2.0)
				else: # Beam / Bar
					draw_line(Vector2(cx, cy), Vector2(cx, cy + line_height), cursor_color, 2.0)

func _is_cell_selected(c: int, r: int, _scroll: int, _total_sb: int) -> bool:
	if selection_start == Vector2i(-1, -1) or selection_end == Vector2i(-1, -1):
		return false

	var s_start := selection_start
	var s_end := selection_end

	if s_start.y > s_end.y or (s_start.y == s_end.y and s_start.x > s_end.x):
		var tmp := s_start
		s_start = s_end
		s_end = tmp

	if r < s_start.y or r > s_end.y:
		return false
	if r == s_start.y and r == s_end.y:
		return c >= s_start.x and c <= s_end.x
	if r == s_start.y:
		return c >= s_start.x
	if r == s_end.y:
		return c <= s_end.x
	return true

func _gui_input(event: InputEvent) -> void:
	if event is InputEventMouseButton:
		var mb := event as InputEventMouseButton
		if mb.button_index == MOUSE_BUTTON_LEFT:
			if mb.pressed:
				grab_focus()
				var cell_col := int((mb.position.x - PADDING_LEFT) / char_width)
				var cell_row := int((mb.position.y - PADDING_TOP) / line_height)
				selection_start = Vector2i(cell_col, cell_row)
				selection_end = selection_start
				is_selecting = true
				queue_redraw()
			else:
				is_selecting = false
				if selection_start == selection_end:
					_check_link_click(mb.position)
					selection_start = Vector2i(-1, -1)
					selection_end = Vector2i(-1, -1)
					queue_redraw()
		elif mb.button_index == MOUSE_BUTTON_WHEEL_UP:
			if mb.pressed:
				scroll_offset = mini(scroll_offset + 3, emulator.get_scrollback_count() if emulator else 0)
				is_scrolled_to_bottom = (scroll_offset == 0)
				queue_redraw()
		elif mb.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			if mb.pressed:
				scroll_offset = maxi(0, scroll_offset - 3)
				is_scrolled_to_bottom = (scroll_offset == 0)
				queue_redraw()
		elif mb.button_index == MOUSE_BUTTON_RIGHT and mb.pressed:
			_show_context_menu(mb.global_position)

	elif event is InputEventMouseMotion:
		var mm := event as InputEventMouseMotion
		if is_selecting:
			var cell_col := int((mm.position.x - PADDING_LEFT) / char_width)
			var cell_row := int((mm.position.y - PADDING_TOP) / line_height)
			selection_end = Vector2i(cell_col, cell_row)
			queue_redraw()

	elif event is InputEventKey:
		var ke := event as InputEventKey
		if ke.pressed:
			_handle_key_event(ke)
			accept_event()

func _handle_key_event(ke: InputEventKey) -> void:
	if pty == null or emulator == null:
		return

	# Toggle dock shortcut (Ctrl + ` or Cmd + `)
	var is_ctrl_or_cmd: bool = ke.ctrl_pressed or ke.meta_pressed
	var is_backtick: bool = (ke.keycode == KEY_QUOTELEFT or ke.physical_keycode == KEY_QUOTELEFT or ke.unicode == 96 or ke.keycode == 96 or ke.physical_keycode == 96 or ke.keycode == 4194306)
	if is_ctrl_or_cmd and is_backtick and not ke.alt_pressed and not ke.shift_pressed:
		toggle_dock_requested.emit()
		return

	# Copy / Paste handling
	var is_cmd_or_ctrl := ke.is_command_or_control_pressed()

	if is_cmd_or_ctrl and ke.keycode == KEY_C and has_selection():
		DisplayServer.clipboard_set(get_selected_text())
		return

	if is_cmd_or_ctrl and ke.keycode == KEY_V:
		var clip := DisplayServer.clipboard_get()
		if not clip.is_empty():
			pty.write_string(clip)
		return

	if is_cmd_or_ctrl and ke.keycode == KEY_L:
		# Clear buffer shortcut
		emulator.clear_scrollback()
		pty.write_data(PackedByteArray([12])) # Form Feed / Ctrl+L
		queue_redraw()
		return

	var encoded: PackedByteArray = emulator.encode_key(
		ke.keycode,
		ke.unicode,
		ke.shift_pressed,
		ke.ctrl_pressed,
		ke.alt_pressed,
		ke.meta_pressed
	)

	if encoded.size() > 0:
		pty.write_data(encoded)
		scroll_offset = 0
		is_scrolled_to_bottom = true

func _check_link_click(pos: Vector2) -> void:
	if emulator == null:
		return
	var row := int(pos.y / line_height)
	var col := int(pos.x / char_width)
	var line_str: String = emulator.get_line_text(row) if emulator != null else ""
	var links: Array = link_matcher.find_links(line_str)

	for link in links:
		var d := link as Dictionary
		var start_col: int = d["start"]
		var end_col: int = d["end"]
		if col >= start_col and col <= end_col:
			if d["type"] == "url":
				OS.shell_open(str(d["target"]))
			elif d["type"] == "file":
				link_clicked.emit(d["type"], d["target"], d["line"], d["col"])
			break

func _show_context_menu(global_pos: Vector2) -> void:
	var popup := PopupMenu.new()
	popup.add_item("Copy", 1)
	popup.add_item("Paste", 2)
	popup.add_separator()
	popup.add_item("Clear Buffer", 3)
	popup.add_item("Select All", 4)
	add_child(popup)
	popup.position = Vector2i(global_pos)
	popup.popup()

	popup.id_pressed.connect(func(id: int):
		if id == 1:
			if has_selection():
				DisplayServer.clipboard_set(get_selected_text())
		elif id == 2:
			var clip := DisplayServer.clipboard_get()
			if not clip.is_empty() and pty != null:
				pty.write_string(clip)
		elif id == 3:
			if emulator != null:
				emulator.clear_scrollback()
			if pty != null:
				pty.write_data(PackedByteArray([12]))
			queue_redraw()
		elif id == 4:
			select_all()
		popup.queue_free()
	)

func has_selection() -> bool:
	return selection_start != Vector2i(-1, -1) and selection_end != Vector2i(-1, -1) and selection_start != selection_end

func get_selected_text() -> String:
	if not has_selection() or emulator == null:
		return ""

	var s_start := selection_start
	var s_end := selection_end
	if s_start.y > s_end.y or (s_start.y == s_end.y and s_start.x > s_end.x):
		var tmp := s_start
		s_start = s_end
		s_end = tmp

	var result := ""
	for r in range(s_start.y, s_end.y + 1):
		var line_str: String = emulator.get_line_text(r)
		var from_col: int = s_start.x if r == s_start.y else 0
		var to_col: int = s_end.x if r == s_end.y else line_str.length() - 1
		from_col = clampi(from_col, 0, line_str.length())
		to_col = clampi(to_col, 0, line_str.length())
		if from_col <= to_col:
			result += line_str.substr(from_col, to_col - from_col + 1)
		if r < s_end.y:
			result += "\n"

	return result

func select_all() -> void:
	selection_start = Vector2i(0, 0)
	selection_end = Vector2i(cols - 1, rows - 1)
	queue_redraw()
