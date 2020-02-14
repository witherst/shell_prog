#define _GNU_SOURCE			// Necessary to mute warning about getline and for -std=c99
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>

// Custom defines
#define DEBUG 1

// Function prototypes
void cd();
void exitProg(char*, char**, char*, char*, char*, char*);
int pidLength(int);
void getCmdAndArgs(char*, char**, char*, int*);
void expandString(char*, char*, int, int);
void forkAndExec(char*, char**, int, char*, char*);
void changeDirs(char**, int);
void getFileNames(char*, char*, char**, int);
bool isAmp(char**, int);	// TODO: Think of better name for this
void cullArgs(char*, char*, char**, int*);

int main(){

	// Redirect and file variables
	bool hasAmp;
	char* inFileName;
	char* outFileName;

	// Shell variables
	int sPid = getpid();				// Shell process id
	int pidLen = pidLength(sPid);		// Will hold length of the pid
	
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
	char* cmd;
	char** args;
	char* expStr;				// Will be expanded string (replacing '$$' with pid)
	int argCount;

	// Allocate cmd to have [2048] length and args to be 512 length
	// Also, allocate other dynamic memory
	cmd = malloc(2048 * sizeof(char));
	args = calloc(512, sizeof(char*));
	expStr = malloc(2048 * sizeof(char));
	inFileName = calloc(256, sizeof(char));
	outFileName = calloc(256, sizeof(char));

	// Begin console prompt
	while(1){
		// Reset necessary variables
		hasAmp = false;
		argCount = 0;

		memset(cmd, '\0', sizeof(cmd));
		memset(args, '\0', sizeof(args)*sizeof(char*));
		memset(expStr, '\0', sizeof(expStr));
		memset(inFileName, '\0', sizeof(inFileName));
		memset(outFileName, '\0', sizeof(outFileName));

		printf(": ");
		fflush(stdout);

		// Get user input
		numChars = getline(&userIn, &buffer, stdin);	// Get line from user
		userIn[numChars - 1] = '\0';

		// Built-in shell functions	
		if(strcmp(userIn, "exit") == 0){
			exitProg(cmd, args, userIn, expStr, inFileName, outFileName);
		}
		else if(strcmp(userIn, "status") == 0){
			printf("You entered 'status'.\n");
		}
		else if(strcmp(userIn, " ") == 0 || userIn[0] == '#' || strcmp(userIn, "") == 0){	// Comment line, skip
			continue;
		}
		else{
			// Expand user string
			expandString(userIn, expStr, sPid, pidLen);

			// Parse expanded string and get the command and args
			getCmdAndArgs(cmd, args, expStr, &argCount);

			// Check for ampersand at end of user input
			hasAmp = isAmp(args, argCount);

			// Check for redirection and get filenames if necessary
			getFileNames(inFileName, outFileName, args, argCount);

			// Check file names and "cull" these arguments out of the array
			cullArgs(inFileName, outFileName, args, &argCount);

			printf("inFileName: %s, outFileName: %s\n", inFileName, outFileName);

			if(strcmp(cmd, "cd") == 0){
				changeDirs(args, argCount);
			}
			else{
				// Fork and exec
				forkAndExec(cmd, args, argCount, inFileName, outFileName);
			}
		}

		// Print out various debug info
		if(DEBUG){
			printf("\n***********DEBUG*************\n");
			printf("Command: %s\nArgs: ", cmd);
		
			for(int i = 0; i < argCount + 3; i++){
				printf("%s", args[i]);
				if(i != argCount-1){
					printf(", ");
				}
			}
			printf(". Total: %d\n", argCount);
		}
		fflush(stdout);	
	}

	return 0;
}

/**********************
 * Function will look check to see if inFileName and/or outFileName
 * exist. If one or both exists, we will subtract the argCount. Ex:
 * "ls -la > junk", argCount will be 4. We will make argCount 2 and
 * get rid of "> junk" in the arg list
 **********************/
void cullArgs(char* inFileName, char* outFileName, char** args, int* argCount){
	char* stop;

	stop = malloc(2*sizeof(char));
	stop[0] = '>';
	stop[1] = '\0';

	// If there is no out or in file name, we're not doing redirection,
	// so change nothing
	if(strcmp(inFileName, "") == 0 && strcmp(outFileName, "") == 0){
		return;
	}
	
	// Find out if we have an in file names, if we do
	// set a character to stop at with our while loop
	if(strcmp(inFileName, "")){
		strcpy(stop, "<");
	}	
	
	while(strcmp(args[*argCount-1], stop) != 0){
		args[*argCount-1] = '\0';		// Nullify this argument
		*argCount -= 1;			
	} 

	// By this point we've gotten to the final "<" or ">", go one more
	*argCount -= 1;	
	args[*argCount] = '\0';
	
	free(stop);

}

/*************************
 * Function will look through the arguments array and
 * if it finds a "<" or ">", will set the name of the
 * input or outfile associated with the redirection
 *************************/
 void getFileNames(char* inFileName, char* outFileName, char** args, int count){
	// Start at end of args, go until we reach a "<"
	// If we find a ">" or "<", the element to the "right", 
	// i.e., i+1, will the file name (in or out)
	for(int i = count-2; i > 0; i--){
		if(strcmp(args[i], "<") == 0){
			// If we get into here, break from the loop (there won't be a ">" to the left of a "<" that we care about
			strcpy(inFileName, args[i+1]);
			break;
		}
		else if(strcmp(args[i], ">") == 0){
			strcpy(outFileName, args[i+1]);	
		}
	}
 } 

/*********************
 * Function just looks for an "&" in the last
 * argument spot. If it's there, return true
 *********************/
bool isAmp(char** args, int count){
	if(strcmp(args[count-1], "&") == 0){
		return true;	
	}
	return false;
}

void changeDirs(char** args, int argc){	
	// If argc is 1, i.e., the user just entered 'cd'
	if(argc == 1){
		chdir(getenv("HOME"));	
	}
	else{
		chdir(args[1]);	
	}	
}

/***************************
 * Write stuff
 ***************************/
void forkAndExec(char* cmd, char** args, int argc, char* inFileName, char* outFileName){
	// File vars	
	int targetFD, sourceFD;
	int result;

	// Other vars
	pid_t spawnPid = -5;
	pid_t actualPid = -5;
	int childExitStatus = -5;
	spawnPid = fork();

	switch(spawnPid){
		case -1: 
			perror("Spawning fork went wrong!\n");
			exit(1);
			break;
		case 0:			// Child
			if(outFileName){
				targetFD = open(outFileName, O_WRONLY | O_CREAT | O_TRUNC, 0644);
				fcntl(targetFD, F_SETFD, FD_CLOEXEC);	// Close on exec
				result = dup2(targetFD, 1);
			}
			if(inFileName){
				sourceFD = open(inFileName, O_RDONLY);	
				fcntl(sourceFD, F_SETFD, FD_CLOEXEC);	// Close on exec
				result = dup2(sourceFD, 0);
			}
			execvp(*args, args);
			break;
		default:		// Parent
			actualPid = waitpid(spawnPid, &childExitStatus, 0);	// Wait for child to finish before moving on
			break;
	}
}

/**********************
 * This function will look for '$$' anywhere in the input,
 * and replace the '$$' with the shell's processor id. The
 * expanded string will be returned.
 **********************/
void expandString(char* input, char* new, int pid, int pidLen){
	char sPid[pidLen];
	
	// Convert the pid to a string
	sprintf(sPid, "%d", pid);

	// Step through input char, by char and copy input
	// into new
	int i = 0;
	while(*input){
		if(strstr(input, "$$") == input){
			// We've hit an occurrence of $$, so put the actual
			// stringified pid at this location of the string
			strcpy(&new[i], sPid);
			i += pidLen;			// Move array index forward the length of the actual process id
			input += 2;				// Move the array index of our input forward 2 spaces, the length of $$.
		}
		else{
			new[i] = *input;		// Copy character from input array into new string
			i++;					// Advance input counter
			*input++;				// Advance input array pointer by 1
		}
	}

	// Add terminating null character
	new[i] = '\0';
}

/************************
 * This function takes in an array for the cmd, 
 * a double array for the arguments, the actual user input,
 * and the argument count (which will be updated by 
 * the function. Also, a NULL will be apended to the end of the
 * argument array.
 ************************/
void getCmdAndArgs(char* cmd, char** args, char* userIn, int* argCount){
	// String splitting variables
	char* token;				// Will be used with strtok to split lines of file to get info I want
	token = strtok(userIn, " ");

	while (token != NULL){
		if(*argCount == 0){		// First argument is command, rest are arguments
			strcpy(cmd, token);	
		}
		args[*argCount] = token;
		*argCount += 1;
		token = strtok(NULL, " ");	// Start search from the NULL strtok puts at the delim spot
	}
	args[*argCount+1] = NULL; 		// Necessary for execvp()
}

/****************
 * Will return the length of the passed
 * in integer. This will be used
 * later for string expansion.
 ***************/
int pidLength(int pid){
	
	int count = 1;
	while(pid/10 != 0){
		pid /= 10;
		count++;
	}

	return count;
}

void cd(){

}

/****************
 * Exits program. Shell will kill any other processes or jobs
 * that are currently running before exiting. Also, memory
 * will be freed from cmd and args array. And the sneaky
 * userIn that's malloced within strtok.
 ****************/
void exitProg(char* cmd, char** args, char* userIn, char* expStr, char* inFileName, char* outFileName){
	free(cmd);
	free(args);
	free(userIn);
	free(expStr);
	free(inFileName);
	free(outFileName);
	exit(0);
}
