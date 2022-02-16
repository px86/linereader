#include "linereader.hpp"
#include <iostream>

int main(int argc, char **argv) {
  auto lr = LineReader(".history_file");
  const char *prompt = "\x1b[33m" ">> " "\x1b[m ";

  for (int i=0; i<3; ++i) {
    // read 3 lines, to demonstrate history feature.
    // Navigate the command history with C-p C-n or Up and Down arrows.
    auto str = lr.readline(prompt);
    std::cout << "\n[" << i << "] Line: " << str << '\n';
  }

  return 0;
}
