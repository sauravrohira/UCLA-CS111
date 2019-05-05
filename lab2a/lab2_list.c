#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <pthread.h>
#include <time.h>
#include <sched.h>
#include <string.h>
#include <signal.h>
#include "SortedList.h"

#define THREADS 'a'
#define ITERATIONS 'b'
#define YIELD 'c'
#define SYNC 'd'

int num_threads = 1;
int num_iterations = 1;
SortedList_t* list_head;
SortedListElement_t* list_elements;
const char ** actual_keys;
int sync_flag = 0;
char* sync_type = NULL;
pthread_mutex_t m_lock;
int spin_lock = 0;

int num_elementstocreate;
int* random_keys;
int random_keys_maxindex;
int opt_yield = 0;

void signalHandler()
{
    fprintf(stderr, "Error: Segmentation Fault\n");
    exit(2);
}

const char* generateKey()
{
    srand(time(NULL));
    int random_index = rand() % (random_keys_maxindex + 1);
    int random_int_key = random_keys[random_index];
    random_keys[random_index] = random_keys[random_keys_maxindex];
    random_keys_maxindex--;
    char* random_key = (char*) malloc (12*sizeof(char));
    sprintf(random_key, "%d", random_int_key);
    return random_key;
}

void* listOperations(void* pthread_nums)
{
    int threadID = *((int*)pthread_nums);
    
    //Set locks if required:
    if(sync_flag == 1 && *sync_type == 'm')
    {
        pthread_mutex_lock(&m_lock);
    }

    if(sync_flag == 1 && *sync_type == 's') 
    {
        while(__sync_lock_test_and_set(&spin_lock, 1));
    }

    //insert elements into list:
    for(int i = threadID; i < num_elementstocreate; i += num_threads)
        SortedList_insert(list_head, list_elements + i);
    
    //check list length:
    int size = SortedList_length(list_head);    
    if (size != num_elementstocreate/num_threads)
        {
            fprintf(stderr, "Error: List Corrupted, list length incorrect!\n");
            exit(2);
        }
    
    for(int i = threadID; i < num_elementstocreate; i += num_threads)
        if(SortedList_delete(SortedList_lookup(list_head, actual_keys[i])) == 1)
        {
            fprintf(stderr, "Error: List Corrupted, key has been deleted before!\n");
            exit(2);
        }

    //release locks if they were set:
    if(sync_flag == 1 && *sync_type == 'm')
        pthread_mutex_unlock(&m_lock);
    if(sync_flag == 1 && *sync_type == 's') 
        __sync_lock_release(&spin_lock);
    return NULL;
}

int main(int argc, char** argv)
{
     //stores the parameter supplied to lab2_add:
    int param;

    // struct used to set up (optional) command line arguments to be identified by getopt_long():
    static struct option longopts[] = {
            {"threads", required_argument, NULL, THREADS},
            {"iterations", required_argument, NULL, ITERATIONS},
            {"yield", required_argument, NULL, YIELD},
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
                if(strlen(optarg) > 3)
                    exit(1);
                for(unsigned int i = 0; i < strlen(optarg); i++)
                {
                    if(optarg[i] == 'i')
                        opt_yield |= INSERT_YIELD;
                    if(optarg[i] == 'd')
                        opt_yield |= DELETE_YIELD;
                    if(optarg[i] == 'l')
                        opt_yield |= LOOKUP_YIELD;
                    if(optarg[i] != 'i' && optarg[i] != 'd' && optarg[i] != 'l')
                    {
                        fprintf(stderr, "Error: Incorrect argument provided to option --yield\n");
                        exit(1);
                    }
                }
                break;

            case SYNC:
                sync_flag = 1;
                if(strlen(optarg) > 1)
                {
                    fprintf(stderr, "Error: Incorrect argument provided to option --sync\n");
                    exit(1);
                }
                sync_type = optarg;
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

    //sets up signal handler for segmentation faults:
    signal(SIGSEGV, signalHandler);

    //sets up random key generation
    num_elementstocreate = num_threads*num_iterations;
    random_keys_maxindex = num_elementstocreate - 1;
    random_keys = (int*) malloc (num_elementstocreate*sizeof(int));
    for(int i = 0; i < num_elementstocreate; i++)
        random_keys[i] = i;
    actual_keys = (const char**) malloc (num_elementstocreate*sizeof(const char*));

    //initialises all elements that will be put in list:
    list_elements = (SortedListElement_t*) malloc(num_elementstocreate * sizeof(SortedListElement_t));
    for (int i = 0; i < num_elementstocreate; i++)
    {
        list_elements[i].next = NULL;
        list_elements[i].prev = NULL;
        list_elements[i].key = generateKey();
        actual_keys[i] = list_elements[i].key;
    }

    //initialises empty list:
    list_head = (SortedList_t*) malloc (sizeof(SortedList_t));
    list_head->next = list_head;
    list_head->prev = list_head;
    list_head->key = NULL;

    //define variables used to output csv:
    struct timespec start_time;
    struct timespec end_time;
    long long runtime;
    long num_operations;
    long long avg_runtime;

    //starts recording time:
    clock_gettime(CLOCK_REALTIME, &start_time);

    //create pthreads:
    int* pthread_nums = (int*) malloc (num_threads*sizeof(int));
    for(int t = 0; t < num_threads; t++)
        pthread_nums[t] = t;
    
    if(sync_flag == 1 && *sync_type == 'm')
        if(pthread_mutex_init(&m_lock, NULL))
        {
            fprintf(stderr, "Error: Could not initialise mutex lock\n");
            exit(2);
        }

    pthread_t* threads = (pthread_t*) malloc (num_threads*sizeof(pthread_t));
    for(int t = 0; t < num_threads; t++) 
    {
        if (pthread_create(&threads[t], NULL, listOperations, (void*) &pthread_nums[t]))
        {
            fprintf(stderr, "Error: Could not initialise pthreads\n");
            exit(2);
        }
    }

    for(int t = 0; t < num_threads; t++)
        if(pthread_join(threads[t], NULL))
        {
            fprintf(stderr, "Error: Could not join pthreads\n");
            exit(2);
        }
    
    if(SortedList_length(list_head) != 0)
    {
        fprintf(stderr, "Error: List size not 0 at end!\n");
        exit(2);
    }
    
    //calculating final csv output:
    clock_gettime(CLOCK_REALTIME, &end_time);
    runtime = ((long long)end_time.tv_sec - (long long)start_time.tv_sec) * 1000000000LL + (end_time.tv_nsec - start_time.tv_nsec);
    num_operations = num_threads * num_iterations * 3;
    avg_runtime = runtime/(long long)num_operations;

    free(list_head);
    free(list_elements);
    free(actual_keys);
    free(random_keys);
    free(pthread_nums);
    free(threads);


    //printing out csv:
    fprintf(stdout, "list-");
    if(opt_yield & INSERT_YIELD)
        fprintf(stdout, "i");
    if(opt_yield & DELETE_YIELD)
        fprintf(stdout, "d");
    if(opt_yield & LOOKUP_YIELD)
        fprintf(stdout, "l");
    if((opt_yield & INSERT_YIELD) | (opt_yield & DELETE_YIELD) | (opt_yield & LOOKUP_YIELD))
        fprintf(stdout, "-");
    else
        fprintf(stdout, "none-");
    if(sync_flag == 1)
        fprintf(stdout, "%s,", sync_type);
    else
        fprintf(stdout, "none,");
    fprintf(stdout, "%d,%d,1,%ld,%lld,%lld\n", num_threads, num_iterations, num_operations, runtime, avg_runtime);
    exit(0);
}