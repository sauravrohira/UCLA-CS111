#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <termios.h>
#include <poll.h>

#define BUF_SIZE 256
#define KEYBOARD 0
#define SHELL 1

struct termios default_mode; //stores the current terminal mode
struct termios project_mode; //stores modified mode for project
int shell_flag = 0; //flag used to identify if shell option was provided
int shell_in[2];
int shell_out[2];
char* buf;
int buf_len = 0;
struct pollfd readPoll[2];
pid_t child_pid = 0;
short poll_events = (POLL_IN | POLL_ERR | POLL_HUP);

void redirectFD(int old_fd, int new_fd)
{
    close(old_fd);
    dup2(new_fd, old_fd);
}

void setupShell()
{
    pipe(shell_in);
    pipe(shell_out);
    child_pid = fork();

    if (child_pid == -1)
    {
        //ERROR!
    }

    if(child_pid == 0)
    {
        redirectFD(STDIN_FILENO, shell_in[0]);
        redirectFD(STDOUT_FILENO, shell_out[1]);
        redirectFD(STDERR_FILENO, shell_out[1]);

        char* shell_path = "/bin/bash";
        char* args[2] = { shell_path, NULL };

        if(execvp(shell_path, args) == -1)
        {
            //ERROR!
        }
    }
}

void setupTerminal()
{
    tcgetattr(0, &default_mode);
    project_mode = default_mode;
    project_mode.c_cflag = ISTRIP;
    project_mode.c_oflag = 0;
    project_mode.c_lflag = 0;
    tcsetattr(0, TCSANOW, &project_mode);
}

void restoreTerminal()
{
    tcsetattr(0, TCSANOW, &default_mode);
}

void callRead(int fd, char* buf, size_t num_bytes)
{
    if(read(fd, buf, num_bytes) == -1)
    {
        fprintf(stderr, "Error: %s", strerror(errno));
        exit(1);
    }
}

void callWrite(int fd, char* buf, size_t num_bytes)
{
    if (write(fd, buf, num_bytes) == -1)
    {
        fprintf(stderr, "Error: %s", strerror(errno));
        exit(1);
    }
}

void processedWrite(int source) 
{
    for(int i = 0; i < buf_len; i++)
    {
        switch(buf[i])
        {
            case 3:

                break;
            case 4:
                if(source == KEYBOARD)
                    exit(0);
            case '\r':
            case '\n':
                callWrite(STDOUT_FILENO, "\r\n", 2);
                if(shell_flag == 1 && source == KEYBOARD)
                    callWrite(shell_in[1], "\n", 1);
                break;
            default:
                callWrite(STDOUT_FILENO, buf + i, 1);
                if(shell_flag == 1 && source == KEYBOARD)
                    callWrite(shell_in[1], buf + i, 1);
                break;
        }
    }
}

void readMode()
{
    while(1)
    {
        callRead(STDIN_FILENO, buf, BUF_SIZE);
        processedWrite(KEYBOARD);
    }
}

void pollMode()
{
    readPoll[0].fd = STDIN_FILENO;
    readPoll[1].fd = child_pid;
    readPoll[0].events = readPoll[1].events = poll_events;

    while(1)
    {
        int poll_return = poll(readPoll, 2, 0);
        if(poll_return < 0)
        {
            //ERROR!
        }

        if(poll_return == 0)
            continue;
        
        if(readPoll[0].revents & POLL_IN)
        {
            callRead(STDIN_FILENO, buf, BUF_SIZE);
            processedWrite(KEYBOARD);
        }

        if(readPoll[1].revents & POLL_IN)
        {
            do
            {
                callRead(shell_out[0], buf, BUF_SIZE);
                processedWrite(SHELL);
            }
            while(buf_len > 0);
        }
    }
}

int main(int argc, char ** argv)
{
    setupTerminal();
    atexit(restoreTerminal);

    //variable used to store getopt_long return values
    int c;
    // struct used to identify --shell option and report incorrect options
    static struct option longopts[] = {
        {"shell", no_argument, &shell_flag, 1}, 
        {0, 0, 0, 0}
    };

    // loop that parses each option provided:
    int option_index = 0;
    while (1)
    {
        c = getopt_long(argc, argv, "", longopts, &option_index);
        if(c == -1)
            break;
        if(c != 0)
            exit(1);
    }

    if(shell_flag == 1)
        setupShell();
    
}

