#include "basa_browser/process.hpp"

#include <utility>
#include <stdexcept>
#include <iostream>
#include <unistd.h>
#include <system_error>
#include <csignal>
#include <cerrno>
#include <sys/wait.h>

namespace basa_browser {
  Process::Process(std::string executable) : executable_(std::move(executable)) {
    std::cout << "Process Constructor\n";
    if (executable_.empty())
      throw std::invalid_argument("Executable cannot be empty");
  }

  Process::~Process() {
    try {
      std::cout << "Process Destructor\n";
      // terminate();
    } catch(...) { } }

  Process::Process(Process&& other) noexcept
    : executable_(std::move(other.executable_)), pid_(std::exchange(other.pid_, -1)) {}

  Process& Process::operator=(Process&& other) noexcept {
    if (this != &other) {
      // TODO: Implement full cleanup semantics
      executable_ = std::move(other.executable_);
      pid_ = std::exchange(other.pid_, -1);
    }

    return *this;
  }

  void Process::start() {
    std::cout << "Process Start\n";
    if (pid_ != -1)
      throw std::logic_error("Process can only be started once");

    const pid_t child = fork();

    if (child == -1)
      throw std::system_error(errno, std::generic_category(), "fork");

    if (child == 0) {
      execl(executable_.c_str(), executable_.c_str(), static_cast<char*>(nullptr));
      // when exec fails
      _exit(127); // 127: Command not found
    }

    pid_ = child;
  }

  void Process::terminate() {
    std::cout << "Process Terminate\n";
    if (pid_ == -1) return;

    if (kill(pid_, SIGTERM) == -1 // Failed to send SIGTERM(Signal Terminate)
	and errno != ESRCH) // ESRCH: No such process
      throw std::system_error(errno, std::generic_category(), "kill");

    int status = 0;

    while (waitpid(pid_, &status, 0) == -1) {
      if (errno == EINTR) continue; // Interrupted system call
      if (errno == ECHILD) break; // No child process
      throw std::system_error(errno, std::generic_category(), "waitpid");
    }
    pid_ = -1;
  }

  bool Process::running() const noexcept {
    if (pid_ == -1) return false;
    return kill(pid_, 0) == 0
      or errno == EPERM; // Operation not permitted
  }
  pid_t Process::pid() const noexcept {return pid_;}
}
