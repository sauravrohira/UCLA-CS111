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
#include <zlib.h>

#define BUF_SIZE 256
#define PORT 'a'
#define COMPRESS 'b'
#define SOCKET 0
#define SHELL 1

int sockfd, newsockfd;
unsigned int clilen;
struct sockaddr_in serv_addr, cli_addr;

char *buf;
int buf_len;

int port_num;
int port_flag = 0;
int compress_flag = 0;

int shell_in[2];  //stores file descriptors for pipe to shell
int shell_out[2]; //stores file descriptors for pipe from shell
pid_t shell_pid = 0; //stores pid of child process that runs the shell

struct pollfd readPoll[2]; //array of pollfd structs used to set up polling
short poll_events = (POLL_IN | POLL_ERR | POLL_HUP); //events setting used by poll structures

z_stream server2client;
z_stream client2server;

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

//Calls read and prints error message on failure:
void callRead(int fd, char *buf, size_t num_bytes)
{
    buf_len = read(fd, buf, num_bytes);
    if (buf_len == -1)
    {
        fprintf(stderr, "Error: %s", strerror(errno));
        exit(1);
    }
}

//Calls write and prints error message on failure:
void callWrite(int fd, char *buf, size_t num_bytes)
{
    if (write(fd, buf, num_bytes) == -1)
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

    deflateEnd(&client2server);
    inflateEnd(&server2client);
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

void setupCompression()
{
    server2client.zalloc = Z_NULL;
    server2client.zfree = Z_NULL;
    server2client.opaque = Z_NULL;

    if (deflateInit(&server2client, Z_DEFAULT_COMPRESSION) != Z_OK)
    {
        fprintf(stderr, "Error: could not initialize compression\n");
        exit(1);
    }

    client2server.zalloc = Z_NULL;
    client2server.zfree = Z_NULL;
    client2server.opaque = Z_NULL;

    if (inflateInit(&client2server) != Z_OK)
    {
        fprintf(stderr, "Error: could not initialize decompression\n");
        exit(1);
    }
}

void compressOutput(char *compress_buf, int *compress_bytes)
{
    server2client.avail_in = buf_len;
    server2client.next_in = (unsigned char *)buf;
    server2client.avail_out = 256;
    server2client.next_out = (unsigned char *)compress_buf;

    do
    {
        deflate(&server2client, Z_SYNC_FLUSH);
    } while (server2client.avail_in > 0);

    *compress_bytes = (256 - server2client.avail_out);
}

void decompressInput(char *decompress_buf, int *decompress_bytes)
{
    client2server.avail_in = buf_len;
    client2server.next_in = (unsigned char *)buf;
    client2server.avail_out = 1024;
    client2server.next_out = (unsigned char *)decompress_buf;

    do
    {
        inflate(&client2server, Z_SYNC_FLUSH);
    } while (client2server.avail_in > 0);

    *decompress_bytes = (1024 - client2server.avail_out);
}

void processInput(int source, char* buffer, int num_bytes)
{
    for(int i = 0; i < num_bytes; i++)
    {
        switch(buffer[i])
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
                    callWrite(shell_in[1], "\n", 1);
                else 
                    callWrite(newsockfd, buffer + i, 1);
                break;
            default:
                if(source == SOCKET)
                    callWrite(shell_in[1], buffer + i, 1);
                else 
                    callWrite(newsockfd, buffer + i, 1);
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
            callRead(newsockfd, buf, BUF_SIZE);
            if(compress_flag == 0)
                processInput(SOCKET, buf, buf_len);
            else
            {
                int compress_bytes;
                char compression_buf[1024];
                decompressInput(compression_buf, &compress_bytes);
                processInput(SOCKET, compression_buf, compress_bytes);
            }
        }

        if (readPoll[1].revents & POLLIN)
        {
            callRead(shell_out[0], buf, BUF_SIZE);
            if(compress_flag == 0)
                processInput(SHELL, buf, buf_len);
            else
            {
                int compress_bytes;
                char compression_buf[256];
                compressOutput(compression_buf, &compress_bytes);
                callWrite(newsockfd, compression_buf, compress_bytes);
            }
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
        {"compress", no_argument, NULL, COMPRESS},
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

            case COMPRESS:
                compress_flag = 1;
                setupCompression();
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