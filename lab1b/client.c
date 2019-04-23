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

//Global Constants:
#define BUF_SIZE 256
#define PORT 'a'
#define LOG 'b'
#define COMPRESS 'c'
#define KEYBOARD 0
#define SOCKET 1

//Global Variables:
struct termios default_mode; //stores the current terminal mode
struct termios project_mode; //stores modified mode for project

//variables used to 
int sockfd;
struct sockaddr_in serv_addr;
struct hostent *server;

int port_num;
int port_flag = 0;
char* log_name = NULL;
int log_fd;
int compress_flag = 0;

struct pollfd readPoll[2]; //array of pollfd structs used to set up polling
short poll_events = (POLL_IN | POLL_ERR | POLL_HUP); //events setting used by poll structures

char* buf;
int buf_len;

z_stream client2server;
z_stream server2client;

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

void setupCompression()
{
    client2server.zalloc = Z_NULL;
    client2server.zfree = Z_NULL;
    client2server.opaque = Z_NULL;

    if (deflateInit(&client2server, Z_DEFAULT_COMPRESSION) != Z_OK)
    {
        fprintf(stderr, "Error: could not initialize compression\n");
        exit(1);
    }

    server2client.zalloc = Z_NULL;
    server2client.zfree = Z_NULL;
    server2client.opaque = Z_NULL;

    if (inflateInit(&server2client) != Z_OK)
    {
        fprintf(stderr, "Error: could not initialize decompression\n");
        exit(1);
    }
}

void compressOutput(char* compress_buf, int* compress_bytes)
{
    client2server.avail_in = buf_len;
    client2server.next_in = (unsigned char *)buf;
    client2server.avail_out = 256;
    client2server.next_out = (unsigned char *)compress_buf;

    do
    {
        deflate(&client2server, Z_SYNC_FLUSH);
    } while (client2server.avail_in > 0);

    *compress_bytes = (256 - client2server.avail_in);
}

void decompressInput(char *decompress_buf, int *decompress_bytes)
{
    server2client.avail_in = buf_len;
    server2client.next_in = (unsigned char *)buf;
    server2client.avail_out = 1024;
    server2client.next_out = (unsigned char *)decompress_buf;
    do
    {
        inflate(&server2client, Z_SYNC_FLUSH);
    } while (server2client.avail_in > 0);

    *decompress_bytes = (1024 - client2server.avail_in);
}

void processInput(int source, char* buffer, int num_bytes, int log_bytes)
{
        for(int i = 0; i < num_bytes; i++)
        {
            switch(buf[i])
            {
                case '\r':
                case '\n':
                    callWrite(STDOUT_FILENO,"\r\n", 2);
                    if ((source == KEYBOARD) && (compress_flag == 0))
                        callWrite(sockfd, buffer + i, 1);
                    break;
                default:
                    callWrite(STDOUT_FILENO, buffer + i, 1);
                    if((source == KEYBOARD) && (compress_flag == 0))
                        callWrite(sockfd, buffer + i, 1);
                    break;
            }
        }

    if (log_name != NULL)
    {
        if (source == KEYBOARD)
            dprintf(log_fd, "SENT %d bytes: ", log_bytes);
        else
            dprintf(log_fd, "RECEIVED %d bytes: ", log_bytes);
        callWrite(log_fd, buffer, num_bytes);
        callWrite(log_fd, "\n", 1);
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
            callRead(STDIN_FILENO, buf, BUF_SIZE);

            if(compress_flag == 1)
            {
                char compress_buf[BUF_SIZE];
                int compress_bytes;
                compressOutput(compress_buf, &compress_bytes);
                processInput(KEYBOARD, buf, buf_len, compress_bytes);
            }
            else
                processInput(KEYBOARD, buf, buf_len, buf_len);
        }

        if(readPoll[1].revents & POLLIN)
        {
            callRead(sockfd, buf, BUF_SIZE);
            if (buf_len == 0)
            {
                callClose(sockfd);
                exit(0);
            }

            if(compress_flag == 1)
            {
                char decompress_buf[BUF_SIZE*4];
                int decompress_bytes;
                compressOutput(decompress_buf, &decompress_bytes);
                processInput(SOCKET, decompress_buf, decompress_bytes, buf_len);
            }
            else
                processInput(SOCKET, buf, buf_len, buf_len);
                
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
        {"compress", no_argument, NULL, COMPRESS},
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
                log_fd = creat(log_name, 0666);
                if (log_fd < 0)
                {
                    fprintf(stderr, "Error: %s\n", strerror(errno));
                    exit(1);
                }
                break;
            
            case COMPRESS:
                compress_flag = 1;
                setupCompression;
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