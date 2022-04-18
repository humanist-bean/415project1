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
	char *string;
	int status;
	int index;

	if(argv[1] == NULL){
		printf("ERROR: You must provide a file name \n");
		return EXIT_FAILURE;
	} else {
		fileName = p1strdup(argv[1]);
	}

	//open file and save words to array
	fd = open(fileName, O_RDONLY);
	if(fd == -1){
		printf("ERROR: Could not open file fd in main\n");
		return EXIT_FAILURE;
	}
	
	int i = 0;
	while(p1getline(fd, buf, sizeof buf) != 0){
		buf[p1strlen(buf)] = "\0";
		index = p1getword(buf, sizeof buf, string);
		if(index == -1){
			printf(" p1getword returned -1, we are at end of buf i think\n");
		}
		args[i] = p1strdup(string); // duplicate command name into args
		i++;
		args[i] = p1strdup((&buf[index])); // duplicate command arguments into args
		i++;
	}
	args[i] = NULL; // sentinel so we know when out of args

	// fork, execute, and join
	i = 0;
	int pid[100];
	while(args[2*i + 1] != NULL){
		pid[i] = fork();
		if(pid[i] == 0){
			//test
			printf("args: %s, %s \n", args[2*i], args[2*i + 1]);
			//end test
			execvp(args[2*i], args[2*i + 1]);
		}
		i++;
	}
	for(int j = 0; j < i; j++){
		wait(pid[i]);
	}

	return EXIT_SUCCESS;
}
