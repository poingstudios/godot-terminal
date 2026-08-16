extends RefCounted

signal session_created(session)
signal session_closed(session, index: int)
signal active_session_changed(session)

const TerminalSessionScript := preload("res://addons/godot_terminal/internal/core/terminal_session.gd")
const TerminalConfigScript := preload("res://addons/godot_terminal/internal/core/terminal_config.gd")

var config := TerminalConfigScript.new()
var sessions: Array = []
var active_session: RefCounted = null
var session_counter: int = 0

func _init() -> void:
	config.load_from_file()

func create_session(command: String = "", args: PackedStringArray = [], cwd: String = "", env_vars: Dictionary = {}) -> Object:
	if command.is_empty():
		command = config.get_default_shell()
	if cwd.is_empty():
		cwd = config.get_default_working_directory()

	session_counter += 1
	var s_id := "session_" + str(session_counter)
	var session := TerminalSessionScript.new(s_id, command, args, cwd, env_vars)

	sessions.append(session)
	_update_session_titles()

	session.exited.connect(_on_session_exited)
	session.title_changed.connect(_on_session_title_changed)

	session_created.emit(session)
	set_active_session(session)
	return session

func set_active_session(session) -> void:
	if active_session == session:
		return
	active_session = session
	active_session_changed.emit(active_session)

func close_session(session) -> void:
	if not sessions.has(session):
		return

	var idx := sessions.find(session)
	sessions.erase(session)
	session_closed.emit(session, idx)
	session.close()

	if sessions.is_empty():
		session_counter = 0
		active_session = null
	else:
		_update_session_titles()
		if active_session == session:
			var next_idx := mini(idx, sessions.size() - 1)
			set_active_session(sessions[next_idx])

func _update_session_titles() -> void:
	for i in range(sessions.size()):
		var s: Variant = sessions[i]
		var shell_name: String = s.command.get_file()
		s.title = str(i + 1) + ": " + shell_name
		s.title_changed.emit(s, s.title)

func close_active_session() -> void:
	if active_session != null:
		close_session(active_session)

func _on_session_exited(session, _exit_code: int) -> void:
	close_session(session)

func _on_session_title_changed(_session, _title: String) -> void:
	pass

func get_session_count() -> int:
	return sessions.size()
