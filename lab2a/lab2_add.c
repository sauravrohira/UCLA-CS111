#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <pthread.h>
#include <time.h>
#include <sched.h>
#include <string.h>

#define THREADS 'a'
#define ITERATIONS 'b'
#define YIELD 'c'
#define SYNC 'd'
int num_iterations = 1;
int num_threads = 1;
int opt_yield = 0;
int opt_sync = 0;
char* sync_type = NULL;
pthread_mutex_t m_lock;
int atomic_lock = 0;

    void add(long long *pointer, long long value) {

            if(opt_sync == 1 && *sync_type == 'c') 
            {
                int old_value, new_value;
                do
                {
                    old_value = *pointer;
                    new_value = old_value + value;
                    
                    if (opt_yield)
                        sched_yield();
                }
                while(__sync_val_compare_and_swap(pointer, old_value, new_value) != old_value);
            }
            else
            {
                if(opt_sync == 1 && *sync_type == 'm')
                    pthread_mutex_lock(&m_lock);
                if(opt_sync == 1 && *sync_type == 's') 
                    while(__sync_lock_test_and_set(&atomic_lock, 1));

                long long sum = *pointer + value;
                if (opt_yield)
                    sched_yield();
                *pointer = sum;
                if(opt_sync == 1 && *sync_type == 'm')
                    pthread_mutex_unlock(&m_lock);
                if(opt_sync == 1 && *sync_type == 's') 
                    __sync_lock_release(&atomic_lock);
            }
    }

//function for pthread to call;
void *add_ntimes(void * counter_address)
{
    long long *pointer;
    pointer = (long long *) counter_address;
    
    for(int i = 0; i < num_iterations; i++)
        add(pointer, 1);
    for(int i = 0; i < num_iterations; i++)
        add(pointer,-1);

    return NULL;
}

int main (int argc, char ** argv)
{
    //stores the parameter supplied to lab2_add:
    int param;

    // struct used to set up (optional) command line arguments to be identified by getopt_long():
    static struct option longopts[] = {
            {"threads", required_argument, NULL, THREADS},
            {"iterations", required_argument, NULL, ITERATIONS},
            {"yield", no_argument, NULL, YIELD},
            {"sync", required_argument, NULL, SYNC},
            {0,0,0,0}
        };

    while(1)
    {
        int option_index = 0;
        param = getopt_long(argc, argv, "", longopts, &option_index);

        if (param == -1)
            break;
        
        switch(param)
        {
            case THREADS:
                num_threads = atoi(optarg);
                break;
            
            case ITERATIONS:
                num_iterations = atoi(optarg);
                break;

            case YIELD:
                opt_yield = 1;
                break;

            case SYNC:
                opt_sync = 1;
                sync_type = optarg;
                if(strlen(optarg) > 1)
                {
                    fprintf(stderr, "Error: Incorrect argument provided to option --sync\n");
                    exit(1);
                }
                break;

            default:
                fprintf(stderr, "Error: Unrecognised option!\n");
                exit(1);
                break;
        }
    }

    //error checking for number of threads provided:
    if(num_threads < 0)
    {
        fprintf(stderr, "Error: Incorrect argument provided to option --threads\n");
        exit(1);
    }

    //error checking for number of threads provided:
    if(num_iterations < 0)
    {
        fprintf(stderr, "Error: Incorrect argument provided to option --threads\n");
        exit(1);
    }

    //initialises the counter modified by add:
    long long counter = 0;
    pthread_t threads[num_threads];
    struct timespec runtimes;
    long start_time;
    long end_time;
    long runtime;
    long num_operations;
    long avg_runtime;

    clock_gettime(CLOCK_REALTIME, &runtimes);
    start_time = runtimes.tv_nsec;

    if(opt_sync == 1 && *sync_type == 'm')
        if(pthread_mutex_init(&m_lock, NULL))
        {
            fprintf(stderr, "Error: Could not initialise mutex lock\n");
            exit(2);
        }

    for (int t = 0; t < num_threads; t++)
        if(pthread_create(&threads[t], NULL, add_ntimes, (void*) &counter))
        {
            fprintf(stderr, "Error: Could not initialise pthreads\n");
            exit(2);
        }

    for(int t = 0; t < num_threads; t++)
        if(pthread_join(threads[t], NULL))
        {
            fprintf(stderr, "Error: Could not join pthreads\n");
            exit(2);
        }
        
    if(opt_sync == 1 && *sync_type == 'm')
        pthread_mutex_destroy(&m_lock);
    
    clock_gettime(CLOCK_REALTIME, &runtimes);
    end_time = runtimes.tv_nsec;
    runtime = end_time - start_time;
    num_operations = num_threads * num_iterations * 2;
    avg_runtime = runtime/num_operations;
    
    fprintf(stdout, "add");
    if(opt_yield == 1)
        fprintf(stdout, "-yield");
    if(opt_sync == 1)
        fprintf(stdout, "-%s,", sync_type);
    else
        fprintf(stdout, "-none,");
    
    fprintf(stdout, "%d,%d,%ld,%ld,%ld,%lld\n", num_threads, num_iterations, num_operations, runtime, avg_runtime, counter);
    exit(0);
}