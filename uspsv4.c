#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <signal.h>
#include "p1fxns.h"
#include "ADTs/queue.h"


#define USESTR "usage: ./uspsv4 [-q <quantum in msec>] [WORKLOAD_FILE] ... \n"
#define UNUSED __attribute__((unused))

// GLOBAL VARIABLES
int signalled = 0;
// Need to use queue or array of pids (as circular buffer using %)
// to keep track of active processes
//pid_t paused_processes;
//pid_t continued_processes;
int active_processes;
int time_to_switch = 0;
// child manager while loop runs till waits = 0;
int waits = 0;
// for tracking when to dump proc info
int time_to_dump = 0; // decrement each switch, dump when time_to_dump = 0

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

	while((pid = waitpid(-1, &status, WNOHANG)) > 0){
		if(WIFEXITED(status)){ // I think we just use WIFEXITED here?
			// might also want to do if checks for all the different signals to determine
			// how we manage our queues/array
			// E.G.: WIFCONTINUED, WIFSTOPPED, etc...
			lastProcess->running = 0; // prevents lastProcess from being enqueued again
			lastProcess->done = 1;
			--waits;
		} // else do nothing? note child_signaled is called for every tim SIGCHLD is sent,
		// which for all I know could be every time the child is continued, stopped, etc...
		// or just when it finishes...
	}
}

void timer_signaled(UNUSED int sig){ // this should be called upon every SIGALARM from settimer
	time_to_switch = 1;
	--time_to_dump;
}

int main( int argc, char *argv[]){
	// start by getting processing argv to find workload file
	// if no workload file is given, process standard input
	const char *fileName;
	int fd;
	char buf[BUFSIZ];
	char command[BUFSIZ];
	char *args[BUFSIZ];
	int status;
	//variables for setting quantum time
	int quantum = -1;
	char *quantum_string;
	// variables to track pids and signals
	pid_t pid;
	pid_t pids[100];
	sigset_t signalset;
	int sig;
	// variables for uspsv4 print proc info
	char *procFileName;
	char pidStr[25];
	int procfd;
	int lines;

	// BEGIN IMPROVED INPUT PROCESSING SECTION
	// get command line arguments and environment variables
	if((quantum_string = getenv("USPS_QUANTUM_MSEC")) != NULL){
		quantum = atoi(quantum_string);
	}
	if(argc == 4 && (p1strneq(argv[1], "-q", 2))){
		// case 1: quantum is given
		quantum = p1atoi(argv[2]);
		fileName = p1strdup(argv[3]);
	} else if(argc == 2){
		// case 2: just filename is given
		fileName = p1strdup(argv[1]);
	} else {
		p1putstr(1, "ERROR - invalid command arguments\n");
		p1putstr(1, USESTR);
		return EXIT_FAILURE;
	}
	
	fd = open(fileName, O_RDONLY);
	if(fd == -1){
		p1putstr(1, "ERROR: Could not open file fd in main\n");
		return EXIT_FAILURE;
	}

	if(quantum < 1){
		p1perror(1, "ERROR - quantum time must be > 0\n");
		return EXIT_FAILURE;
	}
	if(quantum == NULL){
		p1perror(1, "ERROR - Quantum value not specified in environment variable or command line\n");
		return EXIT_FAILURE;
	}
	// END IMPROVED INPUT CAPTURE SECTION
	
	// Initialize Queue
	if ((myQueue = Queue_create(freeValue)) == NULL){
		p1perror(1, "ERROR - failed to create stack ADT Queue\n");
		return EXIT_FAILURE;
	}

	// initialize and add SIGUSR1 to signalset
	status = sigemptyset(&signalset);
	if(sigaddset(&signalset, SIGUSR1) == -1){
		p1putstr(1, "ERROR - failed to add signal to signal set\n");
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
			signal(SIGUSR1, signal_handler);
			sigwait(&signalset, &sig);
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
		childProcess[j].myPid = pids[j];
		childProcess[j].running = 0;
		childProcess[j].started = 0;
		childProcess[j].done = 0;
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
		it_val.it_value.tv_sec = 0; // was 1 when working // not sure exactly how to set tv_sec and tv_usec
		it_val.it_value.tv_usec = quantum; // maybe they should match? IDK...
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

		time_to_dump = lines = waits; // we want to run our first dump after starting each process
		time_to_dump = 0; //TEST
		while(waits){ //runs till queue is empty
			if(time_to_switch){
				struct Process *currentProcess;
		      		myQueue->dequeue(myQueue, &currentProcess);
			

				if(lastProcess->running && !(lastProcess->done)){
					//p1putstr(1, "1!\n");
					kill(lastProcess->myPid, SIGSTOP);
					lastProcess->running = 0; 
					myQueue->enqueue(myQueue, lastProcess);
				}

				if(!(currentProcess->started)){
					//p1putstr(1, "2!\n");
					lastProcess = currentProcess;		
					kill(currentProcess->myPid, SIGUSR1); //starts process for the first time
					currentProcess->started = 1;
					currentProcess->running = 1;
				}
				else if(!(currentProcess->running)){
					//p1putstr(1, "3!\n");
					lastProcess = currentProcess;		
					kill(currentProcess->myPid, SIGCONT);
					currentProcess->running = 1;
				}
				time_to_switch = 0;
			}
			// uspsv4 print proc info, code here
			//printf("TEST: time_do_dump = %d\n", time_to_dump);
			if(time_to_dump < 1){
				for(int z = 0; z < lines; z++){
					// open proc file by creating string for path
					char procTemplate[50];
					p1itoa(pids[z], pidStr);
					p1strcat(procTemplate, "/proc/");
					p1strcat(procTemplate, pidStr);
					p1strcat(procTemplate, "/stat");
					procFileName = p1strdup(procTemplate);
					//procTemplate is ready to use as way to open file
					//now its time to open the file
					printf("TEST: procTemplate = %s\n", procTemplate);
					procfd = open(procTemplate, O_RDONLY);
					if(procfd == -1){
						p1putstr(1, "ERROR: Could not open file procfd in main\n");
						//return EXIT_FAILURE;
						goto next_process;
					} else{
						printf("SUCCESSFULLY OPENED A PROC FILE\n");
					}
					//file is open, now we print contents in formatted manner
					int test = 0;
					while(p1getline(procfd, buf, sizeof buf) != 0){	
						//print shit here
						printf("Proc buf #%d: %s\n", test, buf);
						++test;

					}
					close(procfd);
					next_process:
						p1strcpy(procTemplate, ""); //clear procTemplate for next process
						free(procFileName);
			
				}
				time_to_dump = 1000; // SET DUMP FREQUENCY HERE!
			}
		}
	}
	// END NEW CHILD PROCESS CONTROL SEQUENCE	
	
	// close file, free fileName, and return
	free(fileName);
	close(fd);
	myQueue->destroy(myQueue);
	return EXIT_SUCCESS;
}
