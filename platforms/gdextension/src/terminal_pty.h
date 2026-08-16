#ifndef TERMINAL_PTY_H
#define TERMINAL_PTY_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

#include "pty/pty_driver.h"

#include <memory>
#include <thread>
#include <mutex>
#include <vector>
#include <atomic>

namespace godot {

class TerminalPty : public RefCounted {
	GDCLASS(TerminalPty, RefCounted);

private:
	std::unique_ptr<IPtyDriver> driver;
	std::thread read_thread;
	std::mutex buffer_mutex;
	std::vector<uint8_t> read_buffer;
	std::atomic<bool> thread_running{false};
	std::atomic<bool> process_ended{false};
	int cached_exit_code = -1;

	void _thread_read_loop();

protected:
	static void _bind_methods();

public:
	TerminalPty();
	virtual ~TerminalPty() override;

	bool open(
		const String &command,
		const PackedStringArray &args,
		const String &cwd,
		const Dictionary &env_vars,
		int cols,
		int rows
	);

	int write_data(const PackedByteArray &data);
	int write_string(const String &text);
	bool resize(int cols, int rows);
	bool is_running();
	int get_exit_code();
	int get_pid() const;
	void kill(int signal = 15);
	void close();

	PackedByteArray poll();
	int get_available_bytes();
};

} // namespace godot

#endif // TERMINAL_PTY_H
