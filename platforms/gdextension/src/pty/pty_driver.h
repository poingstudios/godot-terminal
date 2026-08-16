#ifndef PTY_DRIVER_H
#define PTY_DRIVER_H

#include <string>
#include <vector>
#include <utility>
#include <cstddef>
#include <cstdint>

class IPtyDriver {
public:
	virtual ~IPtyDriver() = default;

	virtual bool open(
		const std::string &command,
		const std::vector<std::string> &args,
		const std::string &cwd,
		const std::vector<std::pair<std::string, std::string>> &env_vars,
		int cols,
		int rows
	) = 0;

	virtual int read(char *buffer, size_t max_len) = 0;
	virtual int write(const char *buffer, size_t len) = 0;
	virtual bool resize(int cols, int rows) = 0;
	virtual bool is_running() = 0;
	virtual int get_exit_code() = 0;
	virtual void kill(int signal = 15) = 0;
	virtual void close() = 0;
	virtual int get_pid() const = 0;
};

#endif // PTY_DRIVER_H
