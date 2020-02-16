#define _GNU_SOURCE			// Necessary to mute warning about getline and for -std=c99
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

// Custom defines
#define DEBUG 0
#define MAXPID 512

// Function prototypes
void cd();
void exitProg(char*, char**, char*, char*, char*, char*, int*);
int pidLength(int);
void getCmdAndArgs(char*, char**, char*, int*);
void expandString(char*, char*, int, int);
void forkAndExec(char*, char**, int, char*, char*, int*, bool, int**, int*);
void changeDirs(char**, int);
void getFileNames(char*, char*, char**, int);
bool isAmp(char**, int);	// TODO: Think of better name for this
void cullArgs(char*, char*, char**, int*, bool);
void checkStatus(int);
bool isEmpty(int);
bool containsPid(int*, int);
void addPid(int*, int, int*);
void printChildPids(int*, int);
void checkForTerm(int*, int*);
void setupSignals();
void catchSIGTSTP(int);

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
	int* childPids;				// Array to hold child PIDs that are running in the bg
	int childCount = 0;

	// String splitting variables
	char* token;				// Will be used with strtok to split lines of file to get info I want

	// Command variables
	char* cmd;
	char** args;
	char* expStr;				// Will be expanded string (replacing '$$' with pid)
	int argCount;

	// Setup signals for SIGINT and SIGSTP
	setupSignals();

	// Allocate cmd to have [2048] length and args to be 512 length
	// Also, allocate other dynamic memory
	cmd = malloc(2048 * sizeof(char));
	args = calloc(512, sizeof(char*));
	expStr = malloc(2048 * sizeof(char));
	inFileName = calloc(256, sizeof(char));
	outFileName = calloc(256, sizeof(char));
	childPids = malloc(MAXPID * sizeof(int));
	
	// Begin console prompt
	while(1){	
		// Check child processes for termination
		checkForTerm(childPids, &childCount);

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
		
		// Get line from user
		numChars = getline(&userIn, &buffer, stdin);

		// If numChars is -1, getline was interrupted (as far as I know), go back to top of while loop
		if(numChars == -1){
			clearerr(stdin);
			continue;
		}
	
		userIn[numChars - 1] = '\0';		

		// Expand user string
		expandString(userIn, expStr, sPid, pidLen);

		// Parse expanded string and get the command and args
		getCmdAndArgs(cmd, args, expStr, &argCount);	

		// Built-in shell functions	
		if(argCount < 1 || strcmp(args[0], " ") == 0 || args[0][0] == '#'){	// Comment line, skip
			continue;
		}
		else if(strcmp(args[0], "exit") == 0){
			if(argCount > 1){	// If argument count is greater than 1, then this isn't valid, ignore
				continue;
			}
			exitProg(cmd, args, userIn, expStr, inFileName, outFileName, childPids);
		}
		else if(strcmp(args[0], "status") == 0){
			if(argCount > 1){	// If argument count is greater than 1, then this isn't valid, ignore
				continue;
			}
			checkStatus(childExitStatus);
		}	
		else{	
			// Check for ampersand at end of user input
			hasAmp = isAmp(args, argCount);

			// Check for redirection and get filenames if necessary
			getFileNames(inFileName, outFileName, args, argCount);

			// Check file names and "cull" these arguments out of the array along with an ampersand if there is one
			cullArgs(inFileName, outFileName, args, &argCount, hasAmp);

			if(strcmp(cmd, "cd") == 0){
				changeDirs(args, argCount);
			}
			else{
				// Fork and exec
				forkAndExec(cmd, args, argCount, inFileName, outFileName, &childExitStatus, hasAmp, &childPids, &childCount);
			}
		}

		// Print out various debug info
		if(DEBUG){
			printf("\n***********DEBUG*************\n");
			printf("Command: %s\nArgs: ", cmd);
		
			for(int i = 0; i < argCount; i++){
				printf("%s", args[i]);
				if(i != argCount-1){
					printf(", ");
				}
			}
			printf(". Total: %d\n", argCount);
			
			printf("*******END DEBUG*************\n");
			fflush(stdout);	
		}
	}

	return 0;
}

void setupSignals(){
	// Signal handling for SIGINT
	struct sigaction ignore_action = {0};		// Init struct to be empty for CTRL+C (SIGINT)
	ignore_action.sa_handler = SIG_IGN;
	ignore_action.sa_flags = SA_RESTART;		// Restart system calls

	sigaction(SIGINT, &ignore_action, NULL);	// Ignore SIGINT

	// Signal handling for SIGSTP
	struct sigaction stp_action = {0};
	stp_action.sa_handler = catchSIGTSTP;
	sigfillset(&stp_action.sa_mask);
	//stp_action.sa_flags = SA_RESTART;

	sigaction(SIGTSTP, &stp_action, NULL);		// Catch SIGTSTP
}

void catchSIGTSTP(int signo){
	char* message = "\nEntering FG Mode\n";
	write(STDOUT_FILENO, message, 18);
}

/**********************
 * Function checks for termination of a child process. Given an
 * array of ints (childPids) and the number of childPids (count),
 * we'll loop through and check for any child processes that
 * have terminated.
 **********************/
void checkForTerm(int* childPids, int* count){
	int exitStatus;
	int check;
	int exit = -1;

	for(int i = 0; i < *count; i++){
		check = waitpid(childPids[i], &exitStatus, WNOHANG);

		// If check > 0, process has finished, get exitStatus and print
		if(check > 0){
			if(WIFEXITED(exitStatus) != 0){
				exit = WEXITSTATUS(exitStatus);
				printf("background pid %d is done: exit value %d\n", childPids[i], exit);
			}
			else if(WIFSIGNALED(exitStatus) != 0){
				exit = WTERMSIG(exitStatus);
				printf("background pid %d is done: terminated by signal %d\n", childPids[i], exit);
			}	

			// Remove pid from the array, i.e., move down values one slot
			for(int j = i; j < *count-1; j++){
				childPids[j] = childPids[j+1];
			}
			*count -= 1;
		}
	}	
}

void printChildPids(int* childPids, int childCount){
	for(int i = 0; i < childCount; i++){
		printf("id[%d]: %d\n", i, childPids[i]);
	}
	printf("\n");

}

/******************
 * Function will add the passed in pid if
 * it doesn't already exist in the array
 ******************/
void addPid(int* childPids, int pid, int* childCount){
	bool add = true;

	// Check if pid exists in array
	if(!isEmpty){
		for(int i = 0; i < *childCount; i++){
			if(childPids[i] == pid){
				false;	
			}
		}
	}

	// Add pid if it doesn't exist in the array
	if(add){
		fflush(stdout);
		if(*childCount >= MAXPID-1){
			return;		// Don't add pid if we have too many processes
		}
		else{
			childPids[*childCount] = pid;
			*childCount += 1;
		}
	}
}

bool isEmpty(int childCount){
	return childCount == 0;
}

/*********************
 * Function will print the exit status
 * of parm stat
 *********************/
void checkStatus(int stat){
	int exitStatus;

	if(stat < 0){
		printf("exit value 0\n");
		fflush(stdout);
	}	
	else if(WIFEXITED(stat) != 0){
		exitStatus = WEXITSTATUS(stat);
		printf("exit value %d\n", exitStatus);
		fflush(stdout);
	}
	else if(WIFSIGNALED(stat) != 0){
		exitStatus = WTERMSIG(stat);
		printf("terminated by signal %d\n", exitStatus);
		fflush(stdout);
	}
}

/**********************
 * Function will look check to see if inFileName and/or outFileName
 * exist. If one or both exists, we will subtract the argCount. Ex:
 * "ls -la > junk", argCount will be 4. We will make argCount 2 and
 * get rid of "> junk" in the arg list
 **********************/
void cullArgs(char* inFileName, char* outFileName, char** args, int* argCount, bool hasAmp){
	char* stop;

	stop = malloc(2*sizeof(char));
	stop[0] = '>';
	stop[1] = '\0';

	// If there's an ampersand, cull it from the arguments
	if(hasAmp){
		args[*argCount-1] = '\0';
		*argCount -= 1;
	}

	// If there is no out or in file name, we're not doing redirection,
	// so change nothing
	if(strcmp(inFileName, "") == 0 && strcmp(outFileName, "") == 0){
		free(stop);
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
 * This function is the main function that causes child processes to run.
 * We pass in the command (cmd), arguments for the cmd (args), any INPUT file name
 * passed in by the user, any OUTPUT file name passed in by the user, the flag (hasAmp) that says
 * if the user wants this process to run in the background, the array of childPids, and the cound
 * of childPids.
 ***************************/
void forkAndExec(char* cmd, char** args, int argc, char* inFileName, char* outFileName, int* childExitStatus, bool hasAmp, int** childPids, int* childCount){
	// Signal vars
	struct sigaction default_action = {0};		// Init struct to be empty for CTRL+C (SIGINT)

	// File vars	
	int targetFD, sourceFD;
	int result;
	int exitStatus;

	// Other vars
	pid_t spawnPid = -5;
	pid_t actualPid = -5;
	spawnPid = fork();

	switch(spawnPid){
		case -1: 
			perror("Spawning fork went wrong!\n");
			exit(1);
			break;
		case 0:			// Child
			// If this process is a FG process, unblock CTRL+C (SIGINT) functionality
			if(!hasAmp){	
				// Unblock CTRL+c (SIGINT) signal that we blocked in the parent
				default_action.sa_handler = SIG_DFL;
				default_action.sa_flags = SA_RESTART;
				
				sigaction(SIGINT, &default_action, NULL);	// Default action for  SIGINT
			}

			// If an output file name exists
			if(strcmp(outFileName, "")){
				targetFD = open(outFileName, O_WRONLY | O_CREAT | O_TRUNC, 0644);
				
				// Checking output file for validity
				if(targetFD < 0){
					printf("cannot open %s for output\n", outFileName);
					return;
				}
			
				fcntl(targetFD, F_SETFD, FD_CLOEXEC);	// Close on exec
				result = dup2(targetFD, 1);
			}
			else if(hasAmp){
				// If this is a bg process and no output file, redirect stdout to /dev/null
				targetFD = open("/dev/null", O_WRONLY);
				fcntl(targetFD, F_SETFD, FD_CLOEXEC);	// Close on exec
				dup2(targetFD, 1);
			}

			// If an input file name exists
			if(strcmp(inFileName, "")){
				sourceFD = open(inFileName, O_RDONLY);	
				if(sourceFD < 0){
					printf("cannot open %s for input\n", inFileName);
					return;
				}

				fcntl(sourceFD, F_SETFD, FD_CLOEXEC);	// Close on exec
				result = dup2(sourceFD, 0);
			}
			else if(hasAmp){
				// If this is a bg process and no input file, redirect stdin from /dev/null
				sourceFD = open("/dev/null", O_RDONLY);
				fcntl(sourceFD, F_SETFD, FD_CLOEXEC);	// Close on exec
				dup2(sourceFD, 0);
			}
			
			// Exec
			if(execvp(*args, args) < 0){
				printf("%s: ", args[0]);
				fflush(stdout);
				perror("");
				exit(1);	// Exit process if there was an error
			}
			break;
		default:		// Parent
			// If a FG process, wait for proc to finish
			if(!hasAmp){
				actualPid = waitpid(spawnPid, childExitStatus, 0);	// Wait for child to finish before moving on

				// Check exit status to see if it was terminated by a signal
				if (WIFSIGNALED(*childExitStatus)){
					exitStatus = WTERMSIG(*childExitStatus);
					printf("terminated by signal %d\n", exitStatus );
				}
			}
			else{
				// BG Process
				// Add pid of child to childPids array of a bg process
				addPid(*childPids, spawnPid, childCount);	
				printf("background pid is %d\n", spawnPid);
				fflush(stdout);
			}
				
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
void exitProg(char* cmd, char** args, char* userIn, char* expStr, char* inFileName, char* outFileName, int* childPids){
	free(cmd);
	free(args);
	free(userIn);
	free(expStr);
	free(inFileName);
	free(outFileName);
	free(childPids);
	exit(0);
}
