#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "config.h"
#include <sys/wait.h>
#include <stdbool.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/stat.h>


//#define BUF_SIZE 1024
//#define SHM_KEY 0x6893

const char * perror_arg0 = "runsim"; // pointer to return error value

// modified example from book
// purpose: to splite a line into pointer array to use for exec
//          and add process id to arguments  
static int makeargv(const char * s, const char * delimiters, char ***argvp) {
	
	int error;
	int i;
	int tlen;
	int numtokens;
	const char * snew;
	char * t;

	
	if ((s == NULL) || (delimiters == NULL) || (argvp == NULL)) {
		errno = EINVAL;
		return -1;
	}

	*argvp = NULL;
	snew = s + strspn(s, delimiters); // snew is a real start of string
	if ((t = malloc(strlen(snew) + 1)) == NULL) {
		return -1;
	}

	*argvp = NULL;
	snew = s + strspn(s, delimiters);  // real start of string
	tlen = strlen(snew) + 1 + 10;
	
	if ((t = malloc(tlen)) == NULL) {
		return -1;
	}
	//strcpy(t, snew);
	// add process id to tokens
	snprintf(t, tlen, "%s %i", snew, id);
	numtokens = 0;
	if (strtok(t, delimiters) != NULL) 
		//count the number of tokens in s
		for (numtokens = 1; strtok(NULL, delimiters) != NULL; numtokens++);
			// create argument array for parts to the tokens
			if ((*argvp = malloc((numtokens + 1)*sizeof(char *))) ==NULL) {
				error = errno;
				free(t);
				errno = error;
				return -1;
			}
	
	// insert pointers to tokens into the argument array
	if (numtokens == 0)
		free(t);
	else {
		//strcpy(t, snew);
		snprintf(t, tlen, "%s %i", snew, id); // add process id to tokens
		**argvp = strtok(t, delimiters);
		for (i =1; i < numtokens; i++) 
			*((*argvp) + i) =  strtok(NULL, delimiters);
	}
	*((*argvp) + numtokens) =NULL; //put in final null pointer
	return numtokens;
}

static void docommand (const char * stdinline) {

	// for a child
	const pid_t pid = fork(); // fork a process
	// if its the child
	if (pid == 0) { 
		printf("Grandchild: %d started\n", getpid());

		char ** args = NULL;

		// Grandchild makes its arguments
		if (makeargv(stdinline, " \t", &args) == - 1) {
			snprintf(perror_buf, sizeof(perror_buf), "%s: makeargv: ", perror_arg0);
			perror(perror_buf);
			exit(-1); // exit with error code
		}

		// execute what is in args[0]
		execv(args[0], args);
		// if execv fails
		snprintf(perror_buf, sizeof(perror_buf), "%s: execvp: ", perror_arg0);
		perror(perror_buf);
		exit(-1); // exit with error code
	}
	// if its the parent
	else {

		int status;
		
		// wait for Grandchild to finish
		waitpid(pid, &status, 0);
		printf("Child[%d]: grandchild %d exited\n", getpid(), pid);
		// when finished return license
		returnlicense();
		printf("Child[%d]: license returned\n", getpid());
	}
}

// function for handling signals
static void signal_handler(const int sig) {

	sigset_t mask;
	sigset_t oldmask;

	sigfillset(&mask);

	// block all signals
	sigprocmask(SIG_SETMASK, &mask, &oldmask);

	// if its Ctrl-C
	if (sig == SIGINT) {

		signalled = 1;
		printf("Process[%d]: Ctrl-C entered\n", id);
	}
	// alarm signal
	else if (sig == SIGALRM) {
		// send to all processes
		kill(0, SIGTERM);
	}

	// unblock all signals
	sigprocmask(SIG_SETMASK, &oldmask, NULL);
}


// initialize signalling
static int init_signalling() {

	struct sigaction sa;

	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = SIG_IGN; // ignore the next signals

	// send to children (this is why we have to ignore)
	if (sigaction(SIGALRM, &sa, NULL) == -1) {

		perror("sigaction");
		return -1;
	}

	// handle Ctrl-C
	sa.sa_handler = signal_handler;
	if ((sigaction(SIGALRM, &sa, NULL) == -1) || (sigaction(SIGINT, &sa, NULL) == -1)) {

		perror("sigaction");
		return -1;
	}

	// alarm - runtime limit
	alarm(ALARM_TIME);
	return 0;
}

int main(int argc, char *argv[]){

	char buf[MAX_CANON]; // line buffer
	int numchildren = 0; // number of active children
	int status; // waitpid status
	pid_t pid;

	if (argc != 2) {
		
		fprintf(stderr, "Error: Missing number of children\n");
		fprintf(stderr, "Usage: ./runsim <n>\n");

		return -1;
	}

	const int n = atoi(argv[1]); // get n value from command line

	// initialize signals and shared memory
	if ((init_signalling() < 0) || (init_shared_data(n) < 0)) {

		return EXIT_FAILURE;

	}

	// read from stdin
	while (fgets(buf, MAX_CANON, stdin) != NULL) {

		getlicense(); // get a license
		//printf("Runsim[%d]: got 1 license\n", getpid());

		// get id for child process
		const int assigned_id = assign_id();

		pid = fork(); // fork a child

		if (pid == 0) {

			id = assigned_id; // new process get a unique id

			fclose(stdin); // child doesn't need stdin

			//printf("Child: %d started\n", id);
			docommand(buf);
			//printf("Child: %d finished\n", id);
			
			exit(0);
		}
		else {

			++numchildren;

			// check for finished children
			while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {

				//printf("Runsim[%d]: %d child finished\n", getpid(), pid);
				--numchildren; // reduce number of active children
			}
		}
	}

	if (numchildren) {
		
		printf("Runsim[%d]: waiting for %d children\n", getpid(), numchildren);

	}

	// wait for all children to finish
	while((numchildren > 0) && ((pid = waitpid(-1, &status, 0)) >= 0)) {

		//printf("Runsim[%d]: %d child finished\n", getpid(), pid);
		--numchildren;
	}

	// put final message in log file
	put_timestamp(buf, sizeof(buf), "Runsim finished");
	logmsg(buf);

	// destroy shared memory
	deinit_shared_data(n);

	return 0;
}




