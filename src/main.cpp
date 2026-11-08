#include "basa_browser/process.hpp"

#include <iostream>

int main() {
  basa_browser::Process process("/usr/bin/sleep", {"30"});

  process.start();

  std::cout << "PID: " << process.pid() << '\n';
  std::cout << "Running: " << std::boolalpha << process.running() << '\n';

  process.terminate();
  std::cout << "Exited: " << (process.state() == basa_browser::Process::State::EXITED) << '\n';

  if (auto code = process.exit_code())
    std::cout << "Exit code: " << *code << '\n';
  if (auto sig = process.termination_signal())
    std::cout << "Terminated by signal: " << *sig << '\n';
}
