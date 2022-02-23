#include <cctype>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>

#include "linereader.hpp"

// Word delimeters
const char *delimeters = " \t\n:-_'\"()[]{}";

TermHandle::TermHandle()
{
  m_raw_mode_enabled = false;

  // Save default termios.
  if (tcgetattr(STDIN_FILENO, &m_orig_termios)) {
    perror("Error: tcgetattr failed");
    exit(EXIT_FAILURE);
  }

  // Settings for raw mode.
  m_raw_termios.c_iflag &=
      ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
  m_raw_termios.c_oflag &= ~OPOST;
  m_raw_termios.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
  m_raw_termios.c_cflag &= ~(CSIZE | PARENB);
  m_raw_termios.c_cflag |= CS8;

  // minimum number of bytes to read before returning
  m_raw_termios.c_cc[VMIN] = 1;
  // timeout in deci-seconds (1/10) (Does it matter now?)
  m_raw_termios.c_cc[VTIME] = 1;
}

inline void TermHandle::enable_raw_mode()
{
  if (m_raw_mode_enabled) return;
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &m_raw_termios)) {
    perror("Error: tcsetattr failed while trying to enable raw mode");
    exit(EXIT_FAILURE);
  }
  m_raw_mode_enabled = true;
}

inline void TermHandle::disable_raw_mode()
{
  if (!m_raw_mode_enabled) return;
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &m_orig_termios)) {
    perror("Error: tcsetattr failed while trying to disable raw mode");
    exit(EXIT_FAILURE);
  }
  m_raw_mode_enabled = false;
}

auto LineReader::readline(const char *prompt) -> std::string
{
  m_term.enable_raw_mode();

  // Add a new string to m_history.
  m_history.push_back("");
  m_line = m_history.rbegin();
  m_insert_char_at = 0;
  m_cursor_position = m_term.get_cursor_position();
  m_last_was_yank = false;

  auto draw_line = [this, prompt]() {
    std::cout << "\x1b["
	      << m_cursor_position.row << ';'
              << m_cursor_position.col << 'H';

    // Clear the screen from current line to the bottom
    std::cout << "\r\x1b[J" << prompt << *m_line;

    // Put the cursor at the current insertion position
    auto cols_left = m_line->size() - m_insert_char_at;
    if (cols_left > 0) std::cout << "\x1b[" << cols_left << "D";

    std::cout.flush();
  };

  bool done = false;
  while (!done) {
    draw_line();
    int32_t key = m_term.read_key();
    if (key == -1) {
      perror("Error: read_key returned '-1'");
      continue;
    }
    else done = process_key(key);
  }

  m_term.disable_raw_mode();
  return *m_line;
}

inline auto TermHandle::read_key() const -> std::int32_t
{
  auto read_byte = []() {
    char byte;
    if (read(STDIN_FILENO, &byte, 1) != 1) {
      perror("Error: read failed while trying to read a byte");
      exit(EXIT_FAILURE);
    } else return byte;
  };
  char seq[4];
  seq[0] = read_byte();
  if (seq[0] != '\x1b') return seq[0];
  seq[1] = read_byte();
  switch (seq[1]) {
  case 'f' : return ALT_f;
  case 'b' : return ALT_b;
  case 'd' : return ALT_d;
  case 'l' : return ALT_l;
  case 'u' : return ALT_u;
  case 'c' : return ALT_c;
  case 't' : return ALT_t;
  case 'y' : return ALT_y;
  case '[' :
    seq[2] = read_byte();
    if (seq[2]>='0' && seq[2]<='9')
      {
	seq[3] = read_byte();
	if (seq[3] == '~') {
	  // \x1b [ ? ~
          switch (seq[2]) {
	  case '1': return HOME_KEY;
	  case '3': return DEL_KEY;
	  case '4': return END_KEY;
          case '5': return PAGE_UP;
          case '6': return PAGE_DOWN;
	  case '7': return HOME_KEY;
	  case '8': return END_KEY;
          }
        }
      }
    else {
      // \x1b [ ?
       switch (seq[2]) {
	case 'A': return ARROW_UP;
        case 'B': return ARROW_DOWN;
        case 'C': return ARROW_RIGHT;
        case 'D': return ARROW_LEFT;
	case 'H': return HOME_KEY;
	case 'F': return END_KEY;
       }
    }
  default: return '\x1b';
  }
}

inline auto TermHandle::get_cursor_position() const -> Position
{
  char buff[32];
  size_t pos = 0;
  // \x1b[6n query for cursor position.
  if (write(STDOUT_FILENO, "\x1b[6n", 4) < 4) {
    perror("Error: write failed in get_cursor_position");
    exit(EXIT_FAILURE);
  }
  while (pos < sizeof(buff)) {
    if (read(STDIN_FILENO, buff + pos, 1) == -1) {
      perror("Error: read failed in get_cursor_position");
      exit(EXIT_FAILURE);
    }
    if (buff[pos++] == 'R') break;
  }
  buff[pos] = '\0';

  Position cursor_pos;
  sscanf(buff, "\x1b[%ud;%udR", &cursor_pos.row, &cursor_pos.col);
  return cursor_pos;
}

inline auto LineReader::process_key(int key) -> bool
{
  // Return 'true' to quit, 'false' to continue processing more keys.

  if (key != CTRL_KEY('y') && key != ALT_y)
    m_last_was_yank = false;

  switch (key) {

  case ENTER_KEY:
    if (m_line->empty()) {
      m_history.pop_back();
      std::cout << '\n';
      m_cursor_position = m_term.get_cursor_position();
    }
    // return from readline
    return true;

  case CTRL_KEY('h'):
  case BACKSPACE:
    if (m_insert_char_at > 0)
      m_line->erase(--m_insert_char_at, 1);
    break;

  case DEL_KEY:
  case CTRL_KEY('d'):
    if (m_line->empty()) {
      m_good = false;
      m_term.disable_raw_mode();
      close(STDIN_FILENO);
      return true; // end the loop
    }
    if (m_insert_char_at < m_line->size())
      m_line->erase(m_insert_char_at, 1);
    break;

  case CTRL_KEY('c'):
    // Discard the current line.
    std::cout << "\x1b[31m" "^C" "\x1b[m\n";
    m_cursor_position = m_term.get_cursor_position();
    m_line->clear();
    m_insert_char_at = 0;
    break;

  case CTRL_KEY('l'):
    m_cursor_position.row = 1;
    break;

  case HOME_KEY:
  case CTRL_KEY('a'):
    m_insert_char_at = 0;
    break;

  case END_KEY:
  case CTRL_KEY('e'):
    m_insert_char_at = m_line->size();
    break;

  case ARROW_LEFT:
  case CTRL_KEY('b'):
    if (m_insert_char_at > 0)
      --m_insert_char_at;
    break;

  case ARROW_RIGHT:
  case CTRL_KEY('f'):
    if (m_insert_char_at < m_line->size())
      ++m_insert_char_at;
    break;

  case ARROW_DOWN:
  case CTRL_KEY('n'):
    if (m_line != m_history.rbegin()) {
      --m_line;
      m_insert_char_at = m_line->size();
    }
    break;

  case ARROW_UP:
  case CTRL_KEY('p'):
    if (m_line != m_history.rend()-1) {
      ++m_line;
      m_insert_char_at = m_line->size();
    }
    break;

  case ALT_f:
    {
      auto i = m_line->find_first_not_of(delimeters, m_insert_char_at);
      if (i != std::string::npos)
	  i = m_line->find_first_of(delimeters, i+1);
      m_insert_char_at = (i != std::string::npos) ? i:m_line->size();
    }
    break;

  case ALT_b:
    {
      if (m_insert_char_at == 0) break;
      auto i = m_line->find_last_not_of(delimeters, m_insert_char_at - 1);
      if (i != std::string::npos)
	  i = m_line->find_last_of(delimeters, i);
      m_insert_char_at = (i != std::string::npos) ? ++i : 0;
    }
    break;

  case ALT_d:
    {
      auto i = m_line->find_first_not_of(delimeters, m_insert_char_at);
      if (i != std::string::npos)
	  i = m_line->find_first_of(delimeters, i+1);
      auto j = (i != std::string::npos) ? i:m_line->size();
      m_line->erase(m_insert_char_at, j-m_insert_char_at);
    }
    break;

  case ALT_l:
    {
      auto i = m_line->find_first_not_of(delimeters, m_insert_char_at);
      if (i == std::string::npos)
        break;
      auto j = m_line->find_first_of(delimeters, i+1);
      if (j == std::string::npos) j = m_line->size();
      for (auto t=i; t<j; ++t) {m_line->at(t) = tolower(m_line->at(t));};
      m_insert_char_at = j;
    }
    break;

  case ALT_u:
    {
      auto i = m_line->find_first_not_of(delimeters, m_insert_char_at);
      if (i == std::string::npos)
        break;
      auto j = m_line->find_first_of(delimeters, i+1);
      if (j == std::string::npos) j = m_line->size();
      for (auto t=i; t<j; ++t) {m_line->at(t) = toupper(m_line->at(t));};
      m_insert_char_at = j;
    }
    break;

  case ALT_c:
    {
      auto i = m_line->find_first_not_of(delimeters, m_insert_char_at);
      if (i == std::string::npos)
        break;
      m_line->at(i) = toupper(m_line->at(i));
      auto j = m_line->find_first_of(delimeters, i+1);
      if (j == std::string::npos) j = m_line->size();
      for (auto t=i+1; t<j; ++t) {m_line->at(t) = tolower(m_line->at(t));};
      m_insert_char_at = j;
    }
    break;

  case CTRL_KEY('t'):
    if (m_insert_char_at != 0 && m_line->size() > 1) {
      if (m_insert_char_at == m_line->size()) {
	char c = m_line->at(m_insert_char_at-2);
        m_line->at(m_insert_char_at - 2) = m_line->at(m_insert_char_at - 1);
        m_line->at(m_insert_char_at - 1) = c;
      } else {
	char c = m_line->at(m_insert_char_at-1);
        m_line->at(m_insert_char_at - 1) = m_line->at(m_insert_char_at);
        m_line->at(m_insert_char_at) = c;
	++m_insert_char_at;
      }
    }
    break;

  case ALT_t:
    if (m_insert_char_at != 0 && m_insert_char_at != m_line->size() &&
        m_line->size() > 2) {

      // r for right, and l for left hand side word
      size_t r_start, r_end, l_start, l_end;

      if (std::isspace(m_line->at(m_insert_char_at))) {
	r_start = m_line->find_first_not_of(delimeters, m_insert_char_at);
	if (r_start == std::string::npos) break;
      } else {
	r_start = m_line->find_last_of(delimeters, m_insert_char_at);
	if (r_start == std::string::npos) break;
	else r_start++;
      }

      r_end = m_line->find_first_of(delimeters, r_start);
      if (r_end == std::string::npos) r_end = m_line->size();
      else r_end--;

      l_end = m_line->find_last_not_of(delimeters, r_start-1);
      if (l_end == std::string::npos) break;

      l_start = m_line->find_last_of(delimeters, l_end);
      if (l_start == std::string::npos) l_start = 0;
      else l_start++;

      auto wr = m_line->substr(r_start, r_end - r_start + 1);
      auto wl = m_line->substr(l_start, l_end - l_start + 1);

      m_line->erase(r_start, wr.size());
      m_line->insert(r_start, wl, 0, wl.size());

      m_line->erase(l_start, wl.size());
      m_line->insert(l_start, wr, 0, wr.size());

      m_insert_char_at = r_start + wr.size();
    }
    break;

  case CTRL_KEY('k'):
    {
      auto killed_text =
        m_line->substr(m_insert_char_at, m_line->size() - m_insert_char_at);
      m_killring.push_back(killed_text);
      m_current_kill = m_killring.rbegin();
      m_line->erase(m_insert_char_at, m_line->size() - m_insert_char_at);
    }
    break;

  case CTRL_KEY('u'):
    {
      auto killed_text = m_line->substr(0, m_insert_char_at);
      m_killring.push_back(killed_text);
      m_current_kill = m_killring.rbegin();
      m_line->erase(0, m_insert_char_at);
      m_insert_char_at = 0;
    }
    break;

  case CTRL_KEY('y'):
    if (m_killring.empty()) break;
    m_line->insert(m_insert_char_at, *m_current_kill);
    m_insert_char_at += m_current_kill->size();
    m_last_was_yank = true;
    break;

  case ALT_y:
    if (!m_last_was_yank || m_killring.size() == 1)
      break;

    m_insert_char_at -= m_current_kill->size();
    m_line->erase(m_insert_char_at, m_current_kill->size());

    ++m_current_kill;
    if (m_current_kill == m_killring.rend())
      m_current_kill = m_killring.rbegin();

    m_line->insert(m_insert_char_at, *m_current_kill);
    m_insert_char_at += m_current_kill->size();
    break;

  default:
    // Insert non-control characters to the current line at 'm_insert_char_at' position.
    if (!iscntrl(key)) {
      if (m_insert_char_at >= 0 and m_insert_char_at < m_line->size()) {
        m_line->insert(m_line->begin() + m_insert_char_at, key);
      } else if (m_insert_char_at == m_line->size()) {
        m_line->push_back(key);
      }
      m_insert_char_at++;
    }
  }
  return false;
}

LineReader::LineReader(const char *historypath) {
  m_historypath = historypath;

  auto history_file = std::ifstream(historypath);
  if (history_file) {
    std::string hist_line;
    while (history_file) {
      std::getline(history_file, hist_line);
      if (!hist_line.empty()) m_history.push_back(hist_line);
    }
  }
}

LineReader::~LineReader() {
  if (m_historypath != nullptr) {
    auto history_file = std::ofstream(m_historypath);
    if (history_file) {
      for (const auto& hist_lin: m_history)
	history_file << hist_lin << '\n';

      history_file.close();
    }
  }
}
