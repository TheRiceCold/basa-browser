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

namespace basa_browser {
  Process::Process(std::string executable, std::vector<std::string> args)
    : executable_(std::move(executable)), args_(std::move(args)) {
    std::cout << "Process Constructor\n";
    if (executable_.empty())
      throw std::invalid_argument("Executable cannot be empty");
  }

  Process::~Process() {
    try {
      std::cout << "Process Destructor\n";
      terminate();
    } catch(...) {
      // NOTE: Destructors must not propagate exceptions.
      // TODO: Add robust fallback cleanup.
    }
  }

  // Process::Process(Process&& other) noexcept
  //   : executable_(std::move(other.executable_)), pid_(std::exchange(other.pid_, -1)) {}

  // Process& Process::operator=(Process&& other) noexcept {
  //   if (this != &other) {
  //     // TODO: Implement full cleanup semantics
  //     executable_ = std::move(other.executable_);
  //     pid_ = std::exchange(other.pid_, -1);
  //   }

  //   return *this;
  // }

  void Process::start() {
    std::cout << "Process Start\n";
    if (pid_ != -1)
      throw std::logic_error("Process can only be started once");

    std::vector<char*> argv;
    argv.reserve(args_.size() + 2); // reserve argv before fork

    argv.push_back(executable_.data());

    for (auto& a : args_) argv.push_back(a.data());

    argv.push_back(nullptr);

    const pid_t child = fork();

    if (child == -1)
      throw std::system_error(errno, std::generic_category(), "fork");

    if (child == 0) {
      // Child: create a new process group.
      if (setpgid(0, 0) == -1) _exit(126);

      execv(executable_.c_str(), argv.data());
      // when exec fails
      _exit(127); // 127: Command not found
    }

    // Parent: attempt to establish the process group.
    if (setpgid(child, child) == -1
	and errno != EACCES
	and errno != ESRCH) {

      const int error = errno; // Preserve error code

      kill(child, SIGKILL);
      waitpid(child, nullptr, 0);

      throw std::system_error(error, std::generic_category(), "setpgid");
    }

    pid_ = child;
    state_ = State::RUNNING;
  }

  void Process::update_state() {
    if (state_ != State::RUNNING) return;

    int status = 0;
    pid_t result;

    do {
      result = waitpid(pid_, &status, WNOHANG); // don't wait for child to finish
    } while (result == -1 and errno == EINTR); // -1: Error, EINTR: Interrupted

    if (result == 0) return; // child hasn't exited yet.

    if (result == -1)
      throw std::system_error(errno, std::generic_category(), "waitpid");

    if (WIFEXITED(status))
      exit_code_ = WEXITSTATUS(status);
    else if (WIFSIGNALED(status))
      termination_signal_ = WTERMSIG(status);

    pid_ = -1;
    state_ = State::EXITED;
  }

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

  void Process::terminate(std::chrono::milliseconds grace_period) {
    std::cout << "Process Terminate\n";

    if (!running()) return;

    if (kill(-pid_, SIGTERM) == -1 and errno != ESRCH)
      throw std::system_error(errno, std::generic_category(), "kill(SIGTERM)");

    const auto deadline = std::chrono::steady_clock::now() + grace_period;

    while(std::chrono::steady_clock::now() < deadline) {
      if (!running()) return;
      std::this_thread::sleep_for(std::chrono::milliseconds{50});
    }

    // Process didn't exist within the grace period.
    if (kill(-pid_, SIGKILL) == -1 and errno != ESRCH)
      throw std::system_error(errno, std::generic_category(), "kill(SIGKILL)");

    int status = 0;
    pid_t result;

    do {
      result = waitpid(pid_, &status, 0);
    } while (result == -1 and errno == EINTR);

    if (result == -1) {
      if (errno == ECHILD) {
	pid_ = -1;
	state_ = State::EXITED;
	return;
      }

      throw std::system_error(errno, std::generic_category(), "waitpid");
    }

    if (WIFEXITED(status)) exit_code_ = WEXITSTATUS(status);
    else if (WIFSIGNALED(status)) termination_signal_ = WTERMSIG(status);

    pid_ = -1;
    state_ = State::EXITED;
  }
}
