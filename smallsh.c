#define _GNU_SOURCE			// Necessary to mute warning about getline and for -std=c99
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

// Custom defines
#define DEBUG 1

// Function prototypes
void cd();
void exitProg();

int main(){
	
	// User input variables
	char* userIn = NULL;		// Points to buffer allocated by getline(), holds string + \n + \0
	size_t buffer = 0;			// Holds how big allocated buffer is
	int numChars = -5;			// How many chars user entered

	// New process variables
	pid_t spawnPid = -5;
	int childExitStatus = -5;

	// String splitting variables
	char* token;				// Will be used with strtok to split lines of file to get info I want

	// Command variables
	char cmd[2048];
	char* args[512];
	int argCount;

	// Begin console prompt
	while(1){
		// Reset necessary variables
		argCount = 0;
		memset(cmd, '\0', sizeof(cmd));
		memset(args, '\0', sizeof(args));

		printf(": ");
		fflush(stdout);

		// Get user input
		numChars = getline(&userIn, &buffer, stdin);	// Get line from user
		userIn[numChars - 1] = '\0';

		// Built-in shell functions	
		if(strcmp(userIn, "exit") == 0){
			exitProg();
		}
		else if(strcmp(userIn, "cd") == 0){
			printf("You entered 'cd'.\n");
		}
		else if(strcmp(userIn, "status") == 0){
			printf("You entered 'status'.\n");
		}
		else if(strcmp(userIn, " ") == 0 || userIn[0] == '#' || strcmp(userIn, "") == 0){
			continue;
		}
		else{
			/**********************
			 * TODO: Need to expand $$. Easiest thing to do
			 * is to get the whole line, see if there are any $$'s
			 * in the line, if there are, expand the $$. Basically,
			 * create a new line with the expanded $$'s, even if there
			 * are multiple $$'s to expand. THEN split up the args like
			 * we're doing below.
			 **********************/
			token = strtok(userIn, " ");
			while (token != NULL){
				if(argCount == 0){		// First argument is command, rest are arguments
					strcpy(cmd, token);	
				}
				else{
					args[argCount] = token;
				}	
				argCount++;
				token = strtok(NULL, " ");	// Start search from the NULL strtok puts at the delim spot
			}
		}
	
		if(DEBUG){
			printf("num args: %d\n", argCount);
			printf("cmd: %s\nargs: ", cmd);

			for(int i = 1; i < argCount; i++){
				printf("%s", args[i]);
				if(i != argCount-1){
					printf(", ");
				}
			}
			printf("\n");
		}
		fflush(stdout);	
	}

	return 0;
}

void cd(){

}

/****************
 * Exits program. Shell will kill any other processes or jobs
 * that are currently running before exiting.
 ****************/
void exitProg(){
	exit(0);
}
