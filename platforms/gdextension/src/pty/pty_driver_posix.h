#ifndef PTY_DRIVER_POSIX_H
#define PTY_DRIVER_POSIX_H

#if !defined(_WIN32)

#include "pty_driver.h"
#include <sys/types.h>

class PtyDriverPosix : public IPtyDriver {
private:
	int master_fd = -1;
	pid_t child_pid = -1;
	int exit_code = -1;
	bool running = false;

public:
	PtyDriverPosix();
	virtual ~PtyDriverPosix() override;

	virtual bool open(
		const std::string &command,
		const std::vector<std::string> &args,
		const std::string &cwd,
		const std::vector<std::pair<std::string, std::string>> &env_vars,
		int cols,
		int rows
	) override;

	virtual int read(char *buffer, size_t max_len) override;
	virtual int write(const char *buffer, size_t len) override;
	virtual bool resize(int cols, int rows) override;
	virtual bool is_running() override;
	virtual int get_exit_code() override;
	virtual void kill(int signal = 15) override;
	virtual void close() override;
	virtual int get_pid() const override;
};

#endif // !_WIN32
#endif // PTY_DRIVER_POSIX_H
