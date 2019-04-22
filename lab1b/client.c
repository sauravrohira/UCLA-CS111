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
#define LOG 'b'
#define KEYBOARD 0
#define SOCKET 1

struct termios default_mode; //stores the current terminal mode
struct termios project_mode; //stores modified mode for project

int sockfd;
struct sockaddr_in serv_addr;
struct hostent *server;

int port_num;
int port_flag = 0;
char* log_name = NULL;
FILE* log_file;

struct pollfd readPoll[2]; //array of pollfd structs used to set up polling
short poll_events = (POLL_IN | POLL_ERR | POLL_HUP); //events setting used by poll structures

char* buf;
int buf_len;


void setupTerminal()
{
    tcgetattr(0, &default_mode); //Regular mode stored in default_mode.
    project_mode = default_mode;
    project_mode.c_cflag = ISTRIP;
    project_mode.c_oflag = 0;
    project_mode.c_lflag = 0;
    tcsetattr(0, TCSANOW, &project_mode); //New modified mode is made active.
}

void restoreTerminal()
{
    free(buf);
    tcsetattr(0, TCSANOW, &default_mode);
}

void processInput(int source)
{
    for(int i = 0; i < buf_len; i++)
    {
        switch(buf[i])
        {
            case 3:
                exit(0);
                break;
            case 4:
                break;
            case '\r':
            case '\n':
                write(STDOUT_FILENO,"\r\n", 2);
                if (source == KEYBOARD)
                    write(sockfd, buf + i, 1);
                break;
            default:
                write(STDOUT_FILENO, buf + i, 1);
                if(source == KEYBOARD)
                    write(sockfd, buf + i, 1);
                break;
        }
    }

    if (log_name != NULL)
    {
        if (source == KEYBOARD)
            fprintf(log_file, "SENT %d bytes: %s\n", buf_len, buf);
        else
            fprintf(log_file, "RECIEVED %d bytes: %s\n", buf_len, buf);
    }
}

void runClient()
{
    readPoll[0].fd = STDIN_FILENO;
    readPoll[1].fd = sockfd;
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

        if(readPoll[0].revents & POLLIN)
        {
            buf_len = read(STDIN_FILENO, buf, BUF_SIZE);
            processInput(KEYBOARD);
        }

        if(readPoll[1].revents & POLLIN)
        {
            buf_len = read(sockfd, buf, BUF_SIZE);
            processInput(SOCKET);
        }

        if (readPoll[1].revents & (POLLHUP | POLLERR))
        {
            exit(0);
        }
    }
}

int main(int argc, char **argv)
{
    //variable used to store getopt_long return values
    int c;
    // struct used to identify --shell option and report incorrect options
    static struct option longopts[] = {
        {"port", required_argument, NULL, PORT},
        {"log", required_argument, NULL, LOG},
        {0, 0, 0, 0}
    };

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
            
            case LOG:
                log_name = optarg;
                log_file = fopen(log_name, "w+");
                if (log_file == NULL)
                {
                    fprintf(stderr, "Error: %s\n", strerror(errno));
                }
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

    //setting up socket for connection to server:
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        exit(1);
    }
    server = gethostbyname("localhost");
    if (server == NULL)
    {
        fprintf(stderr, "ERROR, no such host\n");
        exit(1);
    }
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
            (char *)&serv_addr.sin_addr.s_addr,
            server->h_length);
    serv_addr.sin_port = htons(port_num);
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        exit(1);
    }

    setupTerminal();
    atexit(restoreTerminal);
    runClient();
}