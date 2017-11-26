#include "coursework.h"
#include <stdio.h>
#include <stdlib.h>


/*
    RR (Round Robin) Implementation of predefined process.
    Predefined constraints are preprocessor macros in 'coursework.h'
*/

// Using this as a helper function
void swap(struct process** left, struct process** right)
{
    struct process* temp = *left;
    *left = *right;
    *right = temp;
}

// RR, add the process to the end of the list.
void add_process(struct process* head, struct process* a_process)
{
    if(head == (void*)0)
        return;
    while(head->oNext != (void*)0)
    {
        head = head->oNext;
    }
    head->oNext = a_process;
}

// Take double pointer to head remains true.
void remove_process(struct process** head, struct process* to_remove)
{
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