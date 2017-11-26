#include "coursework.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>

/*
    RR Bounded (Round Robin with Bounding Buffer) Implementation of predefined process.
    Predefined constraints are preprocessor macros in 'coursework.h'
*/

// Using this as a helper function
void swap(struct process** left, struct process** right)
{
    struct process* temp = *left;
    *left = *right;
    *right = temp;
}

// Quick and dirty way to get size of linked list. size_t as opposed to unsigned int as values inputted into things like operator[] are not unsigned ints, and do have different limits.
// size_t declared in stdlib.h and stddef.h iirc.
size_t list_size(struct process* head)
{
    size_t size = 0;
    while(head != (void*)0)
    {
        size++;
        head = head->oNext;
    }
    return size;
}

/* pthread functionality requires that all functions ran on a separate thread must return void* and take a single void* parameter.
however, multiple parameters will be requires, such as a pointer to the head of the process list, a mutex lock etc.
to solve this, the following structs are used to contain all required data:
*/

struct creator_pack
{
    pthread_mutex_t* mutex_handle;
    // This is the shared data. Although this is just a copy of a pointer, the data being pointed to is the shared data.
    // Therefore whenever consumer runs or edits any process in the list in anyway, mutex lock must be invoked during such execution.
    struct process* head;
};

struct consumer_pack
{
    pthread_mutex_t* mutex_handle;
    // still is the shared data.
    struct process* head;
    // Want to access the totals values to edit them with any consumption of processes performed.
    unsigned int* total_response_time;
    unsigned int* total_turnaround_time;
};

// RR, add the process to the end of the list. edits the list so MUST be mutex locked.
void add_process(pthread_mutex_t* lock, struct process* head, struct process* a_process)
{
    pthread_mutex_lock(lock);
    if(head == (void*)0)
        return;
    while(head->oNext != (void*)0)
    {
        head = head->oNext;
    }
    head->oNext = a_process;
    pthread_mutex_unlock(lock);
}

// Must be ran on a creator package where the process head is just one element.
void* create_processes(void* creator_package)
{
    struct creator_pack* creator = (struct creator_pack*) creator_package;
    printf("Asserting list_size == 1...\n");
    assert(list_size(creator->head) == 1);
    size_t processes_created = 1;
    while(processes_created < NUMBER_OF_PROCESSES)
    {
        // this thread keeps trying to create new processes until the number of processes made in total is what we need.
        if(list_size(creator->head) <= BUFFER_SIZE)
        {
            // we have space to generate a new process, so do so.
            struct process* new_process = generateProcess();
            add_process(creator->mutex_handle, creator->head, new_process);
            printf("Adding process to end of the list.");
        }
        else
        {
            // we arent allowed to generate a new process. the mutex is not locked so we wait, i.e do nothing and sleep.
            printf("Buffer Size exceeded, waiting...\n");
            sleep(1);
        }
    }
    pthread_exit(NULL);
    // Kill the thread. We're done creating processes.
}

// Take double pointer to head remains true. edits the list so MUST be locked!
void remove_process(pthread_mutex_t* lock, struct process** head, struct process* to_remove)
{
    pthread_mutex_lock(lock);
    struct process* process_head = *head;
    if(process_head == (void*)0)
        return;
    if(process_head == to_remove)
    {
        struct process* tmp = process_head;
        head = &process_head->oNext;
        free(process_head);
    }
    while(process_head->oNext != (void*)0)
    {
        if(process_head->oNext == to_remove)
        {
            if(to_remove->oNext != (void*)0)
                process_head->oNext = to_remove->oNext;
            free(to_remove);
        }
        process_head = process_head->oNext;
    }
    pthread_mutex_unlock(lock);
}

void* consume_processes(void* consumer_package)
{
    struct consumer_pack* consumer = (struct consumer_pack*) consumer_package;
}

int is_finished(struct process* a_process)
{
    return a_process->iState == FINISHED;
}

int all_finished(struct process* process_head)
{
    while(process_head != (void*)0)
    {
        if(process_head->iState != FINISHED)
            return 0;
        process_head = process_head->oNext;
    }
    return 1;
}

int main()
{
    unsigned int total_turnaround_time = 0;
    unsigned int total_response_time = 0;
    // Give me a process. Linked List is currently sorted as contains one element.
    struct process* process_head = generateProcess();
    struct process* process_tail = process_head;
    unsigned int i;
    // make number of processes we've allocated equal to the macro
    for(i = 0; i < NUMBER_OF_PROCESSES; i++)
    {
        struct process* a_process = generateProcess();
        add_process(process_head, a_process);
        process_tail = a_process;
    }

    pthread_mutex_t lock;
    pthread_t creator_thread_handle;
    // Creator thread separate. Consumption thread unnecessary as that will be done in the main thread.
    // The reason I do not create another thread for consumption as the main thread will just wait for it anyway so might aswell use it.

    struct process* tmp = process_head;
    while(!all_finished(process_head))
    {
        struct timeval start, end;
        int previous_burst = tmp->iBurstTime;
        int already_running = 0;
        if(tmp->iState == RUNNING || tmp->iState == READY)
            already_running = 1;
        simulateRoundRobinProcess(tmp, &start, &end);
        unsigned int response_time = getDifferenceInMilliSeconds(tmp->oTimeCreated, start);
        printf("pid = %d, previous burst = %d, new burst = %d", tmp->iProcessId, previous_burst, tmp->iBurstTime);
        if(!already_running)
        {
            printf(", response time = %ld", response_time);
            total_response_time += response_time;
        }
        struct process* check = tmp;
        if(tmp->oNext == (void*)0)
            tmp = process_head;
        else
            tmp = tmp->oNext;
        if(is_finished(check))
        {
            unsigned int turnaround_time = getDifferenceInMilliSeconds(tmp->oTimeCreated, end);
            printf(", turnaround time = %ld", turnaround_time);
            total_turnaround_time += turnaround_time;
            remove_process(&process_head, check);
        }
        printf("\n");
    }
    printf("Done. Average Response Time = %ldms, Average Turnaround Time = %ldms\n", total_response_time / NUMBER_OF_PROCESSES, total_turnaround_time / NUMBER_OF_PROCESSES);
    return 0;
}