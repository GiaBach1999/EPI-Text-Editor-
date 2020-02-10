#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <stdarg.h>
#include <stdlib.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

/*** data***/
#define EPITOR_VERSION "0.0.1"
#define EPITOR_TAB_STOP 8
#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT {NULL, 0}

typedef struct erow {
  int size;
  int rsize;
  char *chars;
  char *render;
} erow; 

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
    int rx;
    int rowoff;
    int coloff;
    int screenrows;
    int screencols;
    int numrows;
    erow *row;
    char *filename;
    char statusmsg[80];
    time_t statusmsg_time;
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

/*** row operations ***/

int editorRowCxToRx(erow *row, int cx) {
  int rx = 0;
  int j;
  for (j = 0; j < cx; j++) {
    if (row->chars[j] == '\t')
      rx += (EPITOR_TAB_STOP - 1) - (rx % EPITOR_TAB_STOP);
    rx++;
  }
  return rx;
}

void editorUpdateRow(erow *row) {
  int tabs = 0;
  int j;
  for (j = 0; j < row->size; j++)
    if (row->chars[j] == '\t') tabs++;
  free(row->render);
  row->render = malloc(row->size + tabs*(EPITOR_TAB_STOP - 1) + 1);

  int idx = 0;
  for (j = 0; j < row->size; j++) {
    if (row->chars[j] == '\t') {
      row->render[idx++] = ' ';
      while (idx % EPITOR_TAB_STOP != 0) row->render[idx++] = ' ';
    } else {
      row->render[idx++] = row->chars[j];
    }
  }
  row->render[idx] = '\0';
  row->rsize = idx;
}

void editorAppendRow(char *s, size_t len) 
{
  editor.row = realloc(editor.row, sizeof(erow) * (editor.numrows + 1));
  int at = editor.numrows;
  editor.row[at].size = len;
  editor.row[at].chars = malloc(len + 1);
  memcpy(editor.row[at].chars, s, len);
  editor.row[at].chars[len] = '\0';

  editor.row[at].rsize = 0;
  editor.row[at].render = NULL;
  editorUpdateRow(&editor.row[at]);

  editor.numrows++;
}

/*** file i/o ***/
void editorOpen(char *filename) 
{
  free(editor.filename);
  editor.filename = strdup(filename);

  FILE *fp = fopen(filename, "r");
  if (!fp) die("fopen");
  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  while ((linelen = getline(&line, &linecap, fp)) != -1) 
  {
    while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
    {
      linelen--;
    }
    editorAppendRow(line, linelen);
  }
  free(line);
  fclose(fp);
}

void editorMoveCursor(int key) 
{
  erow *row = (editor.cy >= editor.numrows) ? NULL : &editor.row[editor.cy];
  switch (key)
   {
    case ARROW_LEFT:
      if(editor.cx != 0)
      {
        editor.cx--;
        } else if (editor.cy > 0) {
        editor.cy--;
        editor.cx = editor.row[editor.cy].size;
      }
      break;
    case ARROW_RIGHT:
      if(row && editor.cx < row -> size)
      {    
        editor.cx++;
        } else if (row && editor.cx == row->size) {
        editor.cy++;
        editor.cx = 0;
      }
      break;
    case ARROW_UP:
      if(editor.cy != 0)
      {
        editor.cy--;
      }
      break;
    case ARROW_DOWN:
    if(editor.cy < editor.numrows)
    {
      editor.cy++;
    }
      break;
  }
  row = (editor.cy >= editor.numrows) ? NULL : &editor.row[editor.cy];
  int rowlen = row ? row->size : 0;
  if (editor.cx > rowlen) {
    editor.cx = rowlen;
  }
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
      if (editor.cy < editor.numrows)
        editor.cx = editor.row[editor.cy].size;
      break;

    case PAGE_UP:
    case PAGE_DOWN:
      {
        if (input == PAGE_UP) {
          editor.cy = editor.rowoff;
        } else if (input == PAGE_DOWN) {
          editor.cy = editor.rowoff + editor.screenrows - 1;
          if (editor.cy > editor.numrows) editor.cy = editor.numrows;
        }

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

void editorScroll() {
    editor.rx = 0;
    if (editor.cy < editor.numrows) {
        editor.rx = editorRowCxToRx(&editor.row[editor.cy], editor.cx);
    }

    if (editor.cy < editor.rowoff) {
        editor.rowoff = editor.cy;
    }
    if (editor.cy >= editor.rowoff + editor.screenrows) {
        editor.rowoff = editor.cy - editor.screenrows + 1;
    }
    if (editor.rx < editor.coloff) {
        editor.coloff = editor.rx;
    }
    if (editor.rx >= editor.coloff + editor.screencols) {
        editor.coloff = editor.rx - editor.screencols + 1;
    }
}

void editorDrawRows(struct abuf *ab) 
{
  int row;
  for (row = 0; row < editor.screenrows; row++) 
  {
      int filerow = row + editor.rowoff;
      if (filerow >= editor.numrows) {
        if (editor.numrows == 0 && row == editor.screenrows / 3) {
            char welcome[80];
            int welcomelen = snprintf(welcome, sizeof(welcome),
                "EPITOR -- version %s", EPITOR_VERSION);
            if (welcomelen > editor.screencols) welcomelen = editor.screencols;
            int padding = (editor.screencols - welcomelen) / 2;
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
      int len = editor.row[filerow].rsize - editor.coloff;
      if(len < 0) len = 0;
      if (len > editor.screencols) len = editor.screencols;
      abAppend(ab, &editor.row[filerow].render[editor.coloff], len);
    }
    abAppend(ab, "\x1b[K", 3);
    abAppend(ab, "\r\n", 2);
    }

}

void editorDrawStatusBar(struct abuf *ab) {
  abAppend(ab, "\x1b[7m", 4);
  char status[80], rstatus[80];
  int len = snprintf(status, sizeof(status), "%.20s - %d lines",
    editor.filename ? editor.filename : "[No Name]", editor.numrows);
  int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d",
    editor.cy + 1, editor.numrows);
  if (len > editor.screencols) len = editor.screencols;
  abAppend(ab, status, len);
  while (len < editor.screencols) {
    if (editor.screencols - len == rlen) {
      abAppend(ab, rstatus, rlen);
      break;
    } else {
    abAppend(ab, " ", 1);
    len++;
    }
  }
  abAppend(ab, "\x1b[m", 3);
  abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(struct abuf *ab) {
  abAppend(ab, "\x1b[K", 3);
  int msglen = strlen(editor.statusmsg);
  if (msglen > editor.screencols) msglen = editor.screencols;
  if (msglen && time(NULL) - editor.statusmsg_time < 5)
    abAppend(ab, editor.statusmsg, msglen);
}

void editorRefreshScreen()
{
    editorScroll();
    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);
    
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);
    editorDrawStatusBar(&ab);
    editorDrawMessageBar(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (editor.cy - editor.rowoff) + 1, (editor.rx - editor.coloff) + 1);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

void editorSetStatusMessage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(editor.statusmsg, sizeof(editor.statusmsg), fmt, ap);
  va_end(ap);
  editor.statusmsg_time = time(NULL);
}

/*** init ***/

void initEditor() 
{
  editor.cx = 0;
  editor.cy = 0;
  editor.rx = 0;
  editor.rowoff = 0;
  editor.coloff = 0;
  editor.numrows = 0;
  editor.row = NULL;
  editor.filename = NULL;
  editor.statusmsg[0] = '\0';
  editor.statusmsg_time = 0;

  if (getWindowSize(&editor.screenrows, &editor.screencols) == -1) die("getWindowSize");
  editor.screenrows -= 2;
}

/*** append buffer ***/


int main(int argc, char *argv[])
{
    enableRawMode();
    initEditor();
    if(argc >= 2)
    {
        editorOpen(argv[1]);
    }

    editorSetStatusMessage("HELP: Ctrl-Q = quit");

    while(1)
    {        
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}