#include "basa_browser/process.hpp"

#include <iostream>

int main() {
  basa_browser::Process process("/usr/bin/sleep");

  process.start();

  std::cout << "PID: " << process.pid() << '\n';

  process.terminate();
}
