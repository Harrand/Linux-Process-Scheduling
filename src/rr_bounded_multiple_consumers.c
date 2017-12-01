#include "posix_utility.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>

/*
    RR Bounded & MC (Shortest-Job-First with Bounding Buffer and Multiple Consumers) Implementation of predefined process.
    Predefined constraints are preprocessor macros in 'posix_utility.h'
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

int is_locked(pthread_mutex_t* mutex)
{
    int ret = 1;
    if(pthread_mutex_trylock(mutex) == 0)
    {
       // successfully locked, i.e the mutex was not locked beforehand.
       ret = 0;
       pthread_mutex_unlock(mutex);
    }
    return ret;
}

void print_list(struct process* head)
{
    struct process* iter = head;
    int i;
    for(i = 0; i < list_size(head); i++)
    {
        printf("[%d] pid = %d\n", i, iter->iProcessId);
        iter = iter->oNext;
    }
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
    struct process** head;
    unsigned int* creating_finished;
};

struct consumer_pack
{
    pthread_mutex_t* mutex_handle;
    unsigned int consumer_id;
    // still is the shared data.
    // head is a double ptr because the head position will change alot.
    struct process** head;
    struct process* tail;
    unsigned int* creating_finished;
    // Want to access the totals values to edit them with any consumption of processes performed.
    unsigned int* total_response_time;
    unsigned int* total_turnaround_time;
};

// RR, add the process to the end of the list. edits the list so MUST be mutex locked.
void add_process(pthread_mutex_t* lock, struct process** head, struct process* a_process)
{
    pthread_mutex_lock(lock);
    struct process* head_cpy = *head;
    if(head_cpy == (void*)0)
        *head = a_process;
    while(head_cpy->oNext != (void*)0)
    {
        head_cpy = head_cpy->oNext;
    }
    head_cpy->oNext = a_process;
    pthread_mutex_unlock(lock);
}

// Must be ran on a creator package where the process head is just one element.
void* create_processes(void* creator_package)
{
    struct creator_pack* creator = (struct creator_pack*) creator_package;
    printf("Asserting list_size == 1...\n");
    assert(list_size(*creator->head) == 1);
    size_t processes_created = 1;
    while(processes_created < NUMBER_OF_PROCESSES)
    {
        // this thread keeps trying to create new processes until the number of processes made in total is what we need.
        if(list_size(*creator->head) <= BUFFER_SIZE)
        {
            // we have space to generate a new process, so do so.
            struct process* new_process = generateProcess();
            add_process(creator->mutex_handle, creator->head, new_process);
            processes_created++;
        }
    }
    pthread_mutex_lock(creator->mutex_handle);
    *(creator->creating_finished) = 1;
    pthread_mutex_unlock(creator->mutex_handle);
    // Make the bool true so the other thread can safely read. Shouldn't need to mutex this.
    pthread_exit(NULL);
    // Kill the thread. We're done creating processes.
}

// Take double pointer to head remains true. edits the list so MUST be locked!
void remove_process(pthread_mutex_t* lock, struct process** head, struct process* to_remove)
{
    pthread_mutex_lock(lock);
    struct process* process_head = *head;
    if(process_head == (void*)0)
    {
        pthread_mutex_unlock(lock);
        return;
    }
    if(process_head == to_remove)
    {
        struct process* tmp = process_head;
        *head = process_head->oNext;
        free(process_head);
        //print_list(*head);
        pthread_mutex_unlock(lock);
        return;
    }
    struct process* previous = process_head;
    while(process_head != (void*)0)
    {
        if(process_head == to_remove)
        {
            if(to_remove != (void*)0)
                process_head = to_remove;
            previous->oNext = process_head->oNext;
            free(to_remove);
            //print_list(*head);
            pthread_mutex_unlock(lock);
            return;
        }
        previous = process_head;
        process_head = process_head->oNext;
    }
    pthread_mutex_unlock(lock);
}

void* consume_processes(void* consumer_package)
{
    struct consumer_pack* consumer = (struct consumer_pack*) consumer_package;
    const unsigned int cid = consumer->consumer_id;
    unsigned int i;
    // because multiple consumers, we dont want to ever actually lock the mutex, just check if its already locked by the creator and sleep if it is.
    while(*(consumer->creating_finished) == 0 || list_size(*consumer->head) > 0) // thread does not die until we're no longer creating more and the list is completely empty.
    {
        // if were not done and we have one more, do not kill the last one
        // but if we are done and have one more, than we can kill the last one
        if(*(consumer->creating_finished) == 0 && list_size(*consumer->head) <= 1)
        {
            //printf("list size is 1 or less, waiting...\n");
            //printf("creation finished = %d, list_size = %d\n", *(consumer->creating_finished), list_size(*consumer->head));
            //sleep(1);
            continue;
        }
        struct process* begin = *consumer->head;
        int is_ready = 1;
        for(i = 0; i < cid; i++)
        {
            if(list_size(*consumer->head) == 0 || begin->oNext == NULL)
                is_ready = 0;
            else
                begin = begin->oNext;
        }
        // move begin to cid position in the list. so if cid = 1, cid is the 1st (not 0th) element of the linked list.
        if(is_ready == 0)
        {
            //printf("cid %d not ready to process... list size = %d\n", cid, list_size(*consumer->head));
            //sleep(1);
            continue;
        }
        if(is_locked(consumer->mutex_handle))
        {
            // wait if creation is happening right now or list size is too small.
            // currently creating more. dont do anything.
            continue;
        }
        // no creation is taking place by now, the list size is fair and our element in the list is valid.
        //pthread_mutex_lock(consumer->mutex_handle);
        //printf("cid = %d, head pid = %d, pid to kill = %d\n", cid, (*consumer->head)->iProcessId, begin->iProcessId);
        struct timeval start, end;
        int previous_burst = begin->iBurstTime;
        int already_running = 0;
        if(begin->iState == RUNNING || begin->iState == READY)
            already_running = 1;
        simulateRoundRobinProcess(begin, &start, &end);
        unsigned int response_time = getDifferenceInMilliSeconds(begin->oTimeCreated, start);
        printf("pid = %d, previous burst = %d, new burst = %d", begin->iProcessId, previous_burst, begin->iBurstTime);
        if(!already_running)
        {
            printf(", response time = %ld", response_time);
            *(consumer->total_response_time) += response_time;
        }
        // now delete it.
        if(is_finished(begin))
        {
            unsigned int turnaround_time = getDifferenceInMilliSeconds(begin->oTimeCreated, end);
            printf(", turnaround time = %ld", turnaround_time);
            //printf("\nprocess being killed. process list size = %d\n", list_size(*consumer->head));
            *(consumer->total_turnaround_time) += turnaround_time;
            remove_process(consumer->mutex_handle, consumer->head, begin);
        }
        printf("\n");
        //printf("finished removing process.\n");
        //printf("list size = %d, done = %d\n", list_size(*consumer->head), *(consumer->creating_finished));
        //pthread_mutex_unlock(consumer->mutex_handle);
    }
    pthread_exit(NULL);
    // Kill the thread.
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
    /*
    for(i = 0; i < NUMBER_OF_PROCESSES; i++)
    {
        struct process* a_process = generateProcess();
        add_process(process_head, a_process);
        process_tail = a_process;
    }
    */
    unsigned int create_done = 0;
    pthread_mutex_t lock;
    pthread_t creator_thread_handle, consumer_thread_handle[NUMBER_OF_CONSUMERS];
    struct creator_pack creator;
    creator.mutex_handle = &lock;
    creator.head = &process_head;
    creator.creating_finished = &create_done;
    pthread_create(&creator_thread_handle, NULL, create_processes, &creator);
    struct consumer_pack consumer[NUMBER_OF_CONSUMERS];
    for(i = 0; i < NUMBER_OF_CONSUMERS; i++)
    {
        consumer[i].mutex_handle = &lock;
        consumer[i].consumer_id = i;
        consumer[i].head = &process_head;
        consumer[i].tail = process_tail;
        consumer[i].creating_finished = &create_done;
        consumer[i].total_response_time = &total_response_time;
        consumer[i].total_turnaround_time = &total_turnaround_time;
        pthread_create(&consumer_thread_handle[i], NULL, consume_processes, &consumer[i]);
    }
    // Creator thread separate. Consumption thread unnecessary as that will be done in the main thread.
    // The reason I do not create another thread for consumption as the main thread will just wait for it anyway so might aswell use it.

    pthread_join(creator_thread_handle, NULL);
    //printf("creator thread joined.\n");
    for(i = 0; i < NUMBER_OF_CONSUMERS; i++)
    {
        pthread_join(consumer_thread_handle[i], NULL);
        //printf("consumer thread joined.\n");
    }
    printf("Done. Average Response Time = %ldms, Average Turnaround Time = %ldms\n", total_response_time / NUMBER_OF_PROCESSES, total_turnaround_time / NUMBER_OF_PROCESSES);
    return 0;
}