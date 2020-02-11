#ifndef _OUTPUT_H_
#define _OUTPUT_H_
#define EPITOR_VERSION "0.0.1"
#define ABUF_INIT {NULL, 0}

#include "epitor.h"
#include "terminal.h"

void editorScroll();
void editorDrawRows(struct abuf *ab);
void editorDrawStatusBar(struct abuf *ab);
void editorDrawMessageBar(struct abuf *ab);
void editorRefreshScreen();
void editorSetStatusMessage(const char *fmt, ...);
void initEditor();
#endif
