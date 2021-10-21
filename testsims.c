/*#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>*/
#include "config.h"

// pointer to return error value
const char * perror_arg0 = "testsim";

int main(int argc, char* argv[]){

	int i;
	char perror_buf[50];
	char logbuf[200];
	char buf2[20];

	int repeats = atoi(argv[1]); // num times to repeat loop
	int seconds = atoi(argv[2]); // num seconds to wait/sleep

	// create the string from the program name
	snprintf(perror_buf, sizeof(perror_buf), "%s: Error: ", "testsim");

	printf("testsim[%d]: Started with %d %d\n", getpid(), repeats, seconds);

	if (initlicense() < 0) {

		return -1;
	}

	for (i = 0; i < repeats; i++) {
	
		sleep(seconds);
		// create message to put in logfile
		snprintf(buf2, sizeof(buf2), "%i %i", i, repeats);
		put_timestamp(logbuf, sizeof(logbuf), buf2);

		logmsg(logbuf);

	}

	deinit_shared_data(0);

	return 0;
}
