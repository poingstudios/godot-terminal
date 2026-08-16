#ifndef TERMINAL_EMULATOR_H
#define TERMINAL_EMULATOR_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/rect2i.hpp>

#include "vterm.h"

#include <vector>
#include <deque>
#include <string>
#include <mutex>

namespace godot {

struct TermCell {
	char32_t character = U' ';
	Color fg_color = Color(0.85, 0.85, 0.85, 1.0);
	Color bg_color = Color(0.0, 0.0, 0.0, 0.0);
	uint8_t flags = 0; // bold, italic, underline, reverse, etc.
	uint8_t width = 1;
};

class TerminalEmulator : public RefCounted {
	GDCLASS(TerminalEmulator, RefCounted);

private:
	VTerm *vt = nullptr;
	VTermScreen *vts = nullptr;
	int cols = 80;
	int rows = 24;
	int max_scrollback = 5000;

	std::deque<std::vector<TermCell>> scrollback_buffer;
	mutable std::recursive_mutex vt_mutex;

	bool is_alt_screen_active = false;
	bool cursor_visible = true;
	int cursor_shape = 0; // 0=Block, 1=Beam, 2=Underline
	bool cursor_blink = true;
	String window_title;

	// Callbacks from libvterm
	static int _cb_damage(VTermRect rect, void *user);
	static int _cb_moverect(VTermRect dest, VTermRect src, void *user);
	static int _cb_movecursor(VTermPos pos, VTermPos oldpos, int visible, void *user);
	static int _cb_settermprop(VTermProp prop, VTermValue *val, void *user);
	static int _cb_bell(void *user);
	static int _cb_resize(int rows, int cols, void *user);
	static int _cb_sb_pushline(int cols, const VTermScreenCell *cells, void *user);
	static int _cb_sb_popline(int cols, VTermScreenCell *cells, void *user);
	static void _cb_output(const char *s, size_t len, void *user);

	static Color _vterm_color_to_godot(const VTermColor &col);

protected:
	static void _bind_methods();

public:
	TerminalEmulator();
	virtual ~TerminalEmulator() override;

	void setup(int p_cols, int p_rows, int p_max_scrollback = 5000);
	void feed_data(const PackedByteArray &data);
	void feed_string(const String &text);
	void set_size(int p_cols, int p_rows);

	int get_cols() const { return cols; }
	int get_rows() const { return rows; }
	Vector2i get_cursor_pos() const;
	bool is_cursor_visible() const { return cursor_visible; }
	int get_cursor_shape() const { return cursor_shape; }
	bool is_cursor_blink() const { return cursor_blink; }
	bool is_alt_screen() const { return is_alt_screen_active; }
	String get_window_title() const { return window_title; }

	int get_scrollback_count() const;
	void clear_scrollback();

	// Line and cell querying
	String get_line_text(int row) const;
	String get_scrollback_line_text(int index) const;
	Dictionary get_cell(int col, int row) const;

	// High-performance line queries for renderer
	PackedInt32Array get_line_chars(int row) const;
	Array get_line_fg_colors(int row) const;
	Array get_line_bg_colors(int row) const;
	PackedInt32Array get_line_flags(int row) const;
	PackedInt32Array get_line_widths(int row) const;

	PackedInt32Array get_scrollback_line_chars(int index) const;
	Array get_scrollback_line_fg_colors(int index) const;
	Array get_scrollback_line_bg_colors(int index) const;
	PackedInt32Array get_scrollback_line_flags(int index) const;
	PackedInt32Array get_scrollback_line_widths(int index) const;

	PackedStringArray get_all_lines() const;

	// Key encoding to bytes
	PackedByteArray encode_key(int godot_key, int unicode, bool shift, bool ctrl, bool alt, bool meta);
	PackedByteArray encode_mouse(int button, int action, int col, int row, bool shift, bool ctrl, bool alt);

	void reset();
	void reset_scroll_region();
};

} // namespace godot

#endif // TERMINAL_EMULATOR_H
