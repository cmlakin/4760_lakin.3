#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <time.h>
#include "config.h"


//const char * perror_arg0 = "license"; // pointer to return error value

int signalled = 0;  // tells if we got a signal(Ctrl-C or alarm)
char perror_buf[50];  // buffer for perror

static int shmid = -1;  // shared memory identifier
static char shm_keyname[PATH_MAX];  // shared memory key path linux/limits.h ex: 4096
static struct shared_data * shdata = NULL;  // shared memory pointer

static int msgid = -1; // message queue identifier
static pid_t qpid = 0; // queue id
enum msgtypes {LOCK = 1, UNLOCK = 2}; // type/status of msg

struct msgbuf {
	// add msg queue  stuff here
	long mtype;
	pid_t sender;
};

// ** Set up msg queue signal handler

// get size of message
static const size_t msg_size = sizeof(pid_t);

// set up a queue manager to check msg type and control crit sec

// Take access to the critical section
static void msg_get() {

	struct msgbuf mb;

	mb.mtype = LOCK;
	mb.sender = getpid(); // this will be msg from us to queue

	// check if critical section  available
	if(msgsnd(msgid, &mb, msg_size, 0) == -1){

		snprintf(perror_buf, sizeof(perror_buf), "%s: msgsnd: ", perror_arg0);
		perror(perror_buf);
		return;
	}

	// receive msg sent (wait)
  	if(msgrcv(msgid, &mb, msg_size, getpid(), 0) == -1){

		snprintf(perror_buf, sizeof(perror_buf), "%s: msgrcv: ", perror_arg0);	
		perror(perror_buf);
		return;
		
	}
}


// get out of the critical section 
static void msg_release() {

	struct msgbuf mb;

	mb.mtype = UNLOCK;
	mb.sender = getpid(); //from us

	//send a message asking to enter critical section
	if(msgsnd(msgid, &mb, msg_size, 0) == -1){
	
		snprintf(perror_buf, sizeof(perror_buf), "%s: msgsnd: ", perror_arg0);
		perror(perror_buf);
		return;
		
	}						  
}

int getlicense(void) {

	// lock the critical section
	msg_get();
	
	// loop until we get a license (blocking function)
	while(shdata->numlicenses <= 0){

	    // release the critical section, so others can access it
	    msg_release();
	
	    if(signalled){
	    	
			return -1;
	    }
	
	    // lock it again, before we try to get a license
	    msg_get();
	}

	// reduce the number of licenses with one
	int license_number = --shdata->numlicenses;
	
	printf("Took license %d\n", license_number);
	
	msg_release();	

	return license_number;
	
}


int returnlicense(void) {
	  
	msg_release();
	    
	// return one license
	shdata->numlicenses++;
	
	//printf("PROCESS[%d]: Returned license\n", id);
	
	msg_release();
	
	return 0;	
}


// ** change to create initial message queue
int initlicense(void){
	   return 0;
}

int addtolicense(int n) {

	msg_get();
	shdata->numlicenses += n;
	
	//printf("PROCESS[%d]: Added %d licenses\n", id, n);

	msg_release();

	return 0;
}


int removelicenses(int n) {

	msg_get();
	shdata->numlicenses -= n;
	msg_release();
	
	//printf("PROCESS[%d]: Removed %d licenses\n", id, n);
	
	return 0;
}


void logmsg(const char* sbuf) {

	//printf("PROCESS[%d]: LOG:  %s", id, sbuf);

	// lock critical section, because file write has to be synchronized
	msg_get();
		
	
	// open the file in write only and append mode (to write at end of file)
	const int log_fd = open(LOG_FILENAME, O_WRONLY | O_APPEND);
	
	if (log_fd == -1) { //if open failed
	
		// show the error
		snprintf(perror_buf, sizeof(perror_buf), "%s: open: ", perror_arg0);	
		perror(perror_buf);
	
	}
	else {
		
		// save the string buffer to file
		write(log_fd, (void*) sbuf, strlen(sbuf));
		
		close(log_fd);
	}

	// unlock the file	
	msg_release();	
}

// Create/attach the shared memory 
int init_shared_data(const int n){

	int flags = 0;

	// create shared memory key name
	snprintf(shm_keyname, PATH_MAX, "/tmp/license.%u", getuid());
		
	
	if (n) {  // if we have to create
			
	
		// create the logfile
		int fd = creat(LOG_FILENAME, 0700);
	   
		if (fd == -1) {
			    	
			snprintf(perror_buf, sizeof(perror_buf), "%s: creat: ", perror_arg0);       
			perror(perror_buf);
		        
			return -1;    
		}

		close(fd);
		//printf("PROCESS[%d]: Created log %s\n", id, LOG_FILENAME);

		// create the shared memory file
		fd = creat(shm_keyname, 0700);
			
		if (fd == -1) {
					
			snprintf(perror_buf, sizeof(perror_buf), "%s: creat: ", perror_arg0);
			perror(perror_buf);
				
			return -1;
			
		}
			
		close(fd);
		//printf("Created license %s\n", shm_keyname);
					
		// set flags for the shared memory creation
		flags = IPC_CREAT | IPC_EXCL | S_IRWXU;
	}

	// make a key for our shared memory
	key_t fkey = ftok(shm_keyname, 1234);
	
	if (fkey == -1) { // if ftok failed
		
		snprintf(perror_buf, sizeof(perror_buf), "%s: ftok: ", perror_arg0);
	    perror(perror_buf);
	    
		return -1;  // return error
	}

	// get a shared memory region
	shmid = shmget(fkey, sizeof(struct shared_data), flags);
	
	if (shmid == -1) {  // if shmget failed
		
		snprintf(perror_buf, sizeof(perror_buf), "%s: shmget: ", perror_arg0);
		perror(perror_buf);    
		
		return -1;
	}

	// attach the region to process memory
	
	shdata = (struct shared_data*) shmat(shmid, NULL, 0);
	
	if (shdata == (void*)-1) {  //i f attach failed
	
		snprintf(perror_buf, sizeof(perror_buf), "%s: shmat: ", perror_arg0);    
		perror(perror_buf);
	    
		return -1;
	}

	if (n) {  //if we created a region

		//clear all of the shared data
		bzero(shdata, sizeof(struct shared_data));
		
		//initialize it
		initlicense();
		addtolicense(n);	
	}
			
	return 0;	
}

// Detach/destroy the shared memory region
int deinit_shared_data(const int n){  // n > 0 only in runsim. testsim uses 0
	
	// detach the shared memory pointer
    if (shmdt(shdata) == -1) {
    
		snprintf(perror_buf, sizeof(perror_buf), "%s: shmdt: ", perror_arg0);
		perror(perror_buf);
        
		return -1;    
	}
  
	if (n > 0) { // if its runsim
    
		// remove the region from system        
		shmctl(shmid, IPC_RMID, NULL);
 
		// delete the shared memory file
		if (unlink(shm_keyname) == -1) {
        	
			snprintf(perror_buf, sizeof(perror_buf), "%s: unlink: ", perror_arg0);
			perror(perror_buf);
            
			return -1;            
		}

		//printf("PROCESS[%d]: Removed license %s\n", id, shm_keyname);        
	}

	return 0;    
}

// join a timestamp and a message to buffer
void put_timestamp(char * buf, const int buf_size, const char * msg) {

  	char stamp[30];
    time_t t = time(NULL);
    struct tm * tm = localtime(&t);
    
	strftime(stamp, sizeof(stamp), "%Y-%M-%d %H:%m:%S", tm);
	snprintf(buf, buf_size, "%s %u %s\n", stamp, getpid(), msg);    
}




















