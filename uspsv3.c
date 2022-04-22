#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <signal.h>
#include "p1fxns.h"
#include "ADTs/queue.h"


#define USESTR "usage: [WORKLOAD_FILE] ... \n"
#define UNUSED __attribute__((unused))

// GLOBAL VARIABLES
int signalled = 0;
// Need to use queue or array of pids (as circular buffer using %)
// to keep track of active processes
//pid_t paused_processes;
//pid_t continued_processes;
int active_processes;
int time_to_switch = 0;

// QUEUE FOR KEEPING TRACK OF PROCESSES
void freeValue(){
	// free() here? or maybe just do nothing...
	return;
}

const Queue *myQueue = NULL;

// STRUCTURE FOR KEEPING TRACK OF PROCESSES
struct Process{
	pid_t myPid;
	int started;
	int running;
	int done;
};
struct Process *lastProcess;

// SIGNAL HANDLERS
void signal_handler(UNUSED int sig){
	//p1putstr(1, "MADE IT! inside signal handler\n");
	//execvp(args[0], args);
	signalled++;
}

/* SIGCHLD Handler from SYSC 1.4.6 */
void child_signaled(UNUSED int sig){ // this is called every time a child makes a signal, which occurs
	// every time a major event occurs in the child proccess
	pid_t pid;
	int status;

	p1putstr(1, "About to enter while loop in child_signalled\n");
	while((pid = waitpid(-1, &status, WNOHANG)) > 0){
		if(WIFEXITED(status)){ // I think we just use WIFEXITED here?
			// might also want to do if checks for all the different signals to determine
			// how we manage our queues/array
			// E.G.: WIFCONTINUED, WIFSTOPPED, etc...
			printf("setting lastProcess->running = 0 within child_signaled\n");
			lastProcess->running = 0; // prevents lastProcess from being enqueued again
			lastProcess->done = 1; // THIS CHANGE ISN'T DOING ANYTHING!
		} // else do nothing? note child_signaled is called for every tim SIGCHLD is sent,
		// which for all I know could be every time the child is continued, stopped, etc...
		// or just when it finishes...
	}
}

void timer_signaled(UNUSED int sig){ // this should be called upon every SIGALARM from settimer
	p1putstr(1, "About to set time_to_switch = 1 in timer signaled()\n");
	time_to_switch = 1;
}

int main( UNUSED int argc, char *argv[]){
	// start by getting processing argv to find workload file
	// if no workload file is given, process standard input
	const char *fileName;
	int fd;
	char buf[BUFSIZ];
	char command[BUFSIZ];
	char *args[BUFSIZ];
	int status;
	int waits = 0;
	pid_t pid;
	pid_t pids[100];
	sigset_t signalset;
	int sig;

	// Initialize Queue
	if ((myQueue = Queue_create(NULL)) == NULL){
		p1perror(1, "ERROR - failed to create stack ADT Queue\n");
		return EXIT_FAILURE;
	}

	// initialize and add SIGUSR1 to signalset
	status = sigemptyset(&signalset);
	if(sigaddset(&signalset, SIGUSR1) == -1){
		p1putstr(1, "ERROR - failed to add signal to signal set\n");
	} 

	if(argv[1] == NULL){
		p1putstr(1, "ERROR: You must provide a file name \n");
		return EXIT_FAILURE;
	} else {
		fileName = p1strdup(argv[1]);
	}

	//open file and save words to array
	fd = open(fileName, O_RDONLY);
	if(fd == -1){
		p1putstr(1, "ERROR: Could not open file fd in main\n");
		return EXIT_FAILURE;
	}
	
	// prepare args[] by loading it with commands and
	// their respective arguments AND FORK!
	while(p1getline(fd, buf, sizeof buf) != 0){		
		++waits;
		int i = 0;
		buf[p1strlen(buf)-1] = '\0';
		int index = 0;
		while((index = p1getword(buf, index, command)) != -1){
			args[i] = p1strdup(command); 
			i++;
		}
		args[i] = NULL; // sentinel so we know when out of args

		// fork, execute, and join
		pid = fork();
		if(pid == 0){
			//p1putstr("args[0]: %s\n", args[0]);	
			//p1putstr(1, "About to register signal from child\n");
			signal(SIGUSR1, signal_handler);
			//sigsuspend(&signalset);
			sigwait(&signalset, &sig);
			//p1putstr(1, "Done waiting, about to call execvp\n");
			execvp(args[0], args);
		} else if(pid > 0){
			pids[waits-1] = pid;
		} else{
			p1putstr(1, "FORK FAILED!\n");
		}
		// noticed memory leaks caused by strdup, so i free here
		for(int j = 0; j < i; j++){
			free(args[j]);
		}
	}

	// LOAD QUEUE
	struct Process childProcess[waits];
	for(int j = 0; j < waits; j++){
		//p1putstr(1, "saving pid to pids in parent\n");
		childProcess[j].myPid = pids[j];
		childProcess[j].running = 0;
		childProcess[j].started = 0;
		childProcess[j].done = 0;
		//printf("About to enqueue chile process in fork() loop, pid: %d\n", childProcess.myPid);
		(void)myQueue->enqueue(myQueue, (void *)&childProcess[j]); //&childProcess
	}

	
	// NEW CHILD PROCESS CONTROL SEQUENCE WITH SETITIMER
	if(pid > 0){
		sleep(1); //wait for child processes to register their signals
		

		// set parent's SIGCHLD to child_signaled
		// so its called upon each signal from child to parent,
		// which I BELIEVE will be sent whenever a child:
		// 1. Completes (WIFEXITED) 2. Is paused (WIFSTOPPED) 3. Is continued (WIFCONTINUED)
		if(signal(SIGCHLD, child_signaled) == SIG_ERR){
			p1perror(1, "ERROR - unable to register SIGCHLD in parent\n");
			return EXIT_FAILURE;
		}

		//
		if(signal(SIGALRM, timer_signaled) == SIG_ERR){
			p1perror(1, "ERROR - unable to register SIGALRM in parent\n");
			return EXIT_FAILURE;
		}

		//initialize it_val with INTERVAL, eventually this should be given by
		// QUANTUM in either args or a environment variable
		struct itimerval it_val;
		it_val.it_value.tv_sec = 1; // not sure exactly how to set tv_sec and tv_usec
		it_val.it_value.tv_usec = 10000; // maybe they should match? IDK...
		it_val.it_interval = it_val.it_value;
		
		
		// use setitimer to send signal alarm every BLANK milliseconds
		if(setitimer(ITIMER_REAL, &it_val, NULL) == -1){
			p1perror(1, "ERROR - setitimer() failed.\n");
			return EXIT_FAILURE;
		} // now SIGALARM should be sent by setitimer every interval in &it_val
		
		// START CHILD MANAGEMENT LOOP 
		struct Process *place_holder_for_loop;
		struct Process get_started;
		get_started.started = 0;
		get_started.running = 0;
		get_started.done = 0;
		lastProcess = &get_started;

		// TESTING BELOW
		int queue_size = myQueue->size(myQueue);
		printf("QUEUE TEST, QUEUE SIZE: %d \n", queue_size);
		// iterate through and print contents of queue!
		const Iterator *iq = myQueue->itCreate(myQueue);
		while (iq->hasNext(iq)){
			iq->next(iq, (void**)&place_holder_for_loop);
			printf("ELEMENT PID: %d\n", place_holder_for_loop->myPid);
		}
			

		while(myQueue->front(myQueue, &place_holder_for_loop)){ //runs till queue is empty
		//	p1putstr(1, "IN CHILD CONTROL WHILE LOOP\n");
			if(time_to_switch){

				//p1putstr(1, "IN CHILD CONTROL IF BLOCK\n");
				struct Process *currentProcess;
		      		myQueue->dequeue(myQueue, &currentProcess);
			
				printf("currentProcess - myPid: %d, started %d, running %d, done %d\n", currentProcess->myPid, currentProcess->started, currentProcess->running, currentProcess->done);

				printf("lastProcess - myPid: %d, started %d, running %d, done %d\n", lastProcess->myPid, lastProcess->started, lastProcess->running, lastProcess->done);
				if(lastProcess->running && !(lastProcess->done)){
					kill(lastProcess->myPid, SIGSTOP);
					lastProcess->running = 0; // this creates an infinite loop!
					// PICK UP HERE!!!
					printf("Enqueuing process after stopping it\n");
					myQueue->enqueue(myQueue, lastProcess);
				}

				if(!(currentProcess->started)){
					lastProcess = currentProcess;		
					kill(currentProcess->myPid, SIGUSR1); //starts process for the first time
					currentProcess->started = 1;
					currentProcess->running = 1;
				}
				else if(!(currentProcess->running)){
					lastProcess = currentProcess;		
					kill(currentProcess->myPid, SIGCONT);
					currentProcess->running = 1;
				}
				time_to_switch = 0;
			}
			//pause(); // THIS COULD CAUSE PROBLEMS...
		}
	}
	// END NEW CHILD PROCESS CONTROL SEQUENCE	
	
	// close file, free fileName, and return
	free(fileName);
	close(fd);
	myQueue->destroy(myQueue);
	return EXIT_SUCCESS;
}
