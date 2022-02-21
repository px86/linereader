#include "linereader.hpp"
#include <iostream>

bool execute(std::string &cmd);

int main(int argc, char **argv) {
  auto lr = LineReader(".history_file");
  const char *prompt = "\x1b[33m" ">> " "\x1b[m ";

  while (lr) {
    auto cmd = lr.readline(prompt);
    if (!cmd.empty()) execute(cmd);
  }
  std::cout << '\n' << "Bye!" << std::endl;

  return 0;
}

bool execute(std::string &cmd) {
  std::cout << '\n'
            << "You entered: "
            << cmd
	    << std::endl;

  return true;
}
