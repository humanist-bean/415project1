#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <signal.h>
#include "p1fxns.h"


#define USESTR "usage: [WORKLOAD_FILE] ... \n"
#define UNUSED __attribute__((unused))

// GLOBAL VARIABLES
int signalled = 0;
int active_processes;
// Need to use queue or array of pids (as circular buffer using %)
// to keep track of active processes
//pid_t paused_processes;
//pid_t continued_processes;

void signal_handler(int sig){
	//p1putstr(1, "MADE IT! inside signal handler\n");
	//execvp(args[0], args);
	signalled++;
}

/* SIGCHLD Handler from SYSC 1.4.6 */
void child_signaled(int sig){ // this is called every time a child makes a signal, which occurs
	// every time a major event occurs in the child proccess
	pid_t pid;
	int status;

	while((pid = waitpid(-1, &status, WNOHANG)) > 0){
		if(WIFEXITED(status) || WIFSIGNALED(status)){ // I think we just use WIFEXITED here?
			// might also want to do if checks for all the different signals to determine
			// how we manage our queues/array
			// E.G.: WIFCONTINUED, WIFSTOPPED, etc...
			--active_processes;
		}
	}
}

void timer_signaled(int sig){ // this should be called upon every SIGALARM from settimer
	asdf;//pick up here, don't forget to register SIGALARM in main !
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
	// their respective arguments
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
			//p1putstr(1, "saving pid to pids in parent\n");
			pids[waits-1] = pid;
			//sleep(1);
		} else{
			p1putstr(1, "FORK FAILED!\n");
		}
		// noticed memory leaks caused by strdup, so i free here
		for(int j = 0; j < i; j++){
			free(args[j]);
		}
	}
	
	// NEW CHILD PROCESS CONTROL SEQUENCE WITH SETITIMER
	int lines = waits;
	if(pid > 0){
		sleep(1); //wait for child processes to register their signals
		
		active_processes = lines; // set Global here for use in child_alarmed()

		// set parent's SIGCHLD to child_signaled
		// so its called upon each signal from child to parent,
		// which I BELIEVE will be sent whenever a child:
		// 1. Completes (WIFEXITED) 2. Is paused (WIFSTOPPED) 3. Is continued (WIFCONTINUED)
		if(signal(SIGCHLD, child_signaled) == SIG_ERR){
			p1perror(1, "ERROR - unable to register SIGCHLD in parent\n");
			return EXIT_FAILURE;
		}

		//initialize it_val with INTERVAL, eventually this should be given by
		// QUANTUM in either args or a environment variable
		struct itimerval it_val;
		it_val.it_value.tv_sec = 0; // not sure exactly how to set tv_sec and tv_usec
		it_val.it_value.tv_usec = 10000; // maybe they should match? IDK...
		it_val.it_interval = it_val.it_value;
		
		
		// use setitimer to send signal alarm every BLANK milliseconds
		if(setitimer(ITIMER_REAL, &it_val, NULL) == -1){
			p1perr(1, "ERROR - setitimer() failed.\n");
			return EXIT_FAILURE;
		} // now SIGALARM should be sent by setitimer every interval in &it_val
	}
	// END NEW CHILD PROCESS CONTROL SEQUENCE
	
	// original child process control sequence from uspsv2
	int lines = waits;
	if(pid > 0){
		// do we have to manually wait for children to register?
		// seems silly but I don't know any other way to do it yet...
		sleep(1);	
		// send SIGUSR1 to each child
		for(int k = 0; k < lines; k++){
		       kill(pids[k], SIGUSR1);
		}
		//send SIGSTOP to each child
		for(int k = 0; k < lines; k++){
		       kill(pids[k], SIGSTOP);
		}
		//send SIGCONT to each child
		for(int k = 0; k < lines; k++){
		       kill(pids[k], SIGCONT);
		       //p1putstr(1, "CONT SIGNAL SENT FROM PARENT!");
		}
    				
	}	

	while(waits > 0){
		wait(NULL);
		--waits;
	}
	// END ORIGINAL CHILD PROCESS CONTROL SEQUENCE
	
	// close file, free fileName, and return
	free(fileName);
	close(fd);
	return EXIT_SUCCESS;
}
