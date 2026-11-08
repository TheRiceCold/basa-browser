#include "basa_browser/process.hpp"

#include <iostream>

int main() {
  basa_browser::Process process("/usr/bin/sleep", {"30"});

  process.start();

  std::cout << "PID: " << process.pid() << '\n';
  process.terminate(std::chrono::seconds{2});
  std::cout << "Running: " << std::boolalpha << process.running() << '\n';
}
