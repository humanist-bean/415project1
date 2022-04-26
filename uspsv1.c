/*
Name: Alder French, DuckID: afrench7, Assignment: CIS 415 Project 1

This is my own work, except for some help from Nick the TA, he helped
me with uspsv1.c in office hours. Thanks for the help Nick!
*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "p1fxns.h"

#define USESTR "usage: ./uspsv3 [-q <quantum in msec>] [WORKLOAD_FILE] ... \n"
#define UNUSED __attribute__((unused))

// use standard functions like fprint() to start off with,
// then come back and replace them with system calls like write()
// after uspsv1 is working.

int main( UNUSED int argc, char *argv[]){
	// start by getting processing argv to find workload file
	// if no workload file is given, process standard input
	const char *fileName;
	int fd;
	char *args[BUFSIZ];
	char buf[BUFSIZ];
	char command[BUFSIZ];
	int status;
	int waits = 0;
	
	int quantum = -1;
	char *quantum_string;
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
		pid_t pid = fork();
		if(pid == 0){
			//p1putstr("args[0]: %s\n", args[0]);	
			execvp(args[0], args);
			//printf("Made it!\n"); // remove me, just testing if code after exexvp ever runs
		}
		// noticed memory leaks caused by strdup, so i free here
		for(int j = 0; j < i; j++){
			free(args[j]);
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
