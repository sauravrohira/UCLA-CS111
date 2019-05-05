#include <stdio.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>
#include <poll.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <mraa.h>
#include <mraa/aio.h>

#define FAHRENHEIT 0
#define CELSIUS 1
#define TEMPERATURE_PIN 1
#define BUTTON_PIN 60
#define B 4275
#define R0 100000
#define buf_size 512


sig_atomic_t volatile run_flag = 1;
int start_flag = 1;
int log_flag = 0;
int temp_unit = FAHRENHEIT;
float period = 1.0;
FILE* log_file = NULL;
char* id_num = NULL;
int port_num = -1;
char* hostname = NULL;

struct hostent* host;
int socket_fd;
struct sockaddr_in server;

float convert_C2F(float celsius) 
{
    float fahrenheit = celsius*1.8 + 32.0;
    return fahrenheit;
}

float calculate_temp(uint16_t value)
{
    float R = 1023.0/value - 1.0;
    R *= R0;
    float celsius = 1.0/(log(R/R0)/B + 1/298.15) - 273.15;

    if(temp_unit == FAHRENHEIT)
        return convert_C2F(celsius);
    else
        return celsius;
}

void print_output(float temp)
{
    time_t now;
    now = time(NULL);
    struct tm * curr_time_info;
    curr_time_info = localtime(&now);
    char curr_time[10];
    strftime(curr_time, sizeof(curr_time), "%H:%M:%S", curr_time_info);
    dprintf(socket_fd, "%s %.1f\n", curr_time, temp);
    if (log_flag == 1)
        fprintf(log_file, "%s %.1f\n", curr_time, temp);
}

void print_shutdown()
{
    time_t now;
    now = time(NULL);
    struct tm *curr_time_info;
    curr_time_info = localtime(&now);
    char curr_time[10];
    strftime(curr_time, sizeof(curr_time), "%H:%M:%S", curr_time_info);

    dprintf(socket_fd, "%s SHUTDOWN\n", curr_time);
    if (log_flag == 1)
        fprintf(log_file, "%s SHUTDOWN\n", curr_time);
}

void process_commands()
{
    char buf[buf_size];
    int read_val = read(socket_fd, buf, buf_size);
    if (read_val == -1)
    {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        exit(1);
    }
    if(read_val)
    {
        int i;
        int start_index = 0;
        for(i = 0; i < read_val; i++)
        {
            if(buf[i] == '\n')
            {
                buf[i] = 0;
                if(log_flag == 1)
                    fprintf(log_file, "%s\n", buf + start_index);

                if (strcmp(buf + start_index, "START") == 0)
                {
                    start_flag = 1;
                }
                if(strncmp(buf + start_index, "SCALE=", 6) == 0)
                {
                    if(strlen(buf + start_index) == 7 && buf[i - 1] == 'C')
                        temp_unit = CELSIUS;
                    if (strlen(buf + start_index) == 7 && buf[i - 1] == 'F')
                        temp_unit = FAHRENHEIT;
                }
                if(strncmp(buf + start_index, "PERIOD=", 7) == 0)
                {
                    char* period_str = buf + start_index + (i - 1);
                    float period_val = atoll(period_str);
                    if(period_val != 0)
                        period = period_val;
                }
                if(strcmp(buf + start_index, "STOP") == 0)
                {
                    start_flag = 0;
                }
                if(strncmp(buf + start_index, "LOG ", 4) == 0)
                {
                    //nothing
                }
                if(strcmp(buf + start_index, "OFF") == 0)
                {
                    run_flag = 0;
                }
                start_index = i + 1;
            }
        }
    }
}


int main(int argc, char** argv)
{
    //variable to store the return value of getopt:
    int c;

    // defines the structure of the options provided to the lab4b executable
    static struct option longopts[] = {
        {"period", required_argument, NULL, 'p'},
        {"scale", required_argument, NULL, 's'},
        {"log", required_argument, NULL, 'l'},
        {"id", required_argument, NULL, 'i'},
        {"host", required_argument, NULL, 'h'},
        {0, 0, 0, 0}};

    //loop moves from each longopt to the next longopt
    while (1)
    {
        int option_index = 0; // stores the index of option in struct
        c = getopt_long(argc, argv, "", longopts, &option_index);

        //breaks out of loop once all options have been parsed:
        if (c == -1)
            break;

        switch(c)
        {
            case 'p':
                period = atof(optarg);
                if(period == 0)
                {
                    fprintf(stderr, "Error: arguments to --period must be a valid time in seconds!\n");
                    exit(1);
                }
                break;
            case 's':
                if(strcmp(optarg, "C") == 0)
                    temp_unit = CELSIUS;
                else if(strcmp(optarg, "F") == 0)
                    temp_unit = FAHRENHEIT;
                else
                {
                    fprintf(stderr, "Error: arguments to --scale must be in the form \"F\" or \"C\"\n");
                    exit(1);
                }
                break;
            case 'l':;
                char * filename = optarg;
                log_file = fopen(filename, "w+");
                if(log_file == NULL)
                {
                    fprintf(stderr, "%s\n", strerror(errno));
                }
                log_flag = 1;
                break;
            case 'i':
                id_num = optarg;
                if(strlen(id_num) < 9 || atoi(id_num) == 0)
                {
                    fprintf(stderr, "Error: arguments to the option --id must be 9 digit integers only!\n");
                    exit(1);
                } 
                break;
            case 'h':
                hostname = optarg;
                break;
            default:
                exit(1);
                break;
        }
    }


    if(optind < argc)
        port_num = atoi(argv[optind]);
    if(port_num == -1)
    {
        fprintf(stderr, "Error: call to lab4c_tcp has mandatory non-switch parameter: port-num (int)\n");
        exit(1);
    }
    if(port_num == 0)
    {
        fprintf(stderr, "Error: non-switch parameter must be of type:integer\n");
        exit(1);
    }

    if(log_flag == 0 || hostname == NULL || id_num == NULL)
    {
        fprintf(stderr, "Error: call to lab4c_tcp has mandatory parameters: --log=filename --id=9-digit-integer --host=server-hostname!\n");
        exit(1);
    }

    //create socket:
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_fd == -1)
    {
        fprintf(stderr, "Error: could not create socket!\n");
        exit(1);
    }

    host = gethostbyname(hostname);
    if(host == NULL)
    {
        fprintf(stderr, "Error: could not find host with name: %s\n", hostname);
        exit(1);
    }

    //connect socket:
    server.sin_family = AF_INET;
    server.sin_port = htons(port_num);

    bzero((void*)&(server.sin_addr.s_addr), sizeof(server.sin_addr.s_addr));
    bcopy((void *)host->h_addr, (void *)&server.sin_addr.s_addr, host->h_length);

    if (connect(socket_fd, (struct sockaddr*) &server, sizeof(struct sockaddr)) == -1)
    {
        fprintf(stderr, "Error: could not connect to remote host! hostname:%s, port:%d\n", hostname, port_num);
        exit(1);
    }

    //set up temperature sensor:
    uint16_t sensor_val;                          //stores return value of sensor
    mraa_aio_context temp_sensor;                 //declare sensor
    temp_sensor = mraa_aio_init(TEMPERATURE_PIN); //iniitalises MRAA pin 1 as sensor

    struct pollfd p;
    p.fd = socket_fd;
    p.events = POLLIN;

    dprintf(socket_fd, "ID=%s\n", id_num);
    if (log_flag == 1)
        fprintf(log_file, "ID=%s\n", id_num);
    while (run_flag)
    {
        if(start_flag)
        {
            sensor_val = mraa_aio_read(temp_sensor);        //reads from sensor
            float temperature = calculate_temp(sensor_val); //converts sensor return value to human-understandable temperature
            print_output(temperature);
            usleep(period*1000000.0);
        }
        int command_entered = poll(&p, 1, 0);
        if(command_entered)
            process_commands();
    }

    print_shutdown();
    exit(0);
}