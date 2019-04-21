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

//Global Constants:
#define BUF_SIZE 256
#define KEYBOARD 0
#define SERVER 1
#define PORT 'a'
#define LOG 'b'
#define COMPRESS 'c'
#define HOST 'd'

//Global Variables:
struct termios default_mode; //stores the current terminal mode
struct termios project_mode; //stores modified mode for project
int shell_flag = 0; //flag used to identify if shell option was provided
int shell_in[2]; //stores file descriptors for pipe to shell
int shell_out[2]; //stores file descriptors for pipe from shell
char* buf; //buffer used for reading from shell and keyboard and writing to shell and screen
int buf_len = 0; //stores number of bytes read by most recent read call
struct pollfd readPoll[2]; //array of pollfd structs used to set up polling
pid_t shell_pid = 0; //stores pid of child process that runs the shell
short poll_events = (POLL_IN | POLL_ERR | POLL_HUP); //events setting used by poll structures
int port_flag = 0;
int port_num = -1;
int socket_fd;
char* log_name = NULL;
FILE* log_file;
char * hostname = "localhost";
struct hostent *host;
struct sockaddr_in server;

//Function called when the shell creates a SIGPIPE:
void handleSignal(int signal_val)
{
    if(signal_val == SIGPIPE)
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
    if(dup2(new_fd, old_fd) < 0)
    {
        fprintf(stderr, "Error: %s", strerror(errno));
        exit(1);
    }
}

//Sets up the shell and pipes for communication with the shell:
void setupShell()
{
    if(signal(SIGPIPE, handleSignal) == SIG_ERR)
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

    if(shell_pid == 0)
    {

        callClose(shell_in[1]);
        callClose(shell_out[0]);
        redirectFD(STDIN_FILENO, shell_in[0]);
        redirectFD(STDOUT_FILENO, shell_out[1]);
        redirectFD(STDERR_FILENO, shell_out[1]);

        char* shell_path = "/bin/bash";
        char* args[2] = { shell_path, NULL };

        if(execvp(shell_path, args) == -1)
        {
            fprintf(stderr, "Error: %s", strerror(errno));
            exit(1);
        }
    }

    if(shell_pid > 0)
    {
        callClose(shell_in[0]);
        callClose(shell_out[1]);
    }
}

//Set terminal to character-at-a-time, no-echo mode:
void setupTerminal()
{
    tcgetattr(0, &default_mode); //Regular mode stored in default_mode.
    project_mode = default_mode; 
    project_mode.c_cflag = ISTRIP;
    project_mode.c_oflag = 0;
    project_mode.c_lflag = 0;
    tcsetattr(0, TCSANOW, &project_mode); //New modified mode is made active.
}

//Function called upon exit, sets terminal back to its default mode:
void restoreTerminal()
{
    free(buf);
    tcsetattr(0, TCSANOW, &default_mode);

    //prints shell exit status:
    if(shell_flag == 1)
    {
        int exit_status;
        if(waitpid(shell_pid, &exit_status, 0) < 0)
        {
            fprintf(stderr, "Error: %s", strerror(errno));
            exit(1);
        }
        if(WIFEXITED(exit_status))
            fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WTERMSIG(exit_status), WEXITSTATUS(exit_status));
    }
}

//Calls read and prints error message on failure:
void callRead(int fd, char* buf, size_t num_bytes)
{
    buf_len = read(fd, buf, num_bytes);
    if (buf_len == -1)
    {
        fprintf(stderr, "Error: %s", strerror(errno));
        exit(1);
    }
}

//Calls write and prints error message on failure:
void callWrite(int fd, char* buf, size_t num_bytes)
{
    if (write(fd, buf, num_bytes) == -1)
    {
        fprintf(stderr, "Error: %s", strerror(errno));
        exit(1);
    }
}

//Processes characters read from keyboard and shell and prints them:
void processedWrite(int source) 
{
    for(int i = 0; i < buf_len; i++)
    {
        callWrite(STDOUT_FILENO, buf + i, 1);
        if (source == KEYBOARD)
            callWrite(socket_fd, buf + i, 1);
    }

    if(log_name != NULL)
    {
        if(source == KEYBOARD)
            fprintf(log_file, "SENT %d bytes: %s\n", buf_len, buf);
        else
            fprintf(log_file, "RECIEVED %d bytes: %s\n", buf_len, buf);
    }
}

//Infinite loop for echo and shell mode:
void runClient()
{
    readPoll[0].fd = STDIN_FILENO;
    readPoll[1].fd = socket_fd;
    readPoll[0].events = readPoll[1].events = poll_events;

    while(1)
    {
        int poll_return = poll(readPoll, 2, 0);

        if(poll_return < 0)
        {
            fprintf(stderr, "Error: %s", strerror(errno));
            exit(1);
        }

        if(poll_return == 0)
            continue;
        
        if(readPoll[0].revents & POLLIN)
        {
            callRead(STDIN_FILENO, buf, BUF_SIZE);
            processedWrite(KEYBOARD);
        }

        if(readPoll[1].revents & POLLIN)
        {
            callRead(socket_fd, buf, BUF_SIZE);
            processedWrite(SERVER);
        }
        
        if(readPoll[1].revents & POLLERR)
        {
            fprintf(stdout, "ERR\n");
            exit(0);
        }

        if (readPoll[1].revents & POLLHUP)
        {
            fprintf(stdout, "HUP\n");
            exit(0);
        }
    }
}

int main(int argc, char ** argv)
{

    //variable used to store getopt_long return values
    int c;
    // struct used to identify --shell option and report incorrect options
    static struct option longopts[] = {
        {"port", required_argument, NULL, PORT},
        {"log", required_argument, NULL, LOG},
        {"compress", no_argument, NULL, COMPRESS},
        {"host", required_argument, NULL, HOST},
        {0, 0, 0, 0}
    };

    // loop that parses each option provided:
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
            
            case COMPRESS:
                break;

            case HOST:
                hostname = optarg;
                break;

            default:
                exit(1);
                break;
        }
    }

    //checks for mandatory port option:
    if(port_flag == 0)
    {
        fprintf(stderr, "Error: mandatory option --port=port# not provided\n");
        exit(1);
    }
    
    //initialize buffer for reads and writes:
    buf = (char*) malloc(BUF_SIZE);

    //create socket:
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_fd < 0)
    {
        fprintf(stderr, "Error: %s", strerror(errno));
        exit(1);
    }

    //processing host provided or localhost (default):
    host = gethostbyname(hostname);
    if (host == NULL)
    {
        fprintf(stderr, "Error: could not find host with name: %s\n", hostname);
        exit(1);
    }

    //connect client socket:
    bzero((char *)&server, sizeof(server));
    server.sin_family = AF_INET;
    bcopy((char *)host->h_addr,
          (char *)&server.sin_addr.s_addr,
          host->h_length);
    server.sin_port = htons(port_num);

    if (connect(socket_fd, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        fprintf(stderr, "Error: could not connect to remote host! hostname:%s, port:%d\n", hostname, port_num);
        exit(1);
    }

    setupTerminal();         //sets up new terminal mode.
    atexit(restoreTerminal); //sets function to be called on exit.
    runClient();
}

