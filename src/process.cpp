#include "basa_browser/process.hpp"

#include <utility>
#include <stdexcept>
#include <iostream>
#include <unistd.h>
#include <system_error>
#include <csignal>
#include <cerrno>
#include <sys/wait.h>
#include <thread>
#include <fcntl.h>

namespace basa_browser {
  Process::Process(std::string executable, std::vector<std::string> args)
    : executable_(std::move(executable)), args_(std::move(args)) {
    std::cout << "Process Constructor\n";
    if (executable_.empty())
      throw std::invalid_argument("Executable path cannot be empty");
  }

  Process::~Process() {
    std::cout << "Process Destructor\n";
    try {terminate();} // Don't allow exceptions to escape.
    catch (...) {// Destruction must continue even if cleanup fails.
      // TODO: Add robust fallback cleanup.
    }
  }

  Process::argv_t Process::build_argv() {
    argv_t argv;
    argv.reserve(args_.size() + 2); // Extra slot is for terminating nullptr
    argv.push_back(executable_.data());
    for (auto &a : args_) argv.push_back(a.data());
    argv.push_back(nullptr);
    return argv;
  }

  Process::pipe_t Process::create_error_pipe() {
     pipe_t pipefd{}; // pipefd[0] = read end, pipefd[1] = write end

    // O_CLOEXEC automatically closes descriptors when exec succeeds.
    if (pipe2(pipefd.data(), O_CLOEXEC) == -1)
      throw std::system_error(errno, std::generic_category(), "pipe2");

    return pipefd;
  }

  void Process::report_child_error(int fd, int error) noexcept {
    // Convert the error number into bytes so it can be sent through the pipe.
    const char* data = reinterpret_cast<const char*>(&error);

    std::size_t remaining = sizeof(error);

    while (remaining > 0) {  // Handle interrupted and partial writes.
      const ssize_t bytes_written = write(fd, data, remaining);

      if (bytes_written == -1) {
	if (errno == EINTR) continue; // Interrupted
	break; // Can't throw from this child-side error path.
      }

      if (bytes_written == 0) break;

      data += bytes_written;
      remaining -= static_cast<std::size_t>(bytes_written);
    }
  }

  [[noreturn]] void Process::execute_child(
    int error_fd,
    const char* executable,
    char* const argv[]) noexcept {
    // Create a new process group.
    // child becomes the leader of its own process group.
    if (setpgid(0, 0) == -1) {
      const int error = errno;
      report_child_error(error_fd, error);
      // _exit() terminates the child without running inherited
      // NOTE: unlike exit(), _exit() won't flush the stdio buffers.
      _exit(126); // 126: Found, but cannot execute
    }

    // Replace the child process image with the requested executable.
    execv(executable, argv); // Successful execv() never returns.

    // If execution reaches this point, execv() failed.
    const int error = errno; // Preserve error code
    report_child_error(error_fd, error);
    _exit(127); // 127: Command not found
  }

  void Process::configure_process_group(pid_t child) {
    // parent attempts to establish child's process group.
    // helps close the race between parent and child calling setpgid.

    if (setpgid(child, child) == 0) return;
    const int error = errno;

    // Neither condition requires treating process-group setup as a failure.
    if (error == EACCES or
	error == ESRCH) return; // ESRCH: child no longer exists.

    throw std::system_error(error, std::generic_category(), "setpgid");
  }

  std::optional<int> Process::read_child_error(int fd) {
    int child_error = 0;
    char* data = reinterpret_cast<char*>(&child_error);
    std::size_t total_read = 0;

    // Read the complete error number.
    while (total_read < sizeof(child_error)) {
      const ssize_t bytes_read = read(
	fd,
	data + total_read,
	sizeof(child_error) - total_read);

      if (bytes_read == -1) {
	if (errno == EINTR) continue; // Interrupted

	throw std::system_error(
	  errno,
	  std::generic_category(),
	  "read child error pipe");
      }

      if (bytes_read == 0) {
	// EOF(End Of File) without receiving an error means child closed the pipe without reporting a failure.

	// Normally happens bc exec succeeded and O_CLOEXEC closed child's write descriptor.
	if (total_read == 0) return std::nullopt;

	// Receiving only part of the error number indicates a broken or incomplete error-reporting protocol.
	throw std::runtime_error("Incomplete child error report");
      }

      total_read += static_cast<std::size_t>(bytes_read);
    }

    return child_error;
  }

  void Process::cleanup_child(pid_t child) noexcept {
    // Attempt to kill the entire process group.

    (void)kill(-child, SIGKILL);
    // Also target the direct child in case process-group creation failed.
    (void)kill(child, SIGKILL);

    // Reap the direct child to prevent a zombie process.
    int status = 0;
    while (waitpid(child, &status, 0) == -1) {
      if (errno == EINTR) continue;
      break;
    }
  }

  void Process::start() {
    // Refresh the cached state b4 deciding whether startup is allowed.
    update_state();

    if (state_ == State::RUNNING)
      throw std::logic_error("Process is already running");

    Process::argv_t argv = build_argv();

    // Create the communication pipe before creating the child.
    auto pipefd = create_error_pipe();

    const pid_t child = fork();
    if (child == -1) {
      const int error = errno;

      close(pipefd[0]);
      close(pipefd[1]);

      throw std::system_error(error, std::generic_category(), "fork");
    }

    if (child == 0) { // Child Process
      close(pipefd[0]); // child does not need the read end.
      execute_child(pipefd[1], executable_.c_str(), argv.data());
    }

    // Parent Process
    close(pipefd[1]); // parent doesn't need the write end.

    // Keep track of the read descriptor so cleanup doesn't close it twice.
    int error_fd = pipefd[0];

    try {
      // Ensure child belongs to its own process group.
      configure_process_group(child);

      // Wait for child to either report an error or close the pipe through successful exec.
      const auto child_error = read_child_error(error_fd);

      close(error_fd);
      error_fd = -1;

      if (child_error.has_value())
	throw std::system_error(*child_error, std::generic_category(), "child process startup");
    }
    catch (...) {
      // Close the read descriptor if it hasn't already been closed.
      if (error_fd != -1) close(error_fd);

      cleanup_child(child); // Do not leave a partially started child behind.

      throw;
    }

    // child passed the error-pipe handshake.
    pid_ = child;
    state_ = State::RUNNING;

    // It's possible for the child to exit immediately after exec.
    update_state();
  }

  void Process::update_state() {
    if (pid_ <= 0 or state_ != State::RUNNING) return;

    int status = 0;
    pid_t result;

    // WNOHANG prevents waitpid() from blocking if child is still running.
    do {result = waitpid(pid_, &status, WNOHANG);}
    while (result == -1 and errno == EINTR); // Interrupted

    if (result == 0) return; // child has not exited yet.

    if (result == -1)
      throw std::system_error(errno, std::generic_category(), "waitpid");

    // The child has exited normally.
    if (WIFEXITED(status)) {
      exit_code_ = WEXITSTATUS(status);
      termination_signal_.reset();
    }

    // The child was terminated by a signal.
    else if (WIFSIGNALED(status)) {
      termination_signal_ = WTERMSIG(status);
      exit_code_.reset();
    }

    // waitpid() has reaped the child.
    // Its PID must no longer be treated as an active child PID.
    pid_ = -1;
    state_ = State::EXITED;
  }

  void Process::terminate(std::chrono::milliseconds grace_period) {
    if (pid_ <= 0 or state_ != State::RUNNING) return;

    const pid_t child = pid_;

    // Ask the process group to terminate gracefully.
    if (kill(-child, SIGTERM) == -1 and errno != ESRCH)
      throw std::system_error(errno, std::generic_category(), "kill SIGTERM");

    const auto deadline = std::chrono::steady_clock::now() + grace_period;

    // Give the direct child time to exit.
    while (std::chrono::steady_clock::now() < deadline) {
      update_state();
      if (state_ != State::RUNNING) return;
      std::this_thread::sleep_for(std::chrono::milliseconds{50});
    }

    // The direct child didn't exit within the grace period.
    // Force termination of its process group.
    if (kill(-child, SIGKILL) == -1 and errno != ESRCH)
      throw std::system_error(errno, std::generic_category(), "kill SIGKILL");

    // Reap the direct child.
    int status = 0;
    pid_t result;

    do {result = waitpid(child, &status, 0);}
    while (result == -1 and errno == EINTR);

    if (result == -1)
      throw std::system_error(errno, std::generic_category(), "waitpid");

    // Record the final termination status.
    if (WIFEXITED(status)) {
      exit_code_ = WEXITSTATUS(status);
      termination_signal_.reset();
    }
    else if (WIFSIGNALED(status)) {
      termination_signal_ = WTERMSIG(status);
      exit_code_.reset();
    }

    pid_ = -1;
    state_ = State::EXITED;
  }

  // --- PUBLIC ACCESSORS --- //
  bool Process::running() {
    update_state();
    return state_ == State::RUNNING;
  }

  Process::State Process::state() const noexcept {return state_;}
  pid_t Process::pid() const noexcept {return pid_;}

  std::optional<int> Process::exit_code() {
    update_state();
    return exit_code_;
  }

  std::optional<int> Process::termination_signal() {
    update_state();
    return termination_signal_;
  }
}
