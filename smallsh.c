#define _GNU_SOURCE			// Necessary to mute warning about getline and for -std=c99
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(){
	
	// User input variables
	char* userIn = NULL;		// Points to buffer allocated by getline(), holds string + \n + \0
	size_t buffer = 0;			// Holds how big allocated buffer is
	int numChars = -5;			// How many chars user entered

	// Begin console prompt
	while(1){
		printf(": ");
		fflush(stdout);

		// Get user input
		numChars = getline(&userIn, &buffer, stdin);	// Get line from user
		userIn[numChars - 1] = '\0';

		// Built-in shell functions	
		if(strcmp(userIn, "exit") == 0){
			printf("You entered 'exit'.\n");	
		}
		else if(strcmp(userIn, "cd") == 0){
			printf("You entered 'cd'.\n");
		}
		else if(strcmp(userIn, "status") == 0){
			printf("You entered 'status'.\n");
		}
		else{
			printf("You didn't enter a recognized command.\n");
		}

		fflush(stdout);

		
	}

	return 0;
}
