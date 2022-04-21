#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "p1fxns.h"
#include <signal.h>

#define USESTR "usage: [WORKLOAD_FILE] ... \n"
#define UNUSED __attribute__((unused))

// GLOBAL VARIABLES
char *args[BUFSIZ]; // so we can call execvp from within the signal_handler 

void signal_handler(int sig){
	p1putstr(1, "Called signal handler function from child proccess\n");
}
	

int main( UNUSED int argc, char *argv[]){
	// start by getting processing argv to find workload file
	// if no workload file is given, process standard input
	const char *fileName;
	int fd;
	//char *args[BUFSIZ];
	char buf[BUFSIZ];
	char command[BUFSIZ];
	int status;
	int waits = 0;
	char *args[BUFSIZ];
	pid_t pid;
	pid_t pids[100];

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
		if(pid < 0){
			p1putstr(1, "ERROR: pid < 0 after fork(), so fork error...\n");
			return EXIT_FAILURE;
		}//child process
		else if(pid == 0){
			//p1putstr("args[0]: %s\n", args[0]);
			p1putstr(1, "Test\n");
			signal(SIGUSR1, signal_handler); // signal handler should now be called 
			// in child process upon receiving SIGUSR1 from parent process
			while(signalled < 1){
				// do nothing here, so we are just waiting for
				// signalled to be incremented
			}
			execvp(args[0], args);
			--signalled;
		}
		// Parent Process
		else {
			/*start by signalling child process
			while(ready < waits){
				sleep(1);
			}*/
			p1putstr(1, "In parent process, collecting pids for later\n");
			pids[waits-1] = pid;
			//kill(pid, SIGUSR1);
		}
		// noticed memory leaks caused by strdup, so i free here
		for(int j = 0; j < i; j++){
			free(args[j]);
		}
	}
	
	if(pid > 0){ // we don't want child processes in here after above
		for(int k = 0; k < waits; k++){
			p1putstr(1, "Calling kill signal...\n");
			kill(pids[k], SIGUSR1);
		}
	}
	
	
	// wait for children to finish
	while(waits > 0){
		wait(NULL);
		--waits;
	}

	// close file, free fileName, and return
	free(fileName);
	close(fd);
	return EXIT_SUCCESS;
}
