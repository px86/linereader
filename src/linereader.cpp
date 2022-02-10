#include "linereader.hpp"

#include <iostream>

#include <sys/ioctl.h>
#include <unistd.h>

LineReader::LineReader() {
  m_raw_mode_enabled = false;
  m_history.push_back("");
  m_line = m_history.rbegin();
  m_insert_char_at = m_line->size();

  // Save default termios.
  if (tcgetattr(STDIN_FILENO, &m_orig_termios)) {
    perror("Error: in LineReader::LineReader -> tcgetattr failed");
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
  // timeout in deci-seconds (1/10)
  m_raw_termios.c_cc[VTIME] = 1;
}

void LineReader::enable_raw_mode()
{
  if (m_raw_mode_enabled) return;
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &m_raw_termios)) {
    perror("Error: in LineReader::enable_raw_mode -> raw mode can not be enabled");
    exit(EXIT_FAILURE);
  }
  m_raw_mode_enabled = true;
}

void LineReader::disable_raw_mode()
{
  if (!m_raw_mode_enabled) return;
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &m_orig_termios)) {
    perror("Error: in LineReader::disable_raw_mode -> raw mode can not be disabled");
    exit(EXIT_FAILURE);
  }
  m_raw_mode_enabled = false;
}

auto LineReader::readline(const char *prompt) -> std::string
{
  enable_raw_mode();
  m_last_cursor_pos = get_cursor_position();

  auto redraw_line = [this, prompt]() {
    auto new_cpos = get_cursor_position();
    // Calculate the number of rows to be moved up
    auto rows_up = new_cpos.row - m_last_cursor_pos.row;
    if (rows_up > 0)
      std::cout << "\x1b[" << rows_up << "A";

    // Clear the screen from current line to the bottom
    std::cout << "\r\x1b[J" << prompt << *m_line;

    // Put the cursor at the current insertion position
    auto cols_left = m_line->size() - m_insert_char_at;
    if (cols_left > 0)
      std::cout << "\x1b[" << cols_left << "D";

    std::cout.flush();
  };

  bool done = false;
  while (!done) {
    redraw_line();
    int key = read_key();
    if (key == -1) {
      perror("Error: LineReader::read_key returned '-1'");
      break;
    }
    else done = process_key(key);
  }

  disable_raw_mode();
  return *m_line;
}

inline auto LineReader::read_key() const -> int
{
  auto read_byte = []() {
    char byte;
    if (read(STDIN_FILENO, &byte, 1) == -1) {
      perror("Error: in LineReader::read_key -> read failed");
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

inline auto LineReader::get_cursor_position() const -> Coordinate
{
  char buff[32];
  size_t pos = 0;

  if (write(STDOUT_FILENO, "\x1b[6n", 4) < 4) {
    perror("Error: in LineReader::get_cursor_position -> write failed");
    exit(EXIT_FAILURE);
  }
  while (pos < sizeof(buff)) {
    if (read(STDIN_FILENO, buff + pos, 1) == -1) {
      perror("Error: in LineReader::get_cursor_position -> read failed");
      exit(EXIT_FAILURE);
    }
    if (buff[pos++] == 'R') break;
  }
  buff[pos] = '\0';

  Coordinate cpos;
  sscanf(buff, "\x1b[%ud;%udR", &cpos.row, &cpos.col);
  return cpos;
}

inline auto LineReader::process_key(int key) -> bool {
  // Return true to quit, false to continue processing more keys.
  switch (key) {

  case ENTER_KEY:
    // TODO: implement history feature
    save_line();
    return true;

  case CTRL_KEY('h'):
  case BACKSPACE:
    if (m_insert_char_at > 0)
      m_line->erase(--m_insert_char_at, 1);
    break;

  case DEL_KEY:
  case CTRL_KEY('d'):
    if (m_insert_char_at < m_line->size())
      m_line->erase(m_insert_char_at, 1);
    break;

  case CTRL_KEY('c'):
    m_line->clear();
    m_insert_char_at = 0;
    break;

  case CTRL_KEY('l'):
    // Clear the screen and move the cursor to the top left corner.
    m_last_cursor_pos.row = 1;
    std::cout.write("\x1b[2J", 4);
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

  default:
    // Add non-control characters to the line.
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

void LineReader::save_line() {
  m_history.push_back(std::string(*m_line));
  m_line = m_history.rbegin();
}
