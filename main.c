#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

struct termios original_terminal;
void disableRawMode()
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_terminal);
}
void enableRawMode()
{
    struct termios raw;
    //get attributes of the terminal
    tcgetattr(STDIN_FILENO, &original_terminal);
    //disable raw mode at exit
    atexit(disableRawMode);
    //turn off echo function of the terminal so what you're typing won't appear
    struct termios raw = original_terminal;
    raw.c_lflag &= ~(ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main()
{
    enableRawMode();
    char input;
    while(read(STDIN_FILENO,&input,1) == 1 && input != 'q');
    return 0;
}
