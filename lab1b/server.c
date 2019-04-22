#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <termios.h>
#include <poll.h>
#include <netdb.h>

#define BUF_SIZE 256
#define PORT 'a'
#define SOCKET 0
#define SHELL 1

int sockfd, newsockfd;
unsigned int clilen;
struct sockaddr_in serv_addr, cli_addr;

char *buf;
int buf_len;

int port_num;
int port_flag = 0;

int shell_in[2];  //stores file descriptors for pipe to shell
int shell_out[2]; //stores file descriptors for pipe from shell
pid_t shell_pid = 0; //stores pid of child process that runs the shell

struct pollfd readPoll[2]; //array of pollfd structs used to set up polling
short poll_events = (POLL_IN | POLL_ERR | POLL_HUP); //events setting used by poll structures

//Function called when the shell creates a SIGPIPE:
void handleSignal(int signal_val)
{
    if (signal_val == SIGPIPE)
        exit(0);
}

//Calls close and prints error message on failure:
void callClose(int fd)
{
    if (close(fd) < 0)
    {
        fprintf(stderr, "Error: %s", strerror(errno));
        exit(1);
    }
}

//Used to redirect input and output:
void redirectFD(int old_fd, int new_fd)
{
    callClose(old_fd);
    if (dup2(new_fd, old_fd) < 0)
    {
        fprintf(stderr, "Error: %s", strerror(errno));
        exit(1);
    }
}

void restore()
{
    free(buf);
    //prints shell exit status:
    int exit_status;
    if (waitpid(shell_pid, &exit_status, 0) < 0)
    {
        fprintf(stderr, "Error: %s", strerror(errno));
        exit(1);
    }
    if (WIFEXITED(exit_status))
        fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WTERMSIG(exit_status), WEXITSTATUS(exit_status));
    callClose(newsockfd);
}

//Sets up the shell and pipes for communication with the shell:
void setupShell()
{
    if (signal(SIGPIPE, handleSignal) == SIG_ERR)
    {
        fprintf(stderr, "Error: %s", strerror(errno));
        exit(1);
    }

    pipe(shell_in);
    pipe(shell_out);
    shell_pid = fork();

    if (shell_pid == -1)
    {
        fprintf(stderr, "Error: %s", strerror(errno));
        exit(1);
    }

    if (shell_pid == 0)
    {

        callClose(shell_in[1]);
        callClose(shell_out[0]);
        redirectFD(STDIN_FILENO, shell_in[0]);
        redirectFD(STDOUT_FILENO, shell_out[1]);
        redirectFD(STDERR_FILENO, shell_out[1]);

        char *shell_path = "/bin/bash";
        char *args[2] = {shell_path, NULL};

        if (execvp(shell_path, args) == -1)
        {
            fprintf(stderr, "Error: %s", strerror(errno));
            exit(1);
        }
    }

    if (shell_pid > 0)
    {
        callClose(shell_in[0]);
        callClose(shell_out[1]);
    }
}

void processInput(int source)
{
    for(int i = 0; i < buf_len; i++)
    {
        switch(buf[i])
        {
            case 3:
                if (kill(shell_pid, SIGINT) < 0)
                {
                    fprintf(stderr, "Error: %s", strerror(errno));
                    exit(1);
                }
                break;
            case 4:
                callClose(shell_in[1]);
                exit(0);
                break;
            case '\r':
            case '\n':
                if(source == SOCKET)
                    write(shell_in[1], "\n", 1);
                else 
                    write(newsockfd, buf + i, 1);
                break;
            default:
                if(source == SOCKET)
                    write(shell_in[1], buf + i, 1);
                else 
                    write(newsockfd, buf + i, 1);
                break;
        }
    }
}

void runServer()
{
    readPoll[0].fd = newsockfd;
    readPoll[1].fd = shell_out[0]; 
    readPoll[0].events = readPoll[1].events = poll_events;

    while (1)
    {
        int poll_return = poll(readPoll, 2, 0);

        if (poll_return < 0)
        {
            fprintf(stderr, "Error: %s", strerror(errno));
            exit(1);
        }

        if (poll_return == 0)
            continue;

        if (readPoll[0].revents & POLLIN)
        {
            buf_len = read(newsockfd, buf, BUF_SIZE);
            processInput(SOCKET);
        }

        if (readPoll[1].revents & POLLIN)
        {
            buf_len = read(shell_out[0], buf, BUF_SIZE);
            processInput(SHELL);
        }

        if (readPoll[1].revents & (POLLHUP | POLLERR))
        {
            exit(0);
        }
    }
}

int main(int argc, char** argv)
{
    //variable used to store getopt_long return values
    int c;
    // struct used to identify --shell option and report incorrect options
    static struct option longopts[] = {
        {"port", required_argument, NULL, PORT},
        {0, 0, 0, 0}};

    int option_index = 0;
    while (1)
    {
        c = getopt_long(argc, argv, "", longopts, &option_index);

        //breaks out of loop once all options have been parsed
        if (c == -1)
            break;

        switch (c)
        {
            case PORT:
                port_flag = 1;
                port_num = atoi(optarg);
                break;

            default:
                exit(1);
                break;
        }
    }

    //checks for mandatory port option:
    if (port_flag == 0)
    {
        fprintf(stderr, "Error: mandatory option --port=port# not provided\n");
        exit(1);
    }

    //initialize buffer for reads and writes:
    buf = (char *)malloc(BUF_SIZE);

    //setting up socket for connection to client:
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        exit(1);
    }
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port_num);
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        exit(1);
    }
    listen(sockfd, 5);
    clilen = sizeof(cli_addr);
    newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
    if (newsockfd < 0)
    {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        exit(1);
    }

    setupShell();
    atexit(restore);
    runServer();
}