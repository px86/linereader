#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <termios.h>

#define CTRL_KEY(x) ((x) & 0x1f)

struct Coordinate {
  unsigned int row, col;
};

class LineReader {
public:
  LineReader();
  std::string readline(const char *prompt);

private:
  std::vector<std::string> m_history;
  std::vector<std::string>::reverse_iterator m_line;
  size_t m_insert_char_at;
  Coordinate m_last_cursor_pos;

  bool m_raw_mode_enabled;
  struct termios m_orig_termios;
  struct termios m_raw_termios;
  void enable_raw_mode();
  void disable_raw_mode();
  void save_line();
  int read_key() const;
  bool process_key(int key);
  Coordinate get_cursor_position() const;
};

enum Key {
  BACKSPACE = 127,
  ENTER_KEY = 13,
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  HOME_KEY,
  END_KEY,
  DEL_KEY,
  PAGE_UP,
  PAGE_DOWN,
  ALT_f,
  ALT_b,
  ALT_d
};
