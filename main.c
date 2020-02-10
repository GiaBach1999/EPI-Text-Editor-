#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>

/*** data***/
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE
#define TAB_STOP 8
#define EPITOR_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT {NULL, 0}

typedef struct erow {
  int size;
  int rsize;
  char *chars;
  char *render;
} erow;

struct editorConfig {
  int cx, cy;
  int screenrows;
  int screencols;
  int rowoff;
  int coloff;
  int numrows;
  int rx;
  erow *row;
  struct termios orig_termios;
};
struct editorConfig E;
enum editorKey
{
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN
};

struct editor_config
{
    int cx, cy;
    int screenrows;
    int screencols;
    struct termios original_terminal;
};

struct editor_config editor;

struct abuf
{
    char *b;
    int len;
};

/*** terminal***/
void die(const char *s)
{
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(s);
    exit(1);
}

//turn off raw mode
void disableRawMode()
{
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &editor.original_terminal) == -1)
        die("tcsetattr");
}
//turn on raw mode
void enableRawMode()
{
    struct termios raw;
    //get attributes of the terminal
    if(tcgetattr(STDIN_FILENO, &editor.original_terminal) == -1)
        die("tcsetattr");
    //disable raw mode at exit
    atexit(disableRawMode);
    //turn off echo function of the terminal so what you're typing won't appear
    raw = editor.original_terminal;
    raw.c_iflag &= ~(ICRNL| IXON | ISTRIP | INPCK | BRKINT);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= ~(CS8);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr");
}

int editorReadKey()
{
  int nread;
  char input;
  while ((nread = read(STDIN_FILENO, &input, 1)) != 1)
  {
    if (nread == -1 && errno != EAGAIN)
         die("read");
  }
  if (input == '\x1b')
  {
    char seq[3];
    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
    if (seq[0] == '[')
     {
       if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
        if (seq[2] == '~') {
          switch (seq[1]) {
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
      else
      {
      switch (seq[1])
      {
        case 'A': return ARROW_UP;
        case 'B': return ARROW_DOWN;
        case 'C': return ARROW_RIGHT;
        case 'D': return ARROW_LEFT;
        case 'H': return HOME_KEY;
        case 'F': return END_KEY;
      }
    }
    } else if (seq[0] == 'O') {
      switch (seq[1]) {
        case 'H': return HOME_KEY;
        case 'F': return END_KEY;
      }
    }
    return '\x1b';
  } else {
    return input;
  }
}

int getCursorPosition(int *rows, int *cols)
{
  char buf[32];
  unsigned int i = 0;

  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
    return -1;

  while (i < sizeof(buf) - 1)
  {
    if(read(STDIN_FILENO, &buf[i], 1) != 1)
      break;
    if(buf[i] == 'R')
      break;
    i++;
  }

  if (buf[0] != '\x1b' || buf[1] != '[')
    return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
    return -1;
  return 0;

}

int getWindowSize(int *rows, int *cols)
{
    struct winsize winsize;
    if(ioctl(STDOUT_FILENO,TIOCGWINSZ, &winsize) == -1 || winsize.ws_col == 0)
    {
        if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B",12) != 12)
            return -1;
        return getCursorPosition(rows,cols);
    }
    else
    {
        *cols = winsize.ws_col;
        *rows = winsize.ws_row;
        return 0;
    }

}

void editorMoveCursor(int key) {
  erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  switch (key) {
    case ARROW_LEFT:
      if (E.cx != 0) {
        E.cx--;
      } else if (E.cy > 0) {
        E.cy--;
        E.cx = E.row[E.cy].size;
      }
      break;
    case ARROW_RIGHT:
      if (row && E.cx < row->size) {
        E.cx++;
      }
      break;
    case ARROW_UP:
      if (E.cy != 0) {
        E.cy--;
      }
      break;
    case ARROW_DOWN:
      if (E.cy < E.numrows) {
        E.cy++;
      }
      break;
  }
  row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  int rowlen = row ? row->size : 0;
  if (E.cx > rowlen) {
    E.cx = rowlen;
  }

  /*** row operations ***/

  int editorRowCxToRx(erow *row, int cx) {
    int rx = 0;
    int j;
    for (j = 0; j < cx; j++) {
      if (row->chars[j] == '\t')
        rx += (TAB_STOP - 1) - (rx % TAB_STOP);
      rx++;
    }
    return rx;
  }

  void editorUpdateRow(erow *row) {
    int tabs = 0;
    int j;
    for (j = 0; j < row->size; j++)
      if (row->chars[j] == '\t')
        tabs++;
    free(row->render);
    row->render = malloc(row->size + tabs*(TAB_STOP - 1) + 1);

    int idx = 0;
    for (j = 0; j < row->size; j++)
    {
      if (row->chars[j] == '\t')
      {
        row->render[idx++] = ' ';
        while (idx % TAB_STOP != 0)
          row->render[idx++] = ' ';
      }
      else
      {
        row->render[idx++] = row->chars[j];
      }
    }
    row->render[idx] = '\0';
    row->rsize = idx;
  }

  void editorAppendRow(char *s, size_t len)
  {
    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
    int at = E.numrows;
    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    E.row[at].rsize = 0;
    E.row[at].render = NULL;

    E.numrows++;
  }

  /*** file i/o ***/
  void editorOpen(char *filename)
  {
    FILE *fp = fopen(filename, "r");
    if (!fp) die("fopen");
    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    linelen = getline(&line, &linecap, fp);
    while ((linelen = getline(&line, &linecap, fp)) != -1)
    {
      while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
        linelen--;
    }
    editorAppendRow(line, linelen);

    free(line);
    fclose(fp);
  }

/*** input ***/
void editorProcessKeypress()
{
  int input = editorReadKey();
  switch (input)
  {
    case CTRL_KEY('q'):
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
        break;

    case HOME_KEY:
      editor.cx = 0;
      break;
    case END_KEY:
      editor.cx = editor.screencols - 1;
      break;

    case PAGE_UP:
    case PAGE_DOWN:
      {
        int times = editor.screenrows;
        while (times--)
          editorMoveCursor(input == PAGE_UP ? ARROW_UP : ARROW_DOWN);
      }
      break;

    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
      editorMoveCursor(input);
      break;
  }
}

void abAppend(struct abuf *ab, const char *s, int len)
{
  char *new = realloc(ab->b, ab->len + len);
  if (new == NULL) return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

void abFree(struct abuf *ab)
{
  free(ab->b);
}

/*** output ***/

void editorDrawRows(struct abuf *ab) {
  int y;
  for (y = 0; y < E.screenrows; y++) {
    int filerow = y + E.rowoff;
    if (filerow >= E.numrows) {
      if (E.numrows == 0 && y == E.screenrows / 3) {
        char welcome[80];
        int welcomelen = snprintf(welcome, sizeof(welcome),
          "Kilo editor -- version %s", EPITOR_VERSION);
        if (welcomelen > E.screencols) welcomelen = E.screencols;
        int padding = (E.screencols - welcomelen) / 2;
        if (padding) {
          abAppend(ab, "~", 1);
          padding--;
        }
        while (padding--) abAppend(ab, " ", 1);
        abAppend(ab, welcome, welcomelen);
      } else {
        abAppend(ab, "~", 1);
      }
    } else {
      int len = E.row[filerow].rsize - E.coloff;
      if (len < 0) len = 0;
      if (len > E.screencols) len = E.screencols;
      abAppend(ab, &E.row[filerow].render[E.coloff], len);
    }
    abAppend(ab, "\x1b[K", 3);
    if (y < E.screenrows - 1) {
      abAppend(ab, "\r\n", 2);
    }
  }
}

void editorScroll() {
  E.rx = 0;
  if (E.cy < E.numrows) {
    E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
  }

  if (E.cy < E.rowoff) {
    E.rowoff = E.cy;
  }
  if (E.cy >= E.rowoff + E.screenrows) {
    E.rowoff = E.cy - E.screenrows + 1;
  }
  if (E.rx < E.coloff) {
    E.coloff = E.rx;
  }
  if (E.rx >= E.coloff + E.screencols) {
    E.coloff = E.rx - E.screencols + 1;
  }
}

void editorRefreshScreen()
{
  editorScroll();

  struct abuf ab = ABUF_INIT;

  abAppend(&ab, "\x1b[?25l", 6);

  abAppend(&ab, "\x1b[H", 3);

  editorDrawRows(&ab);

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, (E.rx - E.coloff) + 1);
  abAppend(&ab, buf, strlen(buf));

  //abAppend(&ab, "\x1b[H", 3);
  abAppend(&ab, "\x1b[?25h", 6);

  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}


/*** init ***/

void initEditor()
{
  E.cx = 0;
  E.cy = 0;
  E.rowoff = 0;
  E.coloff = 0;
  E.numrows = 0;
  E.rx = 0;
  E.row = NULL;
  if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}

/*** append buffer ***/


int main(int argc, char *argv[]) {
  enableRawMode();
  initEditor();
  if (argc >= 2) {
    editorOpen(argv[1]);
  }
  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }
  return 0;
}
