#pragma once
#include <sys/types.h>
#include <string>

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
    explicit Process(std::string executable);
    ~Process();

    // Modern C++ Disable Copy
    // NOTE; = delete instead of private to prevent "friend" backdoor
    Process(const Process&) = delete; // Copy Construction
    Process& operator=(const Process&) = delete; // Copy Assignment

    // noexcept operator C++11 - Escape exceptions
    Process(Process&&) noexcept;
    Process& operator=(Process&&) noexcept;

    void start();
    void terminate();

    // [[nodiscard]] C++ 17
    // Warns if called and return value is ignored.
    [[nodiscard]] bool running() const noexcept;
    [[nodiscard]] pid_t pid() const noexcept;

  private:
    std::string executable_;
    pid_t pid_{-1}; // No child process currently associated with the object
  };
}
