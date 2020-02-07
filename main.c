#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

/*** data***/
#define CTRL_KEY(k) ((k) & 0x1f)

struct editor_config
{
    int screenrows;
    int screencols;
    struct termios original_terminal;
};

struct editor_config editor;

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

char editorReadKey() 
{
  int nread;
  char input;
  while ((nread = read(STDIN_FILENO, &input, 1)) != 1) 
  {
    if (nread == -1 && errno != EAGAIN)
         die("read");
  }
  return input;
}

int getCursorPosition(int *rows, int *cols) {
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
  printf("\r\n");
  char input;
  while (read(STDIN_FILENO, &input, 1) == 1) {
    if (iscntrl(input)) {
      printf("%d\r\n", input);
    } else {
      printf("%d ('%c')\r\n", input, input);
    }
  }
  editorReadKey();
  return -1;
}

int getWindowSize(int *rows, int *cols)
{
    struct winsize winsize;
    if(1 || ioctl(STDIN_FILENO,TIOCGWINSZ, &winsize) == -1 || winsize.ws_col == 0)
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
/*** input ***/
void editorProcessKeypress() 
{
  char input = editorReadKey();
  switch (input) 
  {
    case CTRL_KEY('q'):
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
        break;
  }
}

/*** output ***/

void editorDrawRows() 
{
  int row;
  for (row = 0; row < editor.screenrows; row++) 
  {
    write(STDOUT_FILENO, "~\r\n", 3);
  }
}

void editorRefreshScreen()
{
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  editorDrawRows();
  write(STDOUT_FILENO, "\x1b[H", 3);
}

/*** init ***/

void initEditor() 
{
  if (getWindowSize(&editor.screenrows, &editor.screencols) == -1) die("getWindowSize");
}


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
