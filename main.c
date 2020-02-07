#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

struct termios original_terminal;

void die(const char *s)
{
    perror(s);
    exit(1);
}

//turn off raw mode
void disableRawMode()
{
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_terminal) == -1)
        die("tcsetattr");
}
//turn on raw mode
void enableRawMode()
{
    struct termios raw;
    //get attributes of the terminal
    if(tcgetattr(STDIN_FILENO, &original_terminal) == -1)
        die("tcsetattr");
    //disable raw mode at exit
    atexit(disableRawMode);
    //turn off echo function of the terminal so what you're typing won't appear
    raw = original_terminal;
    raw.c_iflag &= ~(ICRNL| IXON | ISTRIP | INPCK | BRKINT);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= ~(CS8);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr");
}

int main()
{
    enableRawMode();
    char input;
    while(1)
    {
        input = '\0';
        if(read(STDIN_FILENO, &input, 1) == -1 && errno != EAGAIN)
            die("read");
        if(iscntrl(input))
        {
            printf("%d\r\n",input);
        }
        else
        {
            printf("%d('%c')\r\n",input,input);
        }
        if(input == 'q')
            break;
    }
    return 0;
}
