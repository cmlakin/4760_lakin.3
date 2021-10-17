#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#define PROCESSES 18
#define ALARM_TIME 100
#define LOG_FILENAME "license.log"


// shared memory structure
struct shared_data {

	int numlicenses; // number of available licenses
};

//extern int id; // process id
extern int signalled; // variable for signalling
extern char perror_buf[50]; // error buffer
extern const char * perror_arg0; // pointer to return error value

// put a time stamp on messages
void put_timestamp(char * buf, const int buf_size, const char * msg);

// functions for shared memory
int init_shared_data(const int n);
int deinit_shared_data(const int n);

// blocks until a license is available
int getlicense(void);

// increments the number of available licenses
int returnlicense(void);

// performs any needed initialization of the license object
int initlicense(void);

// adds n licenses to the number available
int addtolicenses(int n);

// decrements the number of licenses by n
int removelicenses(int n);

// write the specified message to the log file. 
// There is only one log file
// This function will treat the logfile as a critical resource
// It will open the file to append the message and close the file
// 	after appending the message
void logmsg(const char * msg);


