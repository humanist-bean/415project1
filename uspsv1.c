#include <stdlib.h>
#include <stdio.h>
#include "p1fxns.h"

#define USESTR "usage: [WORKLOAD_FILE] ... \n"
#define UNUSED __attribute__((unused))

// use standard functions like fprint() to start off with,
// then come back and replace them with system calls like write()
// after uspsv1 is working

int main( UNUSED int argc, char *argv[]){
	// start by getting processing argv to find workload file
	// if no workload file is given, process standard input
	char *fileName;
	FILE *fp;
	char *args[BUFSIZ];
	char buf[BUFSIZ];

	if(argv[1] == NULL){
		printf("ERROR: You must provide a file name \n");
		return EXIT_FAILURE;
	} else {
		fileName = p1strdup(argv[1]);
	}

	//open file and save words to array
	fp = fopen(fileName, "r");
	if(fp == NULL){
		printf("ERROR: Could not open file fp in main\n");
		return EXIT_FAILURE;
	}
	
	int i = 0;
	while(fgets(buf, sizeof buf, fp) != NULL){
		args[i] = p1strdup(buf);
		i++;
	}
	args[i] = NULL; // sentinel so we know when out of args

	// fork, execute, and join
	i = 0;
	int pid[100];
	while(args[2*i + 1] != NULL){
		pid[i] = fork();
		if(pid[i] == 0){
			execvp(args[2*i], args[2*i + 1]);
		}
		i++;
	}
	for(int j = 0; j < i; j++){
		wait(pid[i]);
	}

	return EXIT_SUCCESS;
}
