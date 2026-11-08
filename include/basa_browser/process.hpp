#pragma once

#include <string>
#include <sys/types.h>
#include <vector>
#include <optional>
#include <chrono>

namespace basa_browser {
  // Linux Process Abstraction
  // A process is a running program the resources/state the OS gives it

  /* The kernel gives each process things like:
     - PID
     - virtual memory
     - file descriptors
     - CPU state
     - environment variables
     - credentials
     - working directory
     - signals*/

  // You can see processes with command: ps

  // Think: Process is our C++ object that owns/manages a Linux process.
  class Process {
  public:
    enum class State {NOT_STARTED, RUNNING, EXITED};
    explicit Process(
      std::string executable,
      std::vector<std::string> args = {});

    ~Process();

    // Modern C++ Disable Copy
    // NOTE; = delete instead of private to prevent "friend" backdoor
    Process(const Process&) = delete; // Copy Construction
    Process& operator=(const Process&) = delete; // Copy Assignment

    // NOTE:disable until ownership transfer
    // Process(Process&&) noexcept;
    // Process& operator=(Process&&) noexcept;

    void start();
    void terminate(std::chrono::milliseconds grace_period = std::chrono::milliseconds{2});

    // [[nodiscard]] C++ 17
    // Warns if called and return value is ignored.
    [[nodiscard]] bool running();
    [[nodiscard]] State state() const noexcept;
    [[nodiscard]] pid_t pid() const noexcept;

    [[nodiscard]] std::optional<int> exit_code();
    [[nodiscard]] std::optional<int> termination_signal();
  private:
    void update_state();

    std::string executable_;
    std::vector<std::string> args_;

    pid_t pid_{-1}; // -1: No child process currently associated with the object
    State state_{State::NOT_STARTED};

    std::optional<int> exit_code_;
    std::optional<int> termination_signal_;
  };
}
