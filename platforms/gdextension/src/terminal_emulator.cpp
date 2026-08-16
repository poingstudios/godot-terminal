#include "terminal_emulator.h"
#include <godot_cpp/classes/global_constants.hpp>
#include <cstring>

namespace godot {

Color TerminalEmulator::_vterm_color_to_godot(const VTermColor &col) {
	if (VTERM_COLOR_IS_RGB(&col)) {
		return Color(col.rgb.red / 255.0f, col.rgb.green / 255.0f, col.rgb.blue / 255.0f, 1.0f);
	}
	if (VTERM_COLOR_IS_INDEXED(&col)) {
		// Standard 16 colors mapping fallback if not direct RGB
		static const Color ansi_16[16] = {
			Color(0.0f, 0.0f, 0.0f),       // 0 Black
			Color(0.8f, 0.0f, 0.0f),       // 1 Red
			Color(0.0f, 0.8f, 0.0f),       // 2 Green
			Color(0.8f, 0.8f, 0.0f),       // 3 Yellow
			Color(0.0f, 0.0f, 0.8f),       // 4 Blue
			Color(0.8f, 0.0f, 0.8f),       // 5 Magenta
			Color(0.0f, 0.8f, 0.8f),       // 6 Cyan
			Color(0.8f, 0.8f, 0.8f),       // 7 White
			Color(0.4f, 0.4f, 0.4f),       // 8 Bright Black
			Color(1.0f, 0.3f, 0.3f),       // 9 Bright Red
			Color(0.3f, 1.0f, 0.3f),       // 10 Bright Green
			Color(1.0f, 1.0f, 0.3f),       // 11 Bright Yellow
			Color(0.3f, 0.3f, 1.0f),       // 12 Bright Blue
			Color(1.0f, 0.3f, 1.0f),       // 13 Bright Magenta
			Color(0.3f, 1.0f, 1.0f),       // 14 Bright Cyan
			Color(1.0f, 1.0f, 1.0f)        // 15 Bright White
		};
		if (col.indexed.idx < 16) {
			return ansi_16[col.indexed.idx];
		}
	}
	return Color(0.85f, 0.85f, 0.85f, 1.0f);
}

int TerminalEmulator::_cb_damage(VTermRect rect, void *user) {
	TerminalEmulator *self = static_cast<TerminalEmulator *>(user);
	Rect2i r(rect.start_col, rect.start_row, rect.end_col - rect.start_col, rect.end_row - rect.start_row);
	self->emit_signal("damage", r);
	return 1;
}

int TerminalEmulator::_cb_moverect(VTermRect dest, VTermRect src, void *user) {
	TerminalEmulator *self = static_cast<TerminalEmulator *>(user);
	Rect2i r(dest.start_col, dest.start_row, dest.end_col - dest.start_col, dest.end_row - dest.start_row);
	self->emit_signal("damage", r);
	return 1;
}

int TerminalEmulator::_cb_movecursor(VTermPos pos, VTermPos oldpos, int visible, void *user) {
	TerminalEmulator *self = static_cast<TerminalEmulator *>(user);
	self->cursor_visible = (visible != 0);
	self->emit_signal("cursor_moved", Vector2i(pos.col, pos.row));
	return 1;
}

int TerminalEmulator::_cb_settermprop(VTermProp prop, VTermValue *val, void *user) {
	TerminalEmulator *self = static_cast<TerminalEmulator *>(user);
	switch (prop) {
		case VTERM_PROP_CURSORVISIBLE:
			self->cursor_visible = val->boolean;
			break;
		case VTERM_PROP_CURSORBLINK:
			self->cursor_blink = val->boolean;
			break;
		case VTERM_PROP_CURSORSHAPE:
			self->cursor_shape = val->number; // 1=Block, 2=Underline, 3=Bar
			break;
		case VTERM_PROP_ALTSCREEN:
			self->is_alt_screen_active = val->boolean;
			self->emit_signal("alt_screen_changed", self->is_alt_screen_active);
			break;
		case VTERM_PROP_TITLE:
			if (val->string.str) {
				self->window_title = String::utf8(val->string.str, static_cast<int>(val->string.len));
				self->emit_signal("title_changed", self->window_title);
			}
			break;
		default:
			break;
	}
	return 1;
}

int TerminalEmulator::_cb_bell(void *user) {
	TerminalEmulator *self = static_cast<TerminalEmulator *>(user);
	self->emit_signal("bell");
	return 1;
}

int TerminalEmulator::_cb_resize(int rows, int cols, void *user) {
	return 1;
}

int TerminalEmulator::_cb_sb_pushline(int cols, const VTermScreenCell *cells, void *user) {
	TerminalEmulator *self = static_cast<TerminalEmulator *>(user);
	if (self->is_alt_screen_active) {
		return 0;
	}
	std::vector<TermCell> line;
	line.reserve(cols);

	for (int c = 0; c < cols; ++c) {
		TermCell tc;
		tc.character = cells[c].chars[0] ? cells[c].chars[0] : U' ';
		tc.width = cells[c].width ? cells[c].width : 1;
		tc.flags = (cells[c].attrs.bold ? 1 : 0) |
				   (cells[c].attrs.italic ? 2 : 0) |
				   (cells[c].attrs.underline ? 4 : 0) |
				   (cells[c].attrs.reverse ? 8 : 0) |
				   (cells[c].attrs.strike ? 16 : 0);

		VTermColor fg = cells[c].fg;
		vterm_screen_convert_color_to_rgb(self->vts, &fg);
		tc.fg_color = _vterm_color_to_godot(fg);

		if (VTERM_COLOR_IS_DEFAULT_BG(&cells[c].bg)) {
			tc.bg_color = Color(0.0f, 0.0f, 0.0f, 0.0f);
		} else {
			VTermColor bg = cells[c].bg;
			vterm_screen_convert_color_to_rgb(self->vts, &bg);
			tc.bg_color = _vterm_color_to_godot(bg);
		}

		line.push_back(tc);
	}

	self->scrollback_buffer.push_back(line);
	if (static_cast<int>(self->scrollback_buffer.size()) > self->max_scrollback) {
		self->scrollback_buffer.pop_front();
	}

	return 1;
}

int TerminalEmulator::_cb_sb_popline(int cols, VTermScreenCell *cells, void *user) {
	TerminalEmulator *self = static_cast<TerminalEmulator *>(user);
	if (self->is_alt_screen_active || self->scrollback_buffer.empty()) {
		return 0;
	}

	const auto &line = self->scrollback_buffer.back();
	for (int c = 0; c < cols && c < static_cast<int>(line.size()); ++c) {
		cells[c].chars[0] = line[c].character;
		cells[c].chars[1] = 0;
		cells[c].width = line[c].width;
	}
	self->scrollback_buffer.pop_back();
	return 1;
}

void TerminalEmulator::_cb_output(const char *s, size_t len, void *user) {
	TerminalEmulator *self = static_cast<TerminalEmulator *>(user);
	if (len == 0 || !s) {
		return;
	}
	PackedByteArray out_data;
	out_data.resize(static_cast<int64_t>(len));
	std::memcpy(out_data.ptrw(), s, len);
	self->emit_signal("write_to_pty", out_data);
}

void TerminalEmulator::_bind_methods() {
	ClassDB::bind_method(D_METHOD("setup", "cols", "rows", "max_scrollback"), &TerminalEmulator::setup, DEFVAL(80), DEFVAL(24), DEFVAL(5000));
	ClassDB::bind_method(D_METHOD("feed_data", "data"), &TerminalEmulator::feed_data);
	ClassDB::bind_method(D_METHOD("feed_string", "text"), &TerminalEmulator::feed_string);
	ClassDB::bind_method(D_METHOD("set_size", "cols", "rows"), &TerminalEmulator::set_size);

	ClassDB::bind_method(D_METHOD("get_cols"), &TerminalEmulator::get_cols);
	ClassDB::bind_method(D_METHOD("get_rows"), &TerminalEmulator::get_rows);
	ClassDB::bind_method(D_METHOD("get_cursor_pos"), &TerminalEmulator::get_cursor_pos);
	ClassDB::bind_method(D_METHOD("is_cursor_visible"), &TerminalEmulator::is_cursor_visible);
	ClassDB::bind_method(D_METHOD("get_cursor_shape"), &TerminalEmulator::get_cursor_shape);
	ClassDB::bind_method(D_METHOD("is_cursor_blink"), &TerminalEmulator::is_cursor_blink);
	ClassDB::bind_method(D_METHOD("is_alt_screen"), &TerminalEmulator::is_alt_screen);
	ClassDB::bind_method(D_METHOD("get_window_title"), &TerminalEmulator::get_window_title);

	ClassDB::bind_method(D_METHOD("get_scrollback_count"), &TerminalEmulator::get_scrollback_count);
	ClassDB::bind_method(D_METHOD("clear_scrollback"), &TerminalEmulator::clear_scrollback);

	ClassDB::bind_method(D_METHOD("get_line_text", "row"), &TerminalEmulator::get_line_text);
	ClassDB::bind_method(D_METHOD("get_scrollback_line_text", "index"), &TerminalEmulator::get_scrollback_line_text);
	ClassDB::bind_method(D_METHOD("get_cell", "col", "row"), &TerminalEmulator::get_cell);

	ClassDB::bind_method(D_METHOD("get_line_chars", "row"), &TerminalEmulator::get_line_chars);
	ClassDB::bind_method(D_METHOD("get_line_fg_colors", "row"), &TerminalEmulator::get_line_fg_colors);
	ClassDB::bind_method(D_METHOD("get_line_bg_colors", "row"), &TerminalEmulator::get_line_bg_colors);
	ClassDB::bind_method(D_METHOD("get_line_flags", "row"), &TerminalEmulator::get_line_flags);
	ClassDB::bind_method(D_METHOD("get_line_widths", "row"), &TerminalEmulator::get_line_widths);

	ClassDB::bind_method(D_METHOD("get_scrollback_line_chars", "index"), &TerminalEmulator::get_scrollback_line_chars);
	ClassDB::bind_method(D_METHOD("get_scrollback_line_fg_colors", "index"), &TerminalEmulator::get_scrollback_line_fg_colors);
	ClassDB::bind_method(D_METHOD("get_scrollback_line_bg_colors", "index"), &TerminalEmulator::get_scrollback_line_bg_colors);
	ClassDB::bind_method(D_METHOD("get_scrollback_line_flags", "index"), &TerminalEmulator::get_scrollback_line_flags);
	ClassDB::bind_method(D_METHOD("get_scrollback_line_widths", "index"), &TerminalEmulator::get_scrollback_line_widths);

	ClassDB::bind_method(D_METHOD("get_all_lines"), &TerminalEmulator::get_all_lines);
	ClassDB::bind_method(D_METHOD("encode_key", "godot_key", "unicode", "shift", "ctrl", "alt", "meta"), &TerminalEmulator::encode_key, DEFVAL(false), DEFVAL(false), DEFVAL(false), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("encode_mouse", "button", "action", "col", "row", "shift", "ctrl", "alt"), &TerminalEmulator::encode_mouse, DEFVAL(false), DEFVAL(false), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("reset"), &TerminalEmulator::reset);
	ClassDB::bind_method(D_METHOD("reset_scroll_region"), &TerminalEmulator::reset_scroll_region);

	ADD_SIGNAL(MethodInfo("damage", PropertyInfo(Variant::RECT2I, "rect")));
	ADD_SIGNAL(MethodInfo("cursor_moved", PropertyInfo(Variant::VECTOR2I, "pos")));
	ADD_SIGNAL(MethodInfo("alt_screen_changed", PropertyInfo(Variant::BOOL, "active")));
	ADD_SIGNAL(MethodInfo("title_changed", PropertyInfo(Variant::STRING, "title")));
	ADD_SIGNAL(MethodInfo("bell"));
	ADD_SIGNAL(MethodInfo("write_to_pty", PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data")));
}

TerminalEmulator::TerminalEmulator() {
	setup(80, 24, 5000);
}

TerminalEmulator::~TerminalEmulator() {
	std::lock_guard<std::recursive_mutex> lock(vt_mutex);
	if (vt) {
		vterm_free(vt);
		vt = nullptr;
		vts = nullptr;
	}
}

void TerminalEmulator::setup(int p_cols, int p_rows, int p_max_scrollback) {
	std::lock_guard<std::recursive_mutex> lock(vt_mutex);

	if (vt) {
		vterm_free(vt);
	}

	cols = p_cols > 0 ? p_cols : 80;
	rows = p_rows > 0 ? p_rows : 24;
	max_scrollback = p_max_scrollback >= 0 ? p_max_scrollback : 5000;

	vt = vterm_new(rows, cols);
	vterm_set_utf8(vt, 1);

	vterm_output_set_callback(vt, &TerminalEmulator::_cb_output, this);

	vts = vterm_obtain_screen(vt);
	vterm_screen_enable_altscreen(vts, 1);
	vterm_screen_enable_reflow(vts, 0);

	static VTermScreenCallbacks cb = {
		&TerminalEmulator::_cb_damage,
		&TerminalEmulator::_cb_moverect,
		&TerminalEmulator::_cb_movecursor,
		&TerminalEmulator::_cb_settermprop,
		&TerminalEmulator::_cb_bell,
		&TerminalEmulator::_cb_resize,
		&TerminalEmulator::_cb_sb_pushline,
		&TerminalEmulator::_cb_sb_popline
	};

	vterm_screen_set_callbacks(vts, &cb, this);
	vterm_screen_reset(vts, 1);
}

void TerminalEmulator::feed_data(const PackedByteArray &data) {
	if (data.is_empty()) {
		return;
	}
	std::lock_guard<std::recursive_mutex> lock(vt_mutex);
	if (!vt || !vts) {
		return;
	}
	vterm_input_write(vt, reinterpret_cast<const char *>(data.ptr()), static_cast<size_t>(data.size()));
	vterm_screen_flush_damage(vts);
}

void TerminalEmulator::feed_string(const String &text) {
	if (text.is_empty()) {
		return;
	}
	CharString utf8 = text.utf8();
	std::lock_guard<std::recursive_mutex> lock(vt_mutex);
	if (!vt || !vts) {
		return;
	}
	vterm_input_write(vt, utf8.get_data(), static_cast<size_t>(utf8.length()));
	vterm_screen_flush_damage(vts);
}

void TerminalEmulator::set_size(int p_cols, int p_rows) {
	if (p_cols <= 0 || p_rows <= 0) {
		return;
	}
	std::lock_guard<std::recursive_mutex> lock(vt_mutex);
	if (cols == p_cols && rows == p_rows) {
		return;
	}
	cols = p_cols;
	rows = p_rows;
	if (vt && vts) {
		vterm_set_size(vt, rows, cols);
		vterm_input_write(vt, "\x1b[r", 3);
		vterm_screen_flush_damage(vts);
	}
}

Vector2i TerminalEmulator::get_cursor_pos() const {
	std::lock_guard<std::recursive_mutex> lock(vt_mutex);
	if (!vt) {
		return Vector2i(0, 0);
	}
	VTermPos pos;
	vterm_state_get_cursorpos(vterm_obtain_state(vt), &pos);
	return Vector2i(pos.col, pos.row);
}

int TerminalEmulator::get_scrollback_count() const {
	std::lock_guard<std::recursive_mutex> lock(vt_mutex);
	return static_cast<int>(scrollback_buffer.size());
}

void TerminalEmulator::clear_scrollback() {
	std::lock_guard<std::recursive_mutex> lock(vt_mutex);
	scrollback_buffer.clear();
}

String TerminalEmulator::get_line_text(int row) const {
	std::lock_guard<std::recursive_mutex> lock(vt_mutex);
	if (!vts || row < 0 || row >= rows) {
		return String();
	}

	String result;
	for (int c = 0; c < cols; ++c) {
		VTermPos pos = { row, c };
		VTermScreenCell cell;
		vterm_screen_get_cell(vts, pos, &cell);
		if (cell.chars[0]) {
			result += String::chr(cell.chars[0]);
		} else {
			result += " ";
		}
	}
	return result;
}

String TerminalEmulator::get_scrollback_line_text(int index) const {
	std::lock_guard<std::recursive_mutex> lock(vt_mutex);
	if (index < 0 || index >= static_cast<int>(scrollback_buffer.size())) {
		return String();
	}

	const auto &line = scrollback_buffer[index];
	String result;
	for (const auto &cell : line) {
		result += String::chr(cell.character);
	}
	return result;
}

Dictionary TerminalEmulator::get_cell(int col, int row) const {
	Dictionary d;
	std::lock_guard<std::recursive_mutex> lock(vt_mutex);
	if (!vts || col < 0 || col >= cols || row < 0 || row >= rows) {
		return d;
	}

	VTermPos pos = { row, col };
	VTermScreenCell cell;
	vterm_screen_get_cell(vts, pos, &cell);

	d["char"] = cell.chars[0] ? String::chr(cell.chars[0]) : " ";
	d["width"] = cell.width ? cell.width : 1;
	d["bold"] = cell.attrs.bold != 0;
	d["italic"] = cell.attrs.italic != 0;
	d["underline"] = cell.attrs.underline != 0;
	d["reverse"] = cell.attrs.reverse != 0;
	d["strike"] = cell.attrs.strike != 0;

	VTermColor fg = cell.fg;
	vterm_screen_convert_color_to_rgb(vts, &fg);
	d["fg"] = _vterm_color_to_godot(fg);

	if (VTERM_COLOR_IS_DEFAULT_BG(&cell.bg)) {
		d["bg"] = Color(0.0f, 0.0f, 0.0f, 0.0f);
	} else {
		VTermColor bg = cell.bg;
		vterm_screen_convert_color_to_rgb(vts, &bg);
		d["bg"] = _vterm_color_to_godot(bg);
	}

	return d;
}

PackedInt32Array TerminalEmulator::get_line_chars(int row) const {
	PackedInt32Array chars;
	std::lock_guard<std::recursive_mutex> lock(vt_mutex);
	if (!vts || row < 0 || row >= rows || cols <= 0) {
		return chars;
	}

	chars.resize(cols);
	int32_t *w = chars.ptrw();
	for (int c = 0; c < cols; ++c) {
		VTermPos pos = { row, c };
		VTermScreenCell cell;
		vterm_screen_get_cell(vts, pos, &cell);
		w[c] = cell.chars[0] ? static_cast<int32_t>(cell.chars[0]) : 32;
	}
	return chars;
}

Array TerminalEmulator::get_line_fg_colors(int row) const {
	Array colors;
	std::lock_guard<std::recursive_mutex> lock(vt_mutex);
	if (!vts || row < 0 || row >= rows || cols <= 0) {
		return colors;
	}

	colors.resize(cols);
	for (int c = 0; c < cols; ++c) {
		VTermPos pos = { row, c };
		VTermScreenCell cell;
		vterm_screen_get_cell(vts, pos, &cell);
		VTermColor fg = cell.fg;
		vterm_screen_convert_color_to_rgb(vts, &fg);
		colors[c] = _vterm_color_to_godot(fg);
	}
	return colors;
}

Array TerminalEmulator::get_line_bg_colors(int row) const {
	Array colors;
	std::lock_guard<std::recursive_mutex> lock(vt_mutex);
	if (!vts || row < 0 || row >= rows || cols <= 0) {
		return colors;
	}

	colors.resize(cols);
	for (int c = 0; c < cols; ++c) {
		VTermPos pos = { row, c };
		VTermScreenCell cell;
		vterm_screen_get_cell(vts, pos, &cell);
		if (VTERM_COLOR_IS_DEFAULT_BG(&cell.bg)) {
			colors[c] = Color(0.0f, 0.0f, 0.0f, 0.0f);
		} else {
			VTermColor bg = cell.bg;
			vterm_screen_convert_color_to_rgb(vts, &bg);
			colors[c] = _vterm_color_to_godot(bg);
		}
	}
	return colors;
}

PackedInt32Array TerminalEmulator::get_line_flags(int row) const {
	PackedInt32Array flags;
	std::lock_guard<std::recursive_mutex> lock(vt_mutex);
	if (!vts || row < 0 || row >= rows || cols <= 0) {
		return flags;
	}

	flags.resize(cols);
	int32_t *w = flags.ptrw();
	for (int c = 0; c < cols; ++c) {
		VTermPos pos = { row, c };
		VTermScreenCell cell;
		vterm_screen_get_cell(vts, pos, &cell);
		w[c] = (cell.attrs.bold ? 1 : 0) |
			   (cell.attrs.italic ? 2 : 0) |
			   (cell.attrs.underline ? 4 : 0) |
			   (cell.attrs.reverse ? 8 : 0) |
			   (cell.attrs.strike ? 16 : 0);
	}
	return flags;
}

PackedInt32Array TerminalEmulator::get_line_widths(int row) const {
	PackedInt32Array widths;
	std::lock_guard<std::recursive_mutex> lock(vt_mutex);
	if (!vts || row < 0 || row >= rows || cols <= 0) {
		return widths;
	}

	widths.resize(cols);
	int32_t *w = widths.ptrw();
	for (int c = 0; c < cols; ++c) {
		VTermPos pos = { row, c };
		VTermScreenCell cell;
		vterm_screen_get_cell(vts, pos, &cell);
		w[c] = cell.width ? cell.width : 1;
	}
	return widths;
}

PackedInt32Array TerminalEmulator::get_scrollback_line_chars(int index) const {
	PackedInt32Array chars;
	std::lock_guard<std::recursive_mutex> lock(vt_mutex);
	if (index < 0 || index >= static_cast<int>(scrollback_buffer.size())) {
		return chars;
	}

	const auto &line = scrollback_buffer[index];
	chars.resize(static_cast<int64_t>(line.size()));
	int32_t *w = chars.ptrw();
	for (size_t c = 0; c < line.size(); ++c) {
		w[c] = static_cast<int32_t>(line[c].character);
	}
	return chars;
}

Array TerminalEmulator::get_scrollback_line_fg_colors(int index) const {
	Array colors;
	std::lock_guard<std::recursive_mutex> lock(vt_mutex);
	if (index < 0 || index >= static_cast<int>(scrollback_buffer.size())) {
		return colors;
	}

	const auto &line = scrollback_buffer[index];
	colors.resize(static_cast<int64_t>(line.size()));
	for (size_t c = 0; c < line.size(); ++c) {
		colors[static_cast<int>(c)] = line[c].fg_color;
	}
	return colors;
}

Array TerminalEmulator::get_scrollback_line_bg_colors(int index) const {
	Array colors;
	std::lock_guard<std::recursive_mutex> lock(vt_mutex);
	if (index < 0 || index >= static_cast<int>(scrollback_buffer.size())) {
		return colors;
	}

	const auto &line = scrollback_buffer[index];
	colors.resize(static_cast<int64_t>(line.size()));
	for (size_t c = 0; c < line.size(); ++c) {
		colors[static_cast<int>(c)] = line[c].bg_color;
	}
	return colors;
}

PackedInt32Array TerminalEmulator::get_scrollback_line_flags(int index) const {
	PackedInt32Array flags;
	std::lock_guard<std::recursive_mutex> lock(vt_mutex);
	if (index < 0 || index >= static_cast<int>(scrollback_buffer.size())) {
		return flags;
	}

	const auto &line = scrollback_buffer[index];
	flags.resize(static_cast<int64_t>(line.size()));
	int32_t *w = flags.ptrw();
	for (size_t c = 0; c < line.size(); ++c) {
		w[c] = line[c].flags;
	}
	return flags;
}

PackedInt32Array TerminalEmulator::get_scrollback_line_widths(int index) const {
	PackedInt32Array widths;
	std::lock_guard<std::recursive_mutex> lock(vt_mutex);
	if (index < 0 || index >= static_cast<int>(scrollback_buffer.size())) {
		return widths;
	}

	const auto &line = scrollback_buffer[index];
	widths.resize(static_cast<int64_t>(line.size()));
	int32_t *w = widths.ptrw();
	for (size_t c = 0; c < line.size(); ++c) {
		w[c] = line[c].width;
	}
	return widths;
}

PackedStringArray TerminalEmulator::get_all_lines() const {
	PackedStringArray lines;
	std::lock_guard<std::recursive_mutex> lock(vt_mutex);
	for (size_t i = 0; i < scrollback_buffer.size(); ++i) {
		const auto &line = scrollback_buffer[i];
		String result;
		for (const auto &cell : line) {
			result += String::chr(cell.character);
		}
		lines.push_back(result);
	}
	for (int r = 0; r < rows; ++r) {
		String result;
		for (int c = 0; c < cols; ++c) {
			VTermPos pos = { r, c };
			VTermScreenCell cell;
			vterm_screen_get_cell(vts, pos, &cell);
			if (cell.chars[0]) {
				result += String::chr(cell.chars[0]);
			} else {
				result += " ";
			}
		}
		lines.push_back(result);
	}
	return lines;
}

PackedByteArray TerminalEmulator::encode_key(int godot_key, int unicode, bool shift, bool ctrl, bool alt, bool meta) {
	PackedByteArray result;
	if (!vt) {
		return result;
	}

	VTermModifier mod = VTERM_MOD_NONE;
	if (shift) mod = static_cast<VTermModifier>(mod | VTERM_MOD_SHIFT);
	if (ctrl) mod = static_cast<VTermModifier>(mod | VTERM_MOD_CTRL);
	if (alt) mod = static_cast<VTermModifier>(mod | VTERM_MOD_ALT);

	VTermKey vkey = VTERM_KEY_NONE;
	switch (godot_key) {
		case KEY_ENTER:
		case KEY_KP_ENTER:
			vkey = VTERM_KEY_ENTER;
			break;
		case KEY_TAB: vkey = VTERM_KEY_TAB; break;
		case KEY_BACKSPACE: vkey = VTERM_KEY_BACKSPACE; break;
		case KEY_ESCAPE: vkey = VTERM_KEY_ESCAPE; break;
		case KEY_UP: vkey = VTERM_KEY_UP; break;
		case KEY_DOWN: vkey = VTERM_KEY_DOWN; break;
		case KEY_LEFT: vkey = VTERM_KEY_LEFT; break;
		case KEY_RIGHT: vkey = VTERM_KEY_RIGHT; break;
		case KEY_INSERT: vkey = VTERM_KEY_INS; break;
		case KEY_DELETE: vkey = VTERM_KEY_DEL; break;
		case KEY_HOME: vkey = VTERM_KEY_HOME; break;
		case KEY_END: vkey = VTERM_KEY_END; break;
		case KEY_PAGEUP: vkey = VTERM_KEY_PAGEUP; break;
		case KEY_PAGEDOWN: vkey = VTERM_KEY_PAGEDOWN; break;
		case KEY_F1: vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(1)); break;
		case KEY_F2: vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(2)); break;
		case KEY_F3: vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(3)); break;
		case KEY_F4: vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(4)); break;
		case KEY_F5: vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(5)); break;
		case KEY_F6: vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(6)); break;
		case KEY_F7: vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(7)); break;
		case KEY_F8: vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(8)); break;
		case KEY_F9: vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(9)); break;
		case KEY_F10: vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(10)); break;
		case KEY_F11: vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(11)); break;
		case KEY_F12: vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(12)); break;
		default:
			break;
	}

	std::lock_guard<std::recursive_mutex> lock(vt_mutex);

	if (vkey != VTERM_KEY_NONE) {
		vterm_keyboard_key(vt, vkey, mod);
	} else if (unicode > 0) {
		vterm_keyboard_unichar(vt, static_cast<uint32_t>(unicode), mod);
	} else if (ctrl && godot_key >= KEY_A && godot_key <= KEY_Z) {
		uint32_t c = static_cast<uint32_t>(godot_key - KEY_A + 1);
		vterm_keyboard_unichar(vt, c, VTERM_MOD_NONE);
	}

	char out_buf[256];
	size_t len = vterm_output_get_buffer_current(vt);
	if (len > 0) {
		len = vterm_output_read(vt, out_buf, sizeof(out_buf));
		result.resize(static_cast<int64_t>(len));
		std::memcpy(result.ptrw(), out_buf, len);
	}

	return result;
}

PackedByteArray TerminalEmulator::encode_mouse(int button, int action, int col, int row, bool shift, bool ctrl, bool alt) {
	PackedByteArray result;
	if (!vt) {
		return result;
	}

	VTermModifier mod = VTERM_MOD_NONE;
	if (shift) mod = static_cast<VTermModifier>(mod | VTERM_MOD_SHIFT);
	if (ctrl) mod = static_cast<VTermModifier>(mod | VTERM_MOD_CTRL);
	if (alt) mod = static_cast<VTermModifier>(mod | VTERM_MOD_ALT);

	std::lock_guard<std::recursive_mutex> lock(vt_mutex);
	vterm_mouse_move(vt, row, col, mod);
	if (button > 0) {
		vterm_mouse_button(vt, button, action != 0, mod);
	}

	char out_buf[256];
	size_t len = vterm_output_get_buffer_current(vt);
	if (len > 0) {
		len = vterm_output_read(vt, out_buf, sizeof(out_buf));
		result.resize(static_cast<int64_t>(len));
		std::memcpy(result.ptrw(), out_buf, len);
	}

	return result;
}

void TerminalEmulator::reset() {
	std::lock_guard<std::recursive_mutex> lock(vt_mutex);
	if (vts) {
		vterm_screen_reset(vts, 1);
	}
	scrollback_buffer.clear();
}

void TerminalEmulator::reset_scroll_region() {
	std::lock_guard<std::recursive_mutex> lock(vt_mutex);
	if (vt && vts) {
		vterm_input_write(vt, "\x1b[r", 3);
		vterm_screen_flush_damage(vts);
	}
}

} // namespace godot
