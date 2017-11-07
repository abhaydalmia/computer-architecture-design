/*
 * student.c
 * Multithreaded OS Simulation for CS 2200, Project 5
 * Fall 2016
 *
 * This file contains the CPU scheduler for the simulation.
 * Name: Abhay Dalmia
 * GTID: 903052440
 */

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include "os-sim.h"

void addToBack(pcb_t *process);
void removeFromFront();
void addPriority(pcb_t *process);
extern void idle(unsigned int cpu_id);

/*
 * current[] is an array of pointers to the currently running processes.
 * There is one array element corresponding to each CPU in the simulation.
 *
 * current[] should be updated by schedule() each time a process is scheduled
 * on a CPU.  Since the current[] array is accessed by multiple threads, you
 * will need to use a mutex to protect it.  current_mutex has been provided
 * for your use.
 */
static pcb_t **current;
static pthread_mutex_t current_mutex;
static unsigned int algorithm;
static int cpu_count_global;
static int algorithm_parameter;

static pcb_t *head;
static int readyQueueSize;
static pthread_mutex_t queue_mutex;
static pthread_cond_t  queueIsNotZero;
/*
 * schedule() is your CPU scheduler.  It should perform the following tasks:
 *
 *   1. Select and remove a runnable process from your ready queue which 
 *	you will have to implement with a linked list or something of the sort.
 *
 *   2. Set the process state to RUNNING
 *
 *   3. Call context_switch(), to tell the simulator which process to execute
 *      next on the CPU.  If no process is runnable, call context_switch()
 *      with a pointer to NULL to select the idle process.
 *	The current array (see above) is how you access the currently running
 *	process indexed by the cpu id. See above for full description.
 *	context_switch() is prototyped in os-sim.h. Look there for more information 
 *	about it and its parameters.
 */
static void schedule(unsigned int cpu_id)
{
	if (algorithm == 0 || algorithm == 2) {
		pthread_mutex_lock(&queue_mutex);
	    if (readyQueueSize == 0) {
	    	pthread_mutex_unlock(&queue_mutex);
	        context_switch(cpu_id, NULL, -1);
	        return;
	    }
	    pthread_mutex_unlock(&queue_mutex);

	    pcb_t *newProcess = head;
	    removeFromFront();

	    newProcess->state = PROCESS_RUNNING;

	    pthread_mutex_lock(&current_mutex);
	    current[cpu_id] = newProcess;
	    pthread_mutex_unlock(&current_mutex);

	    context_switch(cpu_id, newProcess, -1);
	} else if (algorithm == 1) {

		pthread_mutex_lock(&queue_mutex);
	    if (readyQueueSize == 0) {
	    	pthread_mutex_unlock(&queue_mutex);
	        context_switch(cpu_id, NULL, algorithm_parameter);
	        return;
	    }
	    pthread_mutex_unlock(&queue_mutex);

	    pcb_t *newProcess = head;
	    removeFromFront();

	    newProcess->state = PROCESS_RUNNING;

	    pthread_mutex_lock(&current_mutex);
	    current[cpu_id] = newProcess;
	    pthread_mutex_unlock(&current_mutex);

	    context_switch(cpu_id, newProcess, algorithm_parameter);

	}


}


/*
 * idle() is your idle process.  It is called by the simulator when the idle
 * process is scheduled.
 *
 * This function should block until a process is added to your ready queue.
 * It should then call schedule() to select the process to run on the CPU.
 */
extern void idle(unsigned int cpu_id)
{
	pthread_mutex_lock(&queue_mutex);
    while (readyQueueSize == 0) {
        pthread_cond_wait(&queueIsNotZero, &queue_mutex);
    }
    pthread_mutex_unlock(&queue_mutex);

    schedule(cpu_id);


    /*
     * REMOVE THE LINE BELOW AFTER IMPLEMENTING IDLE()
     *
     * idle() must block when the ready queue is empty, or else the CPU threads
     * will spin in a loop.  Until a ready queue is implemented, we'll put the
     * thread to sleep to keep it from consuming 100% of the CPU time.  Once
     * you implement a proper idle() function using a condition variable,
     * remove the call to mt_safe_usleep() below.
     */
    //mt_safe_usleep(1000000);
}


/*
 * preempt() is the handler called by the simulator when a process is
 * preempted due to its timeslice expiring.
 *
 * This function should place the currently running process back in the
 * ready queue, and call schedule() to select a new runnable process.
 */
extern void preempt(unsigned int cpu_id)
{
		current[cpu_id]->state = PROCESS_READY;
        if (algorithm == 1) {
		  addToBack(current[cpu_id]);
        }
        if (algorithm == 2) {
            addPriority(current[cpu_id]);
        }
		schedule(cpu_id);
}


/*
 * yield() is the handler called by the simulator when a process yields the
 * CPU to perform an I/O request.
 *
 * It should mark the process as WAITING, then call schedule() to select
 * a new process for the CPU.
 */
extern void yield(unsigned int cpu_id)
{
    pthread_mutex_lock(&current_mutex);
    current[cpu_id]->state = PROCESS_WAITING;
    pthread_mutex_unlock(&current_mutex);
    schedule(cpu_id);
}


/*
 * terminate() is the handler called by the simulator when a process completes.
 * It should mark the process as terminated, then call schedule() to select
 * a new process for the CPU.
 */
extern void terminate(unsigned int cpu_id)
{
    pthread_mutex_lock(&current_mutex);
    current[cpu_id]->state = PROCESS_TERMINATED;
    pthread_mutex_unlock(&current_mutex);

    schedule(cpu_id);

}


/*
 * wake_up() is the handler called by the simulator when a process's I/O
 * request completes.  It should perform the following tasks:
 *
 *   1. Mark the process as READY, and insert it into the ready queue.
 *
 *   2. If the scheduling algorithm is static priority, wake_up() may need
 *      to preempt the CPU with the lowest priority process to allow it to
 *      execute the process which just woke up.  However, if any CPU is
 *      currently running idle, or all of the CPUs are running processes
 *      with a higher priority than the one which just woke up, wake_up()
 *      should not preempt any CPUs.
 *	To preempt a process, use force_preempt(). Look in os-sim.h for 
 * 	its prototype and the parameters it takes in.
 */
extern void wake_up(pcb_t *process)
{
    if (algorithm == 0 || algorithm == 1) {
	   process->state = PROCESS_READY;
	   addToBack(process);
    }

    if (algorithm == 2) {
        int idle = 0;
        process->state = PROCESS_READY;
        addPriority(process);
        pthread_mutex_lock(&current_mutex);
        for (int i = 0; i < cpu_count_global; i++) {
            if (current[i] == NULL || current[i]->state != PROCESS_RUNNING) {
				idle = 1;
            }
        }
        if (idle == 0) {
        	unsigned int min = 11;
        	int indexMin = -1;
        	for (int i = 0; i < cpu_count_global; i++) {
	            if (current[i]->static_priority < min) {
					min = current[i]->static_priority;
					indexMin = i;
	            }
	        }
            pthread_mutex_unlock(&current_mutex);

	        if (min < process->static_priority) {
	        	force_preempt(indexMin);
	        }
        } else {
            pthread_mutex_unlock(&current_mutex);
        }
    }
}


/*
 * main() simply parses command line arguments, then calls start_simulator().
 * You will need to modify it to support the -r and -p command-line parameters.
 */
int main(int argc, char *argv[])
{
    int cpu_count;

    /* Parse command-line arguments */
    if (argc > 4 || argc < 2)
    {
        fprintf(stderr, "CS 2200 Project 5 Fall 2016 -- Multithreaded OS Simulator\n"
            "Usage: ./os-sim <# CPUs> [ -r <time slice> | -p ]\n"
            "    Default : FIFO Scheduler\n"
            "         -r : Round-Robin Scheduler\n"
            "         -p : Static Priority Scheduler\n\n");
        return -1;
    }

    cpu_count = atoi(argv[1]);
    cpu_count_global = cpu_count;

    /* FIX ME - Add support for -r and -p parameters*/
    if (argc == 2) {
    	algorithm = 0;
    } else {
    	int opt = getopt(argc, argv, "r:p");
    	switch (opt) {
    		case 'r':
    			algorithm = 1;
                algorithm_parameter = atoi(argv[3]);

    			break;
    		case 'p':
    			algorithm = 2;
    			break;
    	}

    }

    


    /* Allocate the current[] array and its mutex */
    /*********** TODO *************/
    head = NULL;
    readyQueueSize = 0;
    pthread_mutex_init(&queue_mutex, NULL);
    pthread_cond_init(&queueIsNotZero, NULL);


    current = malloc(sizeof(pcb_t*) * cpu_count);
    assert(current != NULL);
    pthread_mutex_init(&current_mutex, NULL);

    /* Start the simulator in the library */
    start_simulator(cpu_count);

    return 0;
}

void removeFromFront() {
    pthread_mutex_lock(&queue_mutex);
    if (readyQueueSize == 0 || head == NULL) {
    } else {
	    readyQueueSize = readyQueueSize - 1;
	    head = head->next;
	}
    pthread_mutex_unlock(&queue_mutex);

}

void addToBack(pcb_t *process) {
    pthread_mutex_lock(&queue_mutex);
    if (readyQueueSize == 0) {
        head = process;
        readyQueueSize = 1;
    } else {
	    pcb_t *lastNode = head;
	    for (int i = 0; i < readyQueueSize - 1; i++) {
	        lastNode = lastNode->next;
	    }
	    lastNode->next = process;
	    readyQueueSize = readyQueueSize + 1;
	}

    pthread_cond_signal(&queueIsNotZero);
    pthread_mutex_unlock(&queue_mutex);

}



void addPriority(pcb_t *process) {

	pthread_mutex_lock(&queue_mutex);
	if (readyQueueSize == 0) {
        head = process;
        readyQueueSize = 1;
    } else {
        
        if (head->static_priority < process->static_priority) {
            process->next = head;
            head = process;
        } else {
            pcb_t *lastNode = head;
            
            for (int i = 0; i < readyQueueSize - 1 && lastNode->static_priority < process->static_priority ; i++) {
                lastNode = lastNode->next;
            }
            pcb_t *nextNode = lastNode->next;
            process->next = nextNode;
            lastNode->next = process;
        }
        readyQueueSize = readyQueueSize + 1;
        pcb_t* printNode = head;
                printf("\n");

        for(int i = 0; i < readyQueueSize - 1; i++) {
            printf("%d ", printNode->static_priority);
            printNode = printNode->next;
        }
    }
    pthread_cond_signal(&queueIsNotZero);
    pthread_mutex_unlock(&queue_mutex);
}
