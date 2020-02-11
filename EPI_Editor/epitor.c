#include "epitor.h"

/*** data***/

#define EPITOR_TAB_STOP 8
#define CTRL_KEY(k) ((k) & 0x1f)


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

void editorRowInsertChar(erow *row, int at, int c) {
  if (at < 0 || at > row->size)
    at = row->size;
  row->chars = realloc(row->chars, row->size + 2);
  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
  row->size++;
  row->chars[at] = c;
  editorUpdateRow(row);
}

void editorRowDeleteChar(erow *row, int at) {
  if (at < 0 || at > row->size)
    return;
  memmove(&row->chars[at], &row->chars[at+1], row->size - at);
  row->size--;
  editorUpdateRow(row);
}
/*** editor operations ***/

void editorInsertChar(int c) {
  if (editor.cy == editor.numrows) {
    editorAppendRow("", 0);
  }
  editorRowInsertChar(&editor.row[editor.cy], editor.cx, c);
  editor.cx++;
}

void editorDeleteChar() {
  if (editor.cy == editor.numrows) {
    return;
  }
  erow *row = &editor.row[editor.cy];
  if (editor.cx > 0) {
    editorRowDeleteChar(row, editor.cx - 1);
    editor.cx--;
  }
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

/*** input ***/

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


void editorProcessKeypress()
{
  int input = editorReadKey();
  switch (input)
  {
    case '\r':
      /* TODO */
      break;

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

    case BACKSPACE:
    case CTRL_KEY('h'):
    case DEL_KEY:
      editorDeleteChar();
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

    case CTRL_KEY('l'):
    case '\x1b':
      break;

    default:
      editorInsertChar(input);
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


/*int main(int argc, char *argv[])
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
}*/
