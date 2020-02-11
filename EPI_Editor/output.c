#include "output.h"

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
