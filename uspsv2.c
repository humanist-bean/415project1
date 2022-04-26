#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include "p1fxns.h"


#define USESTR "usage: [WORKLOAD_FILE] ... \n"
#define UNUSED __attribute__((unused))

// GLOBAL VARIABLES
int signalled = 0;

void signal_handler(int sig){
	//p1putstr(1, "MADE IT! inside signal handler\n");
	//execvp(args[0], args);
	signalled++;
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
	
	// BEGIN IMPROVED INPUT PROCESSING SECTION
	int quantum = -1;
	char *quantum_string;
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

	// close file, free fileName, and return
	free(fileName);
	close(fd);
	return EXIT_SUCCESS;
}
