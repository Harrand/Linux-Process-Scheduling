#include "coursework.h"
#include <stdio.h>
#include <stdlib.h>

// Using this as a helper function
void swap(struct process** left, struct process** right)
{
    struct process* temp = *left;
    *left = *right;
    *right = temp;
}

// Take address of head because the head may well change during adding of a new process, the head pointer passed into the function shall remain valid.
void add_process(struct process** head, struct process* a_process)
{
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
}

int main()
{
    unsigned int total_turnaround_time = 0;
    unsigned int total_response_time = 0;
    // Give me a process. Linked List is currently sorted as contains one element.
    struct process* process_head = generateProcess();
    unsigned int i;
    // make number of processes we've allocated equal to the macro
    for(i = 0; i < NUMBER_OF_PROCESSES; i++)
    {
        struct process* a_process = generateProcess();
        add_process(&process_head, a_process);
    }

    struct process* tmp;
    // iterate through the list, running each element just before freeing it. follows RAII, a cpp practice and is efficient.
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
    printf("Done. Average Response Time = %ldms, Average Turnaround Time = %ldms\n", total_response_time / NUMBER_OF_PROCESSES, total_turnaround_time / NUMBER_OF_PROCESSES);
    return 0;
}