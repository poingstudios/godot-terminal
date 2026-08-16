extends RefCounted

signal exited(session, exit_code: int)
signal title_changed(session, new_title: String)

const TerminalViewScript := preload("res://addons/godot_terminal/internal/ui/terminal_view.gd")

# ANSI terminal formatting escape codes
const ANSI_RESET := "\u001b[0m"
const ANSI_BOLD := "\u001b[1m"
const ANSI_UNDERLINE := "\u001b[4m"
const ANSI_WHITE := "\u001b[97m"
const ANSI_GRAY := "\u001b[90m"
const ANSI_YELLOW := "\u001b[93m"
const ANSI_CYAN := "\u001b[96m"

var id: String = ""
var title: String = "Terminal"
var command: String = ""
var args: PackedStringArray = []
var cwd: String = ""
var env_vars: Dictionary = {}

var pty: Object = null
var emulator: Object = null
var view: Control = null

func _init(p_id: String, p_command: String, p_args: PackedStringArray = [], p_cwd: String = "", p_env: Dictionary = {}) -> void:
	id = p_id
	command = p_command
	args = p_args
	cwd = p_cwd
	env_vars = p_env

	# ClassDB lookup for GDExtension classes
	if ClassDB.class_exists("TerminalPty"):
		pty = ClassDB.instantiate("TerminalPty")
	if ClassDB.class_exists("TerminalEmulator"):
		emulator = ClassDB.instantiate("TerminalEmulator")

	view = TerminalViewScript.new()
	view.set_session(emulator, pty)

	if pty != null:
		pty.process_exited.connect(_on_pty_process_exited)
	if emulator != null:
		emulator.title_changed.connect(_on_emulator_title_changed)
		if emulator.has_signal("write_to_pty"):
			emulator.write_to_pty.connect(_on_emulator_write_to_pty)

func start(cols: int = 80, rows: int = 24) -> bool:
	if pty == null or emulator == null:
		return false

	emulator.setup(cols, rows)
	var current_year: int = Time.get_date_dict_from_system()["year"]

	var banner := (
		ANSI_BOLD + ANSI_WHITE + "* Godot Terminal" + ANSI_RESET + " " +
		ANSI_GRAY + "© " + str(current_year) + " Poing Studios" + ANSI_RESET + "\r\n" +
		"  Support the project by leaving a " +
		ANSI_BOLD + ANSI_YELLOW + "* Star" + ANSI_RESET + " on GitHub:\r\n  " +
		ANSI_UNDERLINE + ANSI_CYAN + "https://github.com/poingstudios/godot-terminal" + ANSI_RESET +
		"\r\n\r\n"
	)
	emulator.feed_string(banner)
	var ok: bool = pty.open(command, args, cwd, env_vars, cols, rows)
	return ok

func _on_pty_process_exited(exit_code: int) -> void:
	exited.emit(self, exit_code)

func _on_emulator_title_changed(new_title: String) -> void:
	if not new_title.is_empty():
		title = new_title
		title_changed.emit(self, new_title)

func _on_emulator_write_to_pty(data: PackedByteArray) -> void:
	if pty != null:
		pty.write_data(data)

func close() -> void:
	if pty != null:
		pty.close()
	if view != null:
		view.queue_free()
