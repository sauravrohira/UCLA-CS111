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

#define INPUT 'a'
#define OUTPUT 'b'
#define SEGFAULT 'c'
#define CATCH 'd'
#define DUMP 'e'

//function that deliberately generates a segmentation fault:
void forceSegfault()
{
    int* p = NULL;
    *p = 0;
}

void catchSegfault()
{
    fprintf(stderr, "Error: Segmentation Fault!");
    exit(4);
}

int main(int argc, char** argv) //parameters provided to allow for option handling.
{
    int c;

    // struct used to set up (optional) command line arguments to be identified by getopt_long():
    static struct option longopts[] = {
            {"input", required_argument, NULL, INPUT},
            {"output", required_argument, NULL, OUTPUT},
            {"segfault", no_argument, NULL, SEGFAULT},
            {"catch", no_argument, NULL, CATCH},
            {"dump-core", no_argument, NULL, DUMP},
            {0,0,0,0}
        };
        
    // loop that parses each argument in the stated order:
    while(1)
    {

        int option_index = 0;
        c = getopt_long (argc, argv, "i:o:scd", longopts, &option_index); //API used for argument parsing.

        //break out of loop if there are no options left to parse. (or none in the first place)
        if (c == -1)
            break;

        //switch case to define behavior based off which option is currently being provided.
        switch(c)
        {
            //handles input redirection.
            case INPUT: ;
                int ifd = open(optarg, O_RDONLY);
                if (ifd >= 0) 
                {
                    close(0);
                    dup(ifd);
                    close(ifd);
                }
                else 
                {
                    fprintf(stderr, "Could not open file %s. Error with argument %s: %s\n", optarg, longopts[option_index].name, strerror(errno));
                    exit(2);
                }
                break;

            //handles output redirection.
            case OUTPUT: ;
                int ofd = creat(optarg, 0666);
                if (ofd >= 0) 
                {
                    close(1);
                    dup(ofd);
                    close(ofd);
                }
                else 
                {
                    fprintf(stderr, "Could not create file %s. Error with argument %s: %s\n", optarg, longopts[option_index].name, strerror(errno));
                    exit(3);
                }
                break;

            //forces a segmentation fault.
            case SEGFAULT:
                forceSegfault();
                break;
            
            //generates signal handler to catch a segmentation fault.
            case CATCH:
                signal(SIGSEGV, catchSegfault); //Custom error function for segmentation faults.
                break;

            //dumps core on segmentation faults.
            case DUMP:
                signal(SIGSEGV, SIG_DFL); //Calls default behavior handling segmentation faults.
                break;

            //prints error message for invalid argument syntax.
            default:
                fprintf(stderr, "Error: argument number %d is invalid.\n", (option_index + 1));
                exit(1);
                break;
        }
    }

    //code to read from stdin and write to stdout
    char * buf = (char *) malloc (sizeof(char));
    int read_val = read(STDIN_FILENO, buf, 1);
    while( read_val > 0)
    {
        int write_val = write(STDOUT_FILENO, buf, 1);
        if (write_val < 0)
        {
            fprintf(stderr, "Error writing to output: %s\n", strerror(errno));
            exit(3);
        }
        read_val = read(STDIN_FILENO, buf, 1); 
    }
    if (read_val < 0)
    {
        fprintf(stderr, "Error reading from input: %s\n", strerror(errno));
        exit(2);
    }
    free(buf);
    exit(0);
}

