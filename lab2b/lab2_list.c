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
#define LISTS 'e'

int num_threads = 1;
int num_iterations = 1;
int num_lists = 1;
SortedList_t* list_heads;
SortedListElement_t* list_elements;
const char ** actual_keys;
int sync_flag = 0;
char* sync_type = NULL;
pthread_mutex_t* mutex_locks;
int* spin_locks;
long long lock_time = 0;

int num_elementstocreate;
int* random_keys;
int random_keys_maxindex;
int opt_yield = 0;

void signalHandler()
{
    fprintf(stderr, "Error: Segmentation Fault\n");
    exit(2);
}

int simpleHashAndMod(const char* key)
{
    int hash = 0;
    for (size_t i = 0; i < strlen(key); i++)
    {
        hash += (int)key[i];
    }
    return (hash % num_lists);
}

char* generateKey()
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

void activateSync(int list_index)
{
    struct timespec lock_start;
    struct timespec lock_end;

    clock_gettime(CLOCK_REALTIME, &lock_start);

    if(sync_flag == 1 && *sync_type == 'm')
        pthread_mutex_lock(mutex_locks + list_index);
    if(sync_flag == 1 && *sync_type == 's')
        while(__sync_lock_test_and_set(spin_locks + list_index, 1));

    clock_gettime(CLOCK_REALTIME, &lock_end);
    lock_time += ((long long)lock_end.tv_sec - (long long)lock_start.tv_sec) * 1000000000LL + (lock_end.tv_nsec - lock_start.tv_nsec);
}

void deactivateSync(int list_index)
{
    if(sync_flag == 1 && *sync_type == 'm')
        pthread_mutex_unlock(mutex_locks + list_index);
    if(sync_flag == 1 && *sync_type == 's')
        __sync_lock_release(spin_locks + list_index);
}

void* listOperations(void* pthread_nums)
{
    int threadID = *((int*)pthread_nums);
    int list_index; 

    //insert elements into list:
    for(int i = threadID; i < num_elementstocreate; i += num_threads)
    {
        list_index = simpleHashAndMod((list_elements + i)->key);
        activateSync(list_index);
        SortedList_insert(list_heads + list_index, list_elements + i);
        deactivateSync(list_index);
    }

    //check list length:
    int size = 0;
    for (int i = 0; i < num_lists; i++)
    {
        list_index = i;
        activateSync(list_index);
        size += SortedList_length(list_heads + list_index);
        deactivateSync(list_index);
    }

    if (size < num_elementstocreate/num_threads)
    {
        fprintf(stderr, "Error: List Corrupted, list length incorrect!\n");
        exit(2);
    }
    
    for(int i = threadID; i < num_elementstocreate; i += num_threads)
    {
        list_index = simpleHashAndMod(actual_keys[i]);
        activateSync(list_index);
        if(SortedList_delete(SortedList_lookup(list_heads + list_index, actual_keys[i])) == 1)
        {
            fprintf(stderr, "Error: List Corrupted, key has been deleted before!\n");
            exit(2);
        }
        deactivateSync(list_index);
    }
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
            {"lists", required_argument, NULL, LISTS},
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

            case LISTS:
                num_lists = atoi(optarg);
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

    if(num_lists < 0)
    {
        fprintf(stderr, "Error: Incorrect argument provided to option --lists\n");
        exit(1);
    }

    //sets up signal handler for segmentation faults:
    signal(SIGSEGV, signalHandler);

    //allocates memory for locks:
    mutex_locks = (pthread_mutex_t*)malloc(num_lists*sizeof(pthread_mutex_t));
    spin_locks = (int*)malloc(num_lists*sizeof(int));

    //initialises locks:
    for(int i = 0; i < num_lists; i++)
    {
        spin_locks[i] = 0;
        if(sync_flag == 1 && *sync_type == 'm')
            if(pthread_mutex_init((mutex_locks + i), NULL))
            {
                fprintf(stderr, "Error: Could not initialise mutex lock\n");
                exit(2);
            }
    }

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

    //initialises empty list(s):
    list_heads = (SortedList_t*) malloc (num_lists*sizeof(SortedList_t));
    for(int i = 0; i < num_lists; i++)
    {
        (list_heads + i)->next = (list_heads + i);
        (list_heads + i)->prev = (list_heads + i);
        (list_heads + i)->key = NULL;
    }

    //define variables used to output csv:
    struct timespec start_time;
    struct timespec end_time;
    long long runtime;
    long num_operations;
    long long avg_runtime;

    clock_gettime(CLOCK_REALTIME, &start_time);

    //create pthreads:
    int* pthread_nums = (int*) malloc (num_threads*sizeof(int));
    for(int t = 0; t < num_threads; t++)
        pthread_nums[t] = t;

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
    
    for(int i = 0; i < num_lists; i++)
        if(SortedList_length(list_heads + i) != 0)
        {
            fprintf(stderr, "Error: List size not 0 at end!\n");
            exit(2);
        }
    
    //calculating final csv output:
    clock_gettime(CLOCK_REALTIME, &end_time);
    runtime = ((long long)end_time.tv_sec - (long long)start_time.tv_sec) * 1000000000LL + (end_time.tv_nsec - start_time.tv_nsec);
    num_operations = num_threads * num_iterations * 3;
    avg_runtime = runtime/(long long)num_operations;

    free(list_heads);
    free(list_elements);
    for(int i = 0; i < num_elementstocreate; i++)
        free((void*)actual_keys[i]);
    free(actual_keys);
    free(random_keys);
    free(pthread_nums);
    free(threads);
    free(mutex_locks);
    free(spin_locks);

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
    fprintf(stdout, "%d,%d,%d,%ld,%lld,%lld, %lld\n", num_threads, num_iterations, num_lists, num_operations, runtime, avg_runtime, (lock_time/num_operations));
    exit(0);
}