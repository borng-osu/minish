/*
* Author: Garrett Born
* Date completed: 05/03/2021
* Description: Miniature bash shell program that handles most typical shell commands.
*/

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>

// Constants for max amount of characters for command inputs and distinct arguments accepted
#define MAX_INPUT 2048
#define MAX_ARG 256

// Global variables that handle most statuses
int curStatus = 0, cmdCount = 0;			// curStatus - exit status for child proc, cmdCount - # of cmds
int bgProcess, outputFile, inputFile;		// bgProcess - bool for if proc bg or not, inputFile & outputFile - bools for input and output redirects
char expander[MAX_INPUT];					// expander - temp string for variable expansion
int bgOn = 1;								// bgOn - bool for if bg procs alowed

// Initializes sigaction structs for SIGINT and SIGTSTP
struct sigaction SIGINT_action = {{ 0 }};
struct sigaction SIGTSTP_action = {{ 0 }};

// Handles when ^Z is received, which signals whether bg procs are allowed or not 
void handle_SIGTSTP(int signo) {
	char* message;

	// If bg procs allowed, will switch to foreground-only mode, disallowing bg procs
	if (bgOn == 1) {
		message = "Entering foreground-only mode ('&' ignored)\n";
		write(STDOUT_FILENO, message, 44);
		fflush(stdout);
		bgOn = 0;
	}

	// If bg proces disallowed, will exit foreground-only mode, allowing bg procs
	else {
		message = "Exiting foreground-only mode\n";
		write(STDOUT_FILENO, message, 29);
		fflush(stdout);
		bgOn = 1;
	}

	return;
}

// Takes statusMethod int and prints whether its linked child process exited or terminated and with what value
void exitStatus(int statusMethod) {
	// If proc exited, will print the value with which it exited
	if (WIFEXITED(statusMethod)) {
		printf("Exited with value: %d\n", WEXITSTATUS(statusMethod));
	}

	// If proc did not exit, will print the value with which it was terminated by a signal
	else {
		printf("Terminated by signal: %d\n", WTERMSIG(statusMethod));
	}
	fflush(stdout);
}

// Takes cmds string and args array of strings and fills them with user input to be run as cmds in shell
void getInput(char* cmds, char* args[]) {
	char* token;

	// Prints command prompt and gets input from user
	printf(": ");
	fflush(stdout);
	fgets(cmds, MAX_INPUT, stdin);

	// Removes the new line character from user input in cmds string
	strtok(cmds, "\n");

	// Sets initial token for first arg of user input
	token = strtok(cmds, " ");
	// Initializes loop counters
	int i = 0;
	int j, diff;
	// Initializes the string to hold the current PID for variable expansion
	char curPID[100];
	sprintf(curPID, "%d", getpid());

	// Loops through user input to parse the user's cmds properly to be run
	while (token != NULL) {
		// If user input contains '<', sets the inputFile flag and proceeds token
		if (strcmp(token, "<") == 0) {
			inputFile = i;
			token = strtok(NULL, " ");
		}
		// If user input contains '>', sets the outputFile flag and proceeds token
		else if (strcmp(token, ">") == 0) {
			outputFile = i;
			token = strtok(NULL, " ");
		}

		// Sets current element in args to token
		args[i] = token;
		
		// Initializes counters for variable expansion
		diff = 2;
		j = 0;
		// Iterates through recently added string, checking for '$$' for variable expansion
		while (j < strlen(args[i])) {
			// If '$$' found, converts it to curPID
			if (args[i][j] == '$' && args[i][j + 1] == '$') {
				strncpy(expander, args[i], j);
				expander[j] = '\0';
				strcat(expander, curPID);
				strcat(expander, args[i] + j + diff);
				args[i] = expander;
				j = j + strlen(curPID);
				diff = strlen(curPID);
				continue;
			}
			j++;
		}

		// Gets next token
		token = strtok(NULL, " ");
		i++;
	}

	// Sets final element to NULL to estabish end point
	args[i] = NULL;
	// Initiaizes the # of cmds
	cmdCount = i - 1;
}

// Takes args array of strings and runs the corresponding cmd program as a child proc if built-in programs not relevant
void runCmd(char* args[]) {
	// Initializes PID for child proc
	pid_t pid = fork();
	// Initializes input & output int to handle input/output redirection
	int input, output;

	// Checks if '&' as last arg and sets bgProcess flag to run proc in background
	if (strcmp(args[cmdCount], "&") == 0) {
		bgProcess = 1;
		args[cmdCount] = NULL;
	}

	// Handles child and parent proc redirection, switch structure based off Module 3
	switch (pid) {
		// Handles error
		case -1:
			perror("Hull breached!\n");
			exit(1);
			break;

		// Handles child proc to run program
		case 0:
			// Redirects input if inputFile flag set
			if (inputFile != 0) {
				// Prints error message if unable to open file set as input and exits with 1
				if ((input = open(args[inputFile], O_RDONLY)) < 0) {
					fprintf(stderr, "cannot open %s as input file\n", args[inputFile]);
					fflush(stdout);
					exit(1);
				}
				// Sets input to stdin for child proc, removes it from args array, and closes it
				dup2(input, 0);
				args[inputFile] = NULL;
				close(input);
			}

			// Redirects output if outputFile flag set
			if (outputFile != 0) {
				// Prints error message if unable to open file set as output and exits with 1
				if ((output = open(args[outputFile], O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) {
					fprintf(stderr, "cannot open %s as output file\n", args[outputFile]);
					fflush(stdout);
					exit(1);
				}
				// Sets output to stdout for child proc, removes it from args array, and closes it
				dup2(output, 1);
				args[outputFile] = NULL;
				close(output);
			}

			// Resets inputFile and outputFile flags
			inputFile = 0;
			outputFile = 0;

			// Allows ^C signals to be sent if child proc run in foreground
			if (bgProcess == 0) {
				SIGINT_action.sa_handler = SIG_DFL;
				sigaction(SIGINT, &SIGINT_action, NULL);
			}

			// Runs cmd as program via execvp
			// If error raised, prints that the cmd is not valid and exits with 1
			if (execvp(args[0], args) == -1) {
				printf("%s: not valid file or directory\n", args[0]);
				fflush(stdout);
				exit(1);
			};
			break;

		// Handles parent proc
		default:
			// If bgProcess flag set and bgOn set, runs child proc in background
			if (bgProcess != 0 && bgOn == 1) {
				waitpid(pid, &curStatus, WNOHANG);
				// Prints child proc's PID
				printf("Background process ID is %d\n", pid);
				fflush(stdout);
			}
			// Else runs it in foreground and waits for its termination
			else {
				waitpid(pid, &curStatus, 0);
			}

			// Checks for background process to terminates before printing that it terminated and its exit status
			while ((pid = waitpid(-1, &curStatus, WNOHANG)) > 0) {
				printf("Background process %d is done: ", pid);
				fflush(stdout);
				exitStatus(curStatus);
			}

			// Resets bgProces flag
			bgProcess = 0;
			break;

	}
	return;
}

// Takes args string array and checks to see if it should be ignored, run by a build-in cmd, or run via runCmd func
void handleInput(char* args[]) {

	// Checks if input is a comment or blank so that it is ignored
	if (args[0][0] == '#' || args[0][0] == '\n') {
		return;
	}
	// Checks if input is an exit cmd in order to exit shell
	else if (strcmp(args[0], "exit") == 0) {
		exit(0);
	}
	// Checks if input is 'cd' and changes directory accordingly
	else if (strcmp(args[0], "cd") == 0) {
		// If there is a directory/path arg, runs chdir with it
		if (args[1]) {
			// Runs chdir with args[1] and prompts user if error encountered
			if (chdir(args[1]) == -1) {
				printf("Directory %s not found.\n", args[1]);
				fflush(stdout);
			}
		}
		// If not directory/path arg, runs chdir on HOME directory
		else {
			chdir(getenv("HOME"));
		}
	}
	// Checks if input is "status" and runs exitStatus func
	else if (strcmp(args[0], "status") == 0) {
		exitStatus(curStatus);
	}
	// If no built-in cmds fit, runs args via runCmd func
	else {
		runCmd(args);
	}
}




/*
* Runs main process of shell using funcs defined above
*/
int main() {
	
	// Sets SIGINT to be ignored for parent procs
	SIGINT_action.sa_handler = SIG_IGN;
	sigfillset(&SIGINT_action.sa_mask);
	SIGINT_action.sa_flags = 0;
	sigaction(SIGINT, &SIGINT_action, NULL);

	// Sets SIGTSTP to run handle_SIGTSTP func
	SIGTSTP_action.sa_handler = handle_SIGTSTP;
	sigfillset(&SIGTSTP_action.sa_mask);
	SIGTSTP_action.sa_flags = 0;
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);

	// Sets cmdInput string for user input and argCount string array to break input into separate args
	char cmdInput[MAX_INPUT];
	char* argCount[512];

	// Runs shell until "exit" cmd input
	while (1) {
		// Runs getInput func with cmdInput and argCount
		getInput(cmdInput, argCount);
		// Runs handleInput func with filled argCount
		handleInput(argCount);
		// Resets inputFile and outputFile flags if not reset by above funcs
		inputFile = 0;
		outputFile = 0;
	}

	return 0;

}