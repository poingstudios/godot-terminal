#if !defined(_WIN32)

#include "pty_driver_posix.h"

#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>

#if defined(__APPLE__)
#include <util.h>
#elif defined(__linux__)
#include <pty.h>
#endif

extern char **environ;

PtyDriverPosix::PtyDriverPosix() :
	master_fd(-1),
	child_pid(-1),
	exit_code(-1),
	running(false) {}

PtyDriverPosix::~PtyDriverPosix() {
	close();
}

bool PtyDriverPosix::open(
	const std::string &command,
	const std::vector<std::string> &args,
	const std::string &cwd,
	const std::vector<std::pair<std::string, std::string>> &env_vars,
	int cols,
	int rows
) {
	if (running) {
		close();
	}

	struct winsize ws;
	std::memset(&ws, 0, sizeof(ws));
	ws.ws_col = static_cast<unsigned short>(cols > 0 ? cols : 80);
	ws.ws_row = static_cast<unsigned short>(rows > 0 ? rows : 24);

	int slave_fd = -1;
	if (openpty(&master_fd, &slave_fd, nullptr, nullptr, &ws) < 0) {
		return false;
	}

	child_pid = fork();
	if (child_pid < 0) {
		::close(master_fd);
		::close(slave_fd);
		master_fd = -1;
		return false;
	}

	if (child_pid == 0) {
		// Child Process
		::close(master_fd);

		// Create a new session and attach slave as controlling terminal
		setsid();
#if defined(TIOCSCTTY)
		ioctl(slave_fd, TIOCSCTTY, 0);
#endif

		dup2(slave_fd, STDIN_FILENO);
		dup2(slave_fd, STDOUT_FILENO);
		dup2(slave_fd, STDERR_FILENO);

		if (slave_fd > STDERR_FILENO) {
			::close(slave_fd);
		}

		// Set default terminal environment
		setenv("TERM", "xterm-256color", 1);
		setenv("COLORTERM", "truecolor", 1);
		if (getenv("LANG") == nullptr) {
			setenv("LANG", "en_US.UTF-8", 1);
		}

		for (const auto &kv : env_vars) {
			setenv(kv.first.c_str(), kv.second.c_str(), 1);
		}

		if (!cwd.empty()) {
			if (chdir(cwd.c_str()) != 0) {
				// Ignore chdir error and proceed
			}
		}

		// Reset signal actions to default
		signal(SIGINT, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
		signal(SIGTSTP, SIG_DFL);
		signal(SIGTTIN, SIG_DFL);
		signal(SIGTTOU, SIG_DFL);
		signal(SIGCHLD, SIG_DFL);

		// Prepare argv
		std::vector<char *> argv;
		argv.push_back(const_cast<char *>(command.c_str()));
		for (const auto &arg : args) {
			argv.push_back(const_cast<char *>(arg.c_str()));
		}
		argv.push_back(nullptr);

		execvp(command.c_str(), argv.data());
		_exit(127);
	}

	// Parent Process
	::close(slave_fd);

	// Set master_fd to non-blocking
	int flags = fcntl(master_fd, F_GETFL, 0);
	if (flags >= 0) {
		fcntl(master_fd, F_SETFL, flags | O_NONBLOCK);
	}

	running = true;
	exit_code = -1;
	return true;
}

int PtyDriverPosix::read(char *buffer, size_t max_len) {
	if (master_fd < 0 || !running) {
		return -1;
	}

	ssize_t bytes_read = ::read(master_fd, buffer, max_len);
	if (bytes_read > 0) {
		return static_cast<int>(bytes_read);
	}

	if (bytes_read < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
			return 0;
		}
	}

	// EOF or error -> check if process has terminated
	if (!is_running()) {
		return -1;
	}

	return 0;
}

int PtyDriverPosix::write(const char *buffer, size_t len) {
	if (master_fd < 0 || !running) {
		return -1;
	}

	size_t total_written = 0;
	while (total_written < len) {
		ssize_t bytes_written = ::write(master_fd, buffer + total_written, len - total_written);
		if (bytes_written > 0) {
			total_written += bytes_written;
		} else if (bytes_written < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
				usleep(1000);
				continue;
			}
			break;
		} else {
			break;
		}
	}

	return static_cast<int>(total_written);
}

bool PtyDriverPosix::resize(int cols, int rows) {
	if (master_fd < 0) {
		return false;
	}

	struct winsize ws;
	std::memset(&ws, 0, sizeof(ws));
	ws.ws_col = static_cast<unsigned short>(cols > 0 ? cols : 80);
	ws.ws_row = static_cast<unsigned short>(rows > 0 ? rows : 24);

	return ioctl(master_fd, TIOCSWINSZ, &ws) == 0;
}

bool PtyDriverPosix::is_running() {
	if (!running) {
		return false;
	}

	if (child_pid <= 0) {
		running = false;
		return false;
	}

	int status = 0;
	pid_t res = waitpid(child_pid, &status, WNOHANG);
	if (res == 0) {
		return true;
	}

	if (res == child_pid) {
		running = false;
		if (WIFEXITED(status)) {
			exit_code = WEXITSTATUS(status);
		} else if (WIFSIGNALED(status)) {
			exit_code = 128 + WTERMSIG(status);
		} else {
			exit_code = 1;
		}
		return false;
	}

	return running;
}

int PtyDriverPosix::get_exit_code() {
	return exit_code;
}

void PtyDriverPosix::kill(int signal_num) {
	if (child_pid > 0 && running) {
		::kill(child_pid, signal_num);
	}
}

void PtyDriverPosix::close() {
	if (running) {
		kill(SIGHUP);
		is_running();
	}

	if (master_fd >= 0) {
		::close(master_fd);
		master_fd = -1;
	}

	running = false;
}

int PtyDriverPosix::get_pid() const {
	return child_pid;
}

#endif // !_WIN32
