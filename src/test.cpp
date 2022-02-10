#include "linereader.hpp"
#include <iostream>

int main(int argc, char **argv) {
  auto lr = LineReader();
  auto str = lr.readline("\x1b[33m$\x1b[m ");
  std::cout << "\n Got: " << str << '\n';
  return 0;
}
