/*#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <time.h>*/
#include "config.h"


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

// msg queue signal handler
static void queue_sig_handler(int sig) {
	printf("QUEUE: Signal %d received\n", sig);
	if (sig == SIGINT) {
		signalled = 1;
	}
}

// get size of message
static const size_t msg_size = sizeof(pid_t);

// set up a queue manager to check msg type and control crit sec
static void queue_manager(void) {

	struct msgbuf mb;
	pid_t holder = 0;

	struct sigaction sa;
  	sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);
	
	sa.sa_handler = queue_sig_handler;

	
	if( (sigaction(SIGTERM, &sa, NULL) == -1) 
		|| (sigaction(SIGALRM, &sa, NULL) == -1) 
		|| (sigaction(SIGINT, &sa, NULL) == -1)) {

			perror("sigaction");
			return; // ask again why I'm not supposed to put anything here ???
	}

	while(signalled == 0){
		
		if(holder == 0){
			
			//wait for request, send to us(we use our PID as type)
			if(msgrcv(msgid, &mb, msg_size, LOCK, 0) == -1){
	        	
				if(errno != EINTR){
					snprintf(perror_buf, sizeof(perror_buf), "%s: msgrcv: ", perror_arg0);
					perror(perror_buf);
					break;
				}
				continue;
			}

			holder = mb.sender; //holder of the lock, is process who sent message
			//printf("QUEUE[%d]: %d LOCKED!\n", getpid(), holder);
			
			//send reply
			mb.mtype = holder; // msg to holder
			mb.sender = getpid(); // from us

			if(msgsnd(msgid, &mb, msg_size, 0) == -1){
				
				snprintf(perror_buf, sizeof(perror_buf), "%s: 1msgsnd: ", perror_arg0);
				perror(perror_buf);
				break;
			}
		}

		//wait for the unlock
		if(msgrcv(msgid, &mb, msg_size, UNLOCK, 0) == -1){
			
			if(errno != EINTR){
				
				snprintf(perror_buf, sizeof(perror_buf), "%s: msgrcv: ", perror_arg0);
				perror(perror_buf);
				break;
			}
			
			continue;
		}

		// if not us
		if (holder != mb.sender)  {
			printf("QUEUE[%d]: Unlocked, but not by holder\n", getpid());
		}

		holder = 0;
	}

	exit(0);
}

// Take access to the critical section
static void msg_get() {

	struct msgbuf mb;

	mb.mtype = LOCK;
	mb.sender = getpid(); // this will be msg from us

	// check if critical section  available - send msg to request
	if (msgsnd(msgid, &mb, msg_size, 0) == -1){

		snprintf(perror_buf, sizeof(perror_buf), "%s: 2Msgsnd: ", perror_arg0);
		perror(perror_buf);
		//return;
	}

	// receive msg sent
  	if (msgrcv(msgid, &mb, msg_size, getpid(), 0) == -1){

		snprintf(perror_buf, sizeof(perror_buf), "%s: msgrcv: ", perror_arg0);	
		perror(perror_buf);
		//return;
		
	}
}


// get out of the critical section 
static void msg_release() {

	struct msgbuf mb;

	mb.mtype = UNLOCK;
	mb.sender = getpid(); //from us

	//send a message asking to enter critical section
	if (msgsnd(msgid, &mb, msg_size, 0) == -1){
	
		snprintf(perror_buf, sizeof(perror_buf), "%s: 3msgsnd: ", perror_arg0);
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

	// reduce the number of licenses 
	int license_number = --shdata->numlicenses;
	
	printf("PROCESS[%d]: Took license %d\n", getpid(), license_number);
	
	msg_release();	

	return license_number;
}

int returnlicense(void) {
	  
	msg_get();
	    
	// return one license
	shdata->numlicenses++;
	printf("PROCESS[%d]: Returned license\n", getpid());
	msg_release();
	
	return 0;	
}


// create initial message queue
int initlicense(void){

	int flags = 0;

	// create shared memory key name
	snprintf(shm_keyname, PATH_MAX, "/tmp/license.%u", getuid());

	// check that runsim is making the call to create sm & queue
	const int is_runsim = (strcmp("runsim", perror_arg0) == 0) ? 1 : 0;

	// if not already created, then create
	if (is_runsim) {
		
		// create a logfile
		int fd = creat(LOG_FILENAME, 0700);
		if (fd == -1) {
			snprintf(perror_buf, sizeof(perror_buf), "%s: creat: ", perror_arg0);
			perror(perror_buf);
			return -1;
		}
		close(fd);

		printf("PROCESS[%d]: Created license %s\n", getpid(), shm_keyname);

		// create the shared memory file
		fd = creat(shm_keyname, 0700);
		if (fd == -1) {
			snprintf(perror_buf, sizeof(perror_buf), "%s: creat: ", perror_arg0);
			perror(perror_buf);
			return -1;
		}
		close(fd);
		
		printf("PROCESS[%d]: Created license %s\n", getpid(), shm_keyname);

		// set the flags for shared memory
		flags = IPC_CREAT | IPC_EXCL | S_IRWXU;
	}

	// make key for shared memory
	key_t fkey = ftok(shm_keyname, 1347);
	if (fkey == -1) {
		
		snprintf(perror_buf, sizeof(perror_buf), "%s: ftok: ", perror_arg0);
		perror(perror_buf);	
		return -1;  //return error
		
	}

	// get shared memory
	shmid = shmget(fkey, sizeof(struct shared_data), flags);
	
	if(shmid == -1){  //if shmget failed
		
		snprintf(perror_buf, sizeof(perror_buf), "%s: shmget: ", perror_arg0);
		perror(perror_buf);
		return -1;
	}

	fkey = ftok(shm_keyname, 6671);

	if (fkey == -1) {
		
		snprintf(perror_buf, sizeof(perror_buf), "%s: ftok: ", perror_arg0);
		perror(perror_buf);	
		return -1;  //return error
		
	}

	// ** add shared memory for msgid
	msgid = msgget(fkey, flags);
	if(shmid == -1){  //if shmget failed
		
		snprintf(perror_buf, sizeof(perror_buf), "%s: msgget: ", perror_arg0);
		perror(perror_buf);
		return -1;
	}
	
	// ** attach to shared memory
	shdata = (struct shared_data*) shmat(shmid, NULL, 0);
	// if attach failed
	if (shdata == (void*) -1) {
	
		snprintf(perror_buf, sizeof(perror_buf), "%s: shmat: ", perror_arg0);
		perror(perror_buf);
		return -1;
	}
	
	// add section to clear if we created a region 
	if (is_runsim) {

		// clear the shared data
		bzero(shdata, sizeof(struct shared_data));

		// create a process to handle the queue communication
		qpid = fork();
		if (qpid == -1) {
			return -1;
		}
		else if (qpid == 0) {
			queue_manager();
			exit(0);
		}
		else {
			printf("RUNSIM[%d]:  Started queue manager with PID %d\n", getpid(), qpid);
		}
	}
	return 0;
}

int addtolicenses(int n) {

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

// ** TODO - Not working right now. Check again after getting more set up ** //
void logmsg(const char* sbuf) {

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
		
		printf("PROCESS[%d]: LOG:  %s", getpid(), sbuf);
		// save the string buffer to file
		write(log_fd, (void*) sbuf, strlen(sbuf));
		
		close(log_fd);
	}

	// unlock the file	
	msg_release();	
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
    
		// stop queue management
		kill(qpid, SIGINT);
		waitpid(qpid, NULL, 0);
		
		// remove the region from system        
		shmctl(shmid, IPC_RMID, NULL);
 		msgctl(msgid, IPC_RMID, NULL);

		// delete the shared memory file
		if (unlink(shm_keyname) == -1) {
        	
			snprintf(perror_buf, sizeof(perror_buf), "%s: unlink: ", perror_arg0);
			perror(perror_buf);
            
			return -1;            
		}

		printf("PROCESS[%d]: Removed license %s\n", getpid(), shm_keyname);        
	}

	return 0;    
}

// join a timestamp and a message to buffer
void put_timestamp(char * buf, const int buf_size, const char * msg) {

  	char stamp[30];
    time_t t = time(NULL);
    struct tm * tm = localtime(&t);
    
	strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", tm);
	snprintf(buf, buf_size, "%s %u %s\n", stamp, getpid(), msg);    
}




















