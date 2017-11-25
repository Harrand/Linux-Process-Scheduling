#include "coursework.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

// Using this as a helper function
void swap(struct process** left, struct process** right)
{
    struct process* temp = *left;
    *left = *right;
    *right = temp;
}

// Take address of head because the head may well change during adding of a new process, the head pointer passed into the function shall remain valid.
void add_process(pthread_mutex_t* lock, struct process** head, struct process* a_process)
{
    pthread_mutex_lock(lock);
    struct process* process_head = *head;
    if(a_process->iBurstTime < process_head->iBurstTime)
    {
        swap(head, &a_process);
        process_head->oNext = a_process;
        return;
    }
    struct process* iter = process_head;
    while(iter->iBurstTime < a_process->iBurstTime)
    {
        // (void*)0 is basically exactly what the macro NULL does. Using it like this as NULL confuses me when it's just referring to memloc 0, not necessarily a nullptr
        if(iter->oNext == (void*)0)
        {
            // at the end of the linked list, so this is the longest burst time so far, so make it the tail
            iter->oNext = a_process;
            break;
        }
        if(iter->oNext->iBurstTime > a_process->iBurstTime)
        {
            // needs to be inserted between iter and iter next
            struct process* tmp = iter->oNext;
            iter->oNext = a_process;
            a_process->oNext = tmp;
        }
        iter = iter->oNext;
    }
    pthread_mutex_unlock(lock);
}

void remove_process(pthread_mutex_t* lock, struct process** head, struct process* to_remove)
{
    //pthread_mutex_lock(lock);
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
    //pthread_mutex_unlock(lock);
}

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

struct process_creator
{
    pthread_mutex_t* lock;
    struct process* process_head;
};

// Take in a head. Must be the only element in the list.
void* fill_processes(void* process_creator_v)
{
    struct process_creator* creator = (struct process_creator*) process_creator_v;
    //pthread_mutex_lock(creator->lock);
    size_t size = 1;
    while(size < NUMBER_OF_PROCESSES)
    {
        if(list_size(creator->process_head) < BUFFER_SIZE)
        {
            // enough space to add a new process.
            add_process(creator->lock, &creator->process_head, generateProcess());
            size++;
            printf("added process (%d/%d)\n", size, NUMBER_OF_PROCESSES);
        }
        else
        {
            //pthread_mutex_unlock(creator->lock);
            printf("list size = %d, waiting...\n", list_size(creator->process_head));
            sleep(1);
            //pthread_mutex_lock(creator->lock);
            // we just have to wait until there's space.
        }
    }
    //pthread_mutex_unlock(creator->lock);
    pthread_exit(NULL);
}

struct process_consumer_singular
{
    pthread_mutex_t* lock;
    struct process* process_head;
    struct process* a_process;
    unsigned int* total_response_time;
    unsigned int* total_turnaround_time;
};

struct process_consumer_all
{
    pthread_mutex_t* lock;
    struct process* process_head;
    unsigned int* total_response_time;
    unsigned int* total_turnaround_time;
};

void* consume_process(void* a_process_consumer_v/*void* process_head_v, void* process_v, void* total_response_time_v, void* total_turnaround_time_v*/)
{
    struct process_consumer_singular* a_process_consumer = (struct process_consumer_singular*) a_process_consumer_v;
    struct timeval start, end;
    pthread_mutex_lock(a_process_consumer->lock);
    simulateSJFProcess(a_process_consumer->a_process, &start, &end);
    unsigned int response_time = getDifferenceInMilliSeconds(a_process_consumer->a_process->oTimeCreated, start);
    unsigned int turnaround_time = getDifferenceInMilliSeconds(a_process_consumer->a_process->oTimeCreated, end);
    printf("consumed process took %ld ms response time and %ld ms turnaround time\n", response_time, turnaround_time);
    *(a_process_consumer->total_response_time) += response_time;
    *(a_process_consumer->total_turnaround_time) += turnaround_time;
    printf("killing and freeing process..\n");
    remove_process(a_process_consumer->lock, &a_process_consumer->process_head, a_process_consumer->a_process);
    printf("process killed.\n");
    pthread_mutex_unlock(a_process_consumer->lock);
}

void* consume_processes(void* process_consumer_v/*void* process_head_v, void* total_response_time_v, void* total_turnaround_time_v*/)
{
    struct process_consumer_all* consumer = (struct process_consumer_all*) process_consumer_v;
    while(list_size(consumer->process_head) > 0)
    {
        struct process_consumer_singular single;
        single.lock = consumer->lock;
        single.process_head = consumer->process_head;
        single.a_process = consumer->process_head->oNext;
        single.total_response_time = consumer->total_response_time;
        single.total_turnaround_time = consumer->total_turnaround_time;
        consume_process((void*)&single);
        printf("consumed process. (%d/%d)", list_size(consumer->process_head), NUMBER_OF_PROCESSES);
    }
    pthread_exit(NULL);
}

int main()
{
    pthread_mutex_t lock;
    unsigned int total_turnaround_time = 0;
    unsigned int total_response_time = 0;
    // Give me a process. Linked List is currently sorted as contains one element.
    struct process* process_head = generateProcess();
    unsigned int i;
    // make number of processes we've allocated equal to the macro
    /*
    for(i = 0; i <= NUMBER_OF_PROCESSES + 1; i++)
    {
        struct process* a_process = generateProcess();
        add_process(&process_head, a_process);
    }
    */
    // Initialise mutex so we can use it later to lock the process during destruction of a member.
    pthread_mutex_init(&lock, NULL);
    pthread_t creator_thread_handle, consumer_thread_handle;
    struct process_creator creator;
    creator.lock = &lock;
    creator.process_head = process_head;
    pthread_create(&creator_thread_handle, NULL, fill_processes, (void*)&creator);
    //pthread_join(creator_thread_handle, NULL);
    struct process_consumer_all consumer;
    consumer.lock = &lock;
    consumer.process_head = process_head;
    consumer.total_response_time = &total_response_time;
    consumer.total_turnaround_time = &total_turnaround_time;
    pthread_create(&consumer_thread_handle, NULL, consume_processes, (void*)&consumer);
    /*
    struct process* tmp;
    while (process_head != (void*)0)
    {
         tmp = process_head;
         process_head = process_head->oNext;
         struct timeval start, end;
         simulateSJFProcess(tmp, &start, &end);
         unsigned int response_time = getDifferenceInMilliSeconds(tmp->oTimeCreated, start);
         unsigned int turnaround_time = getDifferenceInMilliSeconds(tmp->oTimeCreated, end);
         printf("process took %ld ms response time and %ld ms turnaround time\n", response_time, turnaround_time);
         total_response_time += response_time;
         total_turnaround_time += turnaround_time;
         free(tmp);
    }
    */
    pthread_join(creator_thread_handle, NULL);
    pthread_join(consumer_thread_handle, NULL);
    pthread_mutex_destroy(&lock);
    printf("Done. Average Response Time = %ldms, Average Turnaround Time = %ldms\n", total_response_time / NUMBER_OF_PROCESSES, total_turnaround_time / NUMBER_OF_PROCESSES);
    return 0;
}