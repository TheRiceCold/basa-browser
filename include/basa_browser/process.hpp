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
    using argv_t = std::vector<char*>;
    using pipe_t = std::array<int, 2>;

    [[nodiscard]] argv_t build_argv();
    [[nodiscard]] static pipe_t create_error_pipe();

    static void report_child_error(int fd, int error) noexcept;

    [[noreturn]] void execute_child(
      int error_fd,
      const char* executable,
      char* const argv[]) noexcept;

    static void configure_process_group(pid_t child);

    [[nodiscard]] static std::optional<int> read_child_error(int fd);

    static void cleanup_child(pid_t child) noexcept;

    void update_state();

    std::string executable_;
    std::vector<std::string> args_;

    pid_t pid_{-1}; // No child process associated
    State state_{State::NOT_STARTED};

    std::optional<int> exit_code_;
    std::optional<int> termination_signal_;
  };
}
