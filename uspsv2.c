#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "p1fxns.h"

#define USESTR "usage: [WORKLOAD_FILE] ... \n"
#define UNUSED __attribute__((unused))

// use standard functions like fprint() to start off with,
// then come back and replace them with system calls like write()
// after uspsv1 is working

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
		pid_t pid = fork();
		if(pid == 0){
			//p1putstr("args[0]: %s\n", args[0]);	
			execvp(args[0], args);
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
