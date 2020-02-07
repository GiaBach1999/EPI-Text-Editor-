#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>

/*** data***/
#define EPITOR_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT {NULL, 0}

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
void editorMoveCursor(int key) 
{
  switch (key)
   {
    case ARROW_LEFT:
      if(editor.cx != 0)
      {
        editor.cx--;
      }
      break;
    case ARROW_RIGHT:
      if(editor.cx != editor.screencols - 1)
      {
        editor.cx++;
      }
      break;
    case ARROW_UP:
      if(editor.cy != 0)
      {
        editor.cy--;
      }
      break;
    case ARROW_DOWN:
    if(editor.cy != editor.screenrows -1)
    {
      editor.cy++;
    }
      break;
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

void editorDrawRows(struct abuf *ab) 
{
  int row;
  for (row = 0; row < editor.screenrows; row++) 
  {
    if (row == editor.screenrows / 3) {
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
    abAppend(ab, "\x1b[K", 3);
    if (row < editor.screenrows - 1) {
      abAppend(ab, "\r\n", 2);
    }
  }
}

void editorRefreshScreen()
{
  struct abuf ab = ABUF_INIT;

  abAppend(&ab, "\x1b[?25l", 6);
  
  abAppend(&ab, "\x1b[H", 3);

  editorDrawRows(&ab);

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", editor.cy + 1, editor.cx + 1);
  abAppend(&ab, buf, strlen(buf));

  //abAppend(&ab, "\x1b[H", 3);
  abAppend(&ab, "\x1b[?25h", 6);

  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

/*** init ***/

void initEditor() 
{
  editor.cx = 0;
  editor.cy = 0;
  if (getWindowSize(&editor.screenrows, &editor.screencols) == -1) die("getWindowSize");
}

/*** append buffer ***/


int main()
{
    enableRawMode();
    initEditor();
    while(1)
    {        
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}
