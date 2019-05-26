#include <stdio.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>
#include <poll.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <mraa.h>
#include <mraa/gpio.h>
#include <mraa/aio.h>

#define FAHRENHEIT 0
#define CELSIUS 1
#define TEMPERATURE_PIN 1
#define BUTTON_PIN 60
#define B 4275
#define R0 100000
#define buf_size 1024


sig_atomic_t volatile run_flag = 1;
int start_flag = 1;
int log_flag = 0;
int temp_unit = FAHRENHEIT;
float period = 1.0;
FILE* log_file = NULL;

void button_pressed()
{
    run_flag= 0;
}

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

void print_curr_time()
{
    time_t now;
    now = time(NULL);
    struct tm * curr_time_info;
    curr_time_info = localtime(&now);
    char curr_time[10];
    strftime(curr_time, sizeof(curr_time), "%H:%M:%S", curr_time_info);
    fprintf(stdout, "%s ", curr_time);
    if(log_flag == 1)
        fprintf(log_file, "%s ", curr_time);
}

void process_commands()
{
    char buf[buf_size];
    int read_val = read(STDIN_FILENO, buf, buf_size);
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
                    //nothing?
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
            default:
                exit(1);
                break;
        }
    }

    //set up button:
    mraa_gpio_context button;                                            //declare button
    button = mraa_gpio_init(BUTTON_PIN);                                 //initialize MRAA pin 60 as button
    mraa_gpio_dir(button, MRAA_GPIO_IN);                                 //configure button GPIO interface to be an input pin
    mraa_gpio_isr(button, MRAA_GPIO_EDGE_RISING, &button_pressed, NULL); //when button is pressed, calls button_pressed method

    //set up temperature sensor:
    uint16_t sensor_val;                          //stores return value of sensor
    mraa_aio_context temp_sensor;                 //declare sensor
    temp_sensor = mraa_aio_init(TEMPERATURE_PIN); //iniitalises MRAA pin 1 as sensor

    struct pollfd p;
    p.fd = STDIN_FILENO;
    p.events = POLLIN;

    while (run_flag)
    {
        if(start_flag)
        {
            sensor_val = mraa_aio_read(temp_sensor);        //reads from sensor
            float temperature = calculate_temp(sensor_val); //converts sensor return value to human-understandable temperature
            print_curr_time();
            fprintf(stdout, "%.1f\n", temperature);
            if(log_flag == 1)
                fprintf(log_file, "%.1f\n", temperature);
            usleep(period*1000000.0);
        }
        int command_entered = poll(&p, 1, 0);
        if(command_entered)
            process_commands();
    }

    print_curr_time();
    fprintf(stdout, "SHUTDOWN\n");
    if(log_flag == 1)
        fprintf(log_file, "SHUTDOWN\n");
    exit(0);
}