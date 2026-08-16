#include "terminal_pty.h"

#if defined(_WIN32)
#include "pty/pty_driver_windows.h"
#else
#include "pty/pty_driver_posix.h"
#endif

#include <chrono>

namespace godot {

void TerminalPty::_bind_methods() {
	ClassDB::bind_method(D_METHOD("open", "command", "args", "cwd", "env_vars", "cols", "rows"), &TerminalPty::open, DEFVAL(PackedStringArray()), DEFVAL(String()), DEFVAL(Dictionary()), DEFVAL(80), DEFVAL(24));
	ClassDB::bind_method(D_METHOD("write_data", "data"), &TerminalPty::write_data);
	ClassDB::bind_method(D_METHOD("write_string", "text"), &TerminalPty::write_string);
	ClassDB::bind_method(D_METHOD("resize", "cols", "rows"), &TerminalPty::resize);
	ClassDB::bind_method(D_METHOD("is_running"), &TerminalPty::is_running);
	ClassDB::bind_method(D_METHOD("get_exit_code"), &TerminalPty::get_exit_code);
	ClassDB::bind_method(D_METHOD("get_pid"), &TerminalPty::get_pid);
	ClassDB::bind_method(D_METHOD("kill", "signal"), &TerminalPty::kill, DEFVAL(15));
	ClassDB::bind_method(D_METHOD("close"), &TerminalPty::close);
	ClassDB::bind_method(D_METHOD("poll"), &TerminalPty::poll);
	ClassDB::bind_method(D_METHOD("get_available_bytes"), &TerminalPty::get_available_bytes);

	ADD_SIGNAL(MethodInfo("data_received", PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data")));
	ADD_SIGNAL(MethodInfo("process_exited", PropertyInfo(Variant::INT, "exit_code")));
}

TerminalPty::TerminalPty() {
#if defined(_WIN32)
	driver = std::make_unique<PtyDriverWindows>();
#else
	driver = std::make_unique<PtyDriverPosix>();
#endif
}

TerminalPty::~TerminalPty() {
	close();
}

void TerminalPty::_thread_read_loop() {
	char chunk[8192];

	while (thread_running.load()) {
		int bytes_read = driver->read(chunk, sizeof(chunk));
		if (bytes_read > 0) {
			{
				std::lock_guard<std::mutex> lock(buffer_mutex);
				read_buffer.insert(read_buffer.end(), reinterpret_cast<uint8_t *>(chunk), reinterpret_cast<uint8_t *>(chunk) + bytes_read);
			}
		} else if (bytes_read < 0) {
			process_ended.store(true);
			break;
		} else {
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}

		if (!driver->is_running()) {
			process_ended.store(true);
			break;
		}
	}

	thread_running.store(false);
}

bool TerminalPty::open(
	const String &command,
	const PackedStringArray &args,
	const String &cwd,
	const Dictionary &env_vars,
	int cols,
	int rows
) {
	close();

	std::string cmd_str = command.utf8().get_data();
	std::vector<std::string> args_vec;
	for (int i = 0; i < args.size(); ++i) {
		args_vec.push_back(args[i].utf8().get_data());
	}

	std::string cwd_str = cwd.utf8().get_data();
	std::vector<std::pair<std::string, std::string>> env_vec;
	Array keys = env_vars.keys();
	for (int i = 0; i < keys.size(); ++i) {
		String key = keys[i];
		String val = env_vars[key];
		env_vec.emplace_back(key.utf8().get_data(), val.utf8().get_data());
	}

	if (!driver->open(cmd_str, args_vec, cwd_str, env_vec, cols, rows)) {
		return false;
	}

	thread_running.store(true);
	process_ended.store(false);
	cached_exit_code = -1;

	read_thread = std::thread(&TerminalPty::_thread_read_loop, this);
	return true;
}

int TerminalPty::write_data(const PackedByteArray &data) {
	if (!driver || !thread_running.load()) {
		return -1;
	}
	if (data.is_empty()) {
		return 0;
	}

	return driver->write(reinterpret_cast<const char *>(data.ptr()), static_cast<size_t>(data.size()));
}

int TerminalPty::write_string(const String &text) {
	if (!driver || !thread_running.load()) {
		return -1;
	}

	CharString utf8 = text.utf8();
	return driver->write(utf8.get_data(), static_cast<size_t>(utf8.length()));
}

bool TerminalPty::resize(int cols, int rows) {
	if (!driver) {
		return false;
	}
	return driver->resize(cols, rows);
}

bool TerminalPty::is_running() {
	if (!driver) {
		return false;
	}
	return driver->is_running();
}

int TerminalPty::get_exit_code() {
	if (!driver) {
		return cached_exit_code;
	}
	int code = driver->get_exit_code();
	if (code != -1) {
		cached_exit_code = code;
	}
	return cached_exit_code;
}

int TerminalPty::get_pid() const {
	return driver ? driver->get_pid() : -1;
}

void TerminalPty::kill(int signal_num) {
	if (driver) {
		driver->kill(signal_num);
	}
}

void TerminalPty::close() {
	thread_running.store(false);

	if (driver) {
		driver->close();
	}

	if (read_thread.joinable()) {
		read_thread.join();
	}

	std::lock_guard<std::mutex> lock(buffer_mutex);
	read_buffer.clear();
}

PackedByteArray TerminalPty::poll() {
	PackedByteArray result;

	{
		std::lock_guard<std::mutex> lock(buffer_mutex);
		if (!read_buffer.empty()) {
			result.resize(static_cast<int64_t>(read_buffer.size()));
			std::memcpy(result.ptrw(), read_buffer.data(), read_buffer.size());
			read_buffer.clear();
		}
	}

	if (result.size() > 0) {
		emit_signal("data_received", result);
	}

	if (process_ended.load()) {
		int exit_code = get_exit_code();
		process_ended.store(false);
		emit_signal("process_exited", exit_code);
	}

	return result;
}

int TerminalPty::get_available_bytes() {
	std::lock_guard<std::mutex> lock(buffer_mutex);
	return static_cast<int>(read_buffer.size());
}

} // namespace godot
