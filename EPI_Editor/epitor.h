#ifndef _EPITOR_H_
#define _EPITOR_H_

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

#include "terminal.h"

typedef struct erow {
  int size;
  int rsize;
  char *chars;
  char *render;
} erow;

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

int editorRowCxToRx(erow *row, int cx);
void editorUpdateRow(erow *row);
void editorAppendRow(char *s, size_t len);
void editorRowInsertChar(erow *row, int at, int c);
void editorRowDeleteChar(erow *row, int at);
void editorInsertChar(int c);
void editorDeleteChar();
/*** file i/o ***/
void editorOpen(char *filename);
/*** input ***/
void editorMoveCursor(int key);
void editorProcessKeypress();
void abAppend(struct abuf *ab, const char *s, int len);
void abFree(struct abuf *ab);

#endif
