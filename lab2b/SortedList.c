#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <stdlib.h>
#include "SortedList.h"

void SortedList_insert(SortedList_t *list, SortedListElement_t *element)
{
    if(opt_yield & INSERT_YIELD)
        sched_yield();

    if(list->next == list && list->prev == list)
    {
        list->next = element;
        list->prev = element;
        element->prev = list;
        element->next = list;
        return;
    }

    //iterate through the list till you find first member that has greater key than element or we reach back to head
    SortedListElement_t* curr = list->next;
    while(curr->key != NULL && strcmp(curr->key, element->key) < 0)
    {
        if(curr->next == curr)
        {
            fprintf(stderr, "Error: List Corrupted!\n");
            break;
        }
        curr = curr->next;
    }
    
    curr->prev->next = element;
    element->prev = curr->prev;
    curr->prev = element;
    element->next = curr;
}

int SortedList_delete( SortedListElement_t *element)
{
    if(opt_yield & DELETE_YIELD)
        sched_yield();

    if(element == NULL)
        return 1;
    
    element->prev->next = element->next;
    element->next->prev = element->prev;
    return 0;
}

SortedListElement_t *SortedList_lookup(SortedList_t *list, const char *key)
{
    if(opt_yield & LOOKUP_YIELD)
        sched_yield();
    if (key == NULL)
        return NULL;

    SortedListElement_t* curr = list->next;
    while(curr != NULL && curr != list)
    {
        if(curr->next == curr)
        {
            fprintf(stderr, "Error: List Corrupted!\n");
            break;
        }

        if (curr->key != NULL && strcmp(curr->key, key) == 0)
            return curr;
        else
            curr = curr->next;
    }

    return NULL;
}

int SortedList_length(SortedList_t *list)
{
    int counter = 0;
    SortedListElement_t* curr = list->next;
    while(curr->key != NULL)
    {
        if(curr->next == curr)
        {
            fprintf(stderr, "Error: List Corrupted!\n");
            break;
        }
        counter++;
        curr = curr->next;
    }
    return counter;
}