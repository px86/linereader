#include <cctype>
#include <cstdlib>
#include <termios.h>
#include <iostream>
#include <unistd.h>

struct termios orig_termios;

void enable_raw_mode();
void disable_raw_mode();

int main(int argc, char **argv) {
  std::cout << "\nPress q to exit\n" << std::endl;

  if (tcgetattr(STDIN_FILENO, &orig_termios) < 0) {
    std::cerr << "tcgetattr failed" << std::endl;
    return 1;
  }
  atexit(disable_raw_mode);
  enable_raw_mode();

  char c;
  while (read(STDIN_FILENO, &c, 1) == 1) {
    if (iscntrl(c)) {
      printf("%d\r\n", c);
    } else printf("%d '%c'\r\n", c, c);

    if (c == 'q') break;
  }
  return 0;
}

void enable_raw_mode() {
  struct termios raw_mode = {0};

  raw_mode.c_iflag &=
      ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
  raw_mode.c_oflag &= ~OPOST;
  raw_mode.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
  raw_mode.c_cflag &= ~(CSIZE | PARENB);
  raw_mode.c_cflag |= CS8;

  // minimum number of bytes to read before returning
  raw_mode.c_cc[VMIN] = 1;
  // timeout in deci-seconds (1/10)
  raw_mode.c_cc[VTIME] = 1;


  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_mode);
}

void disable_raw_mode() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}
