#if defined(_WIN32)

#include "pty_driver_windows.h"
#include <sstream>
#include <vector>

PtyDriverWindows::PtyDriverWindows() :
	h_pc(nullptr),
	h_process(nullptr),
	h_thread(nullptr),
	h_pipe_in(nullptr),
	h_pipe_out(nullptr),
	process_id(0),
	exit_code(-1),
	running(false) {}

PtyDriverWindows::~PtyDriverWindows() {
	close();
}

bool PtyDriverWindows::open(
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

	HANDLE h_pipe_in_read = nullptr;
	HANDLE h_pipe_in_write = nullptr;
	HANDLE h_pipe_out_read = nullptr;
	HANDLE h_pipe_out_write = nullptr;

	if (!CreatePipe(&h_pipe_in_read, &h_pipe_in_write, nullptr, 0)) {
		return false;
	}
	if (!CreatePipe(&h_pipe_out_read, &h_pipe_out_write, nullptr, 0)) {
		CloseHandle(h_pipe_in_read);
		CloseHandle(h_pipe_in_write);
		return false;
	}

	COORD size;
	size.X = static_cast<SHORT>(cols > 0 ? cols : 80);
	size.Y = static_cast<SHORT>(rows > 0 ? rows : 24);

	HRESULT hr = CreatePseudoConsole(size, h_pipe_in_read, h_pipe_out_write, 0, &h_pc);
	CloseHandle(h_pipe_in_read);
	CloseHandle(h_pipe_out_write);

	if (FAILED(hr)) {
		CloseHandle(h_pipe_in_write);
		CloseHandle(h_pipe_out_read);
		return false;
	}

	h_pipe_in = h_pipe_in_write;
	h_pipe_out = h_pipe_out_read;

	// Prepare process startup info with ConPTY attribute
	SIZE_T bytes_required = 0;
	InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes_required);
	std::vector<BYTE> attr_list_buffer(bytes_required);
	PPROC_THREAD_ATTRIBUTE_LIST attr_list = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(attr_list_buffer.data());

	if (!InitializeProcThreadAttributeList(attr_list, 1, 0, &bytes_required)) {
		close();
		return false;
	}

	if (!UpdateProcThreadAttribute(
			attr_list,
			0,
			PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
			h_pc,
			sizeof(HPCON),
			nullptr,
			nullptr)) {
		DeleteProcThreadAttributeList(attr_list);
		close();
		return false;
	}

	STARTUPINFOEXW si_ex;
	std::memset(&si_ex, 0, sizeof(si_ex));
	si_ex.StartupInfo.cb = sizeof(STARTUPINFOEXW);
	si_ex.lpAttributeList = attr_list;

	// Build command line
	std::wstring cmdline;
	std::wstringstream ss;
	ss << L"\"" << std::wstring(command.begin(), command.end()) << L"\"";
	for (const auto &arg : args) {
		ss << L" \"" << std::wstring(arg.begin(), arg.end()) << L"\"";
	}
	cmdline = ss.str();

	std::wstring wide_cwd = std::wstring(cwd.begin(), cwd.end());
	LPCWSTR pc_cwd = wide_cwd.empty() ? nullptr : wide_cwd.c_str();

	PROCESS_INFORMATION pi;
	std::memset(&pi, 0, sizeof(pi));

	std::vector<wchar_t> cmd_buffer(cmdline.begin(), cmdline.end());
	cmd_buffer.push_back(0);

	BOOL success = CreateProcessW(
		nullptr,
		cmd_buffer.data(),
		nullptr,
		nullptr,
		FALSE,
		EXTENDED_STARTUPINFO_PRESENT,
		nullptr,
		pc_cwd,
		&si_ex.StartupInfo,
		&pi
	);

	DeleteProcThreadAttributeList(attr_list);

	if (!success) {
		close();
		return false;
	}

	h_process = pi.hProcess;
	h_thread = pi.hThread;
	process_id = pi.dwProcessId;
	running = true;
	exit_code = -1;

	return true;
}

int PtyDriverWindows::read(char *buffer, size_t max_len) {
	if (h_pipe_out == nullptr || !running) {
		return -1;
	}

	DWORD bytes_available = 0;
	if (!PeekNamedPipe(h_pipe_out, nullptr, 0, nullptr, &bytes_available, nullptr)) {
		return -1;
	}

	if (bytes_available == 0) {
		if (!is_running()) {
			return -1;
		}
		return 0;
	}

	DWORD bytes_to_read = static_cast<DWORD>(max_len < bytes_available ? max_len : bytes_available);
	DWORD bytes_read = 0;
	if (ReadFile(h_pipe_out, buffer, bytes_to_read, &bytes_read, nullptr)) {
		return static_cast<int>(bytes_read);
	}

	return -1;
}

int PtyDriverWindows::write(const char *buffer, size_t len) {
	if (h_pipe_in == nullptr || !running) {
		return -1;
	}

	DWORD bytes_written = 0;
	if (WriteFile(h_pipe_in, buffer, static_cast<DWORD>(len), &bytes_written, nullptr)) {
		return static_cast<int>(bytes_written);
	}

	return -1;
}

bool PtyDriverWindows::resize(int cols, int rows) {
	if (h_pc == nullptr) {
		return false;
	}

	COORD size;
	size.X = static_cast<SHORT>(cols > 0 ? cols : 80);
	size.Y = static_cast<SHORT>(rows > 0 ? rows : 24);

	return SUCCEEDED(ResizePseudoConsole(h_pc, size));
}

bool PtyDriverWindows::is_running() {
	if (!running || h_process == nullptr) {
		return false;
	}

	DWORD code = 0;
	if (GetExitCodeProcess(h_process, &code)) {
		if (code == STILL_ACTIVE) {
			return true;
		}
		exit_code = static_cast<int>(code);
		running = false;
		return false;
	}

	running = false;
	return false;
}

int PtyDriverWindows::get_exit_code() {
	return exit_code;
}

void PtyDriverWindows::kill(int signal_num) {
	if (h_process != nullptr && running) {
		TerminateProcess(h_process, 1);
	}
}

void PtyDriverWindows::close() {
	if (running) {
		kill(15);
	}

	if (h_pipe_in != nullptr) {
		CloseHandle(h_pipe_in);
		h_pipe_in = nullptr;
	}

	if (h_pipe_out != nullptr) {
		CloseHandle(h_pipe_out);
		h_pipe_out = nullptr;
	}

	if (h_pc != nullptr) {
		ClosePseudoConsole(h_pc);
		h_pc = nullptr;
	}

	if (h_thread != nullptr) {
		CloseHandle(h_thread);
		h_thread = nullptr;
	}

	if (h_process != nullptr) {
		CloseHandle(h_process);
		h_process = nullptr;
	}

	running = false;
}

int PtyDriverWindows::get_pid() const {
	return static_cast<int>(process_id);
}

#endif // _WIN32
