#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <termios.h>

#define CTRL_KEY(x) ((x) & 0x1f)

struct Position {
  std::uint32_t row;
  std::uint32_t col;
};

class TermHandle {
public:
  TermHandle();
  void enable_raw_mode();
  void disable_raw_mode();

  Position get_cursor_position() const;
  int32_t read_key() const;
private:
  termios m_orig_termios;
  termios m_raw_termios;
  bool m_raw_mode_enabled;
};

class LineReader {
public:
  LineReader() = default;
  LineReader(const char *historypath);
  ~LineReader();
  std::string readline(const char *prompt);
  operator bool() { return m_good; };
private:
  std::vector<std::string> m_history;
  std::vector<std::string> m_killring;
  std::vector<std::string>::reverse_iterator m_line;
  std::vector<std::string>::reverse_iterator m_current_kill;
  bool m_last_was_yank;
  size_t m_insert_char_at;
  TermHandle m_term;
  Position m_cursor_position;
  bool m_good = true;
  const char *m_historypath;
  bool process_key(int key);
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
  ALT_d,
  ALT_l,
  ALT_u,
  ALT_c,
  ALT_t,
  ALT_y
};
