#include<stdio.h>
#include<string.h>
#include<unistd.h>
#include<sys/wait.h>
#include<signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define CLIENT_NUMBER	1
#define CLIENT_FILENAME	"./client_db-"

void event_handler(int signo);
void *client_func(void *arg);
int readyFlag = 0;				/* flag to tell the threads to proceed when signaled */
int activeclients = 0;
pthread_cond_t  event;    		/* event to wait on / signal */
pthread_cond_t  sevent;    		/* sync event to wait on / signal */
pthread_mutex_t mtx;     		/* mutex for the above */

int main(void)
{	
	int ret, i;
	struct sigaction s1,s2;
	sigset_t set1,set2;
	pthread_t client_td[CLIENT_NUMBER];	
	pthread_mutex_init(&mtx,NULL);
	pthread_cond_init(&event,NULL);
	pthread_cond_init(&sevent,NULL);

	/* Init Sigactions */
	memset(&s1,0,sizeof(struct sigaction));
	memset(&s2,0,sizeof(struct sigaction));
	s1.sa_handler = event_handler;
	sigfillset(&set1);
	sigdelset(&set1,SIGUSR1);
	sigaction(SIGUSR1,&s1,&s2);

	for(i=0; i<CLIENT_NUMBER; i++) {
		ret = pthread_create(&client_td[i],NULL,client_func,(void *)&i);
		if(ret) {
			printf("error in thread creation1\n");
			return -1;
		}
		sleep(1);
	}

	for(i=0; i<CLIENT_NUMBER; i++) {
		pthread_join(client_td[i],NULL);
	}

	return 0;
}

void event_handler(int signo)
{
	printf("signal catch %d\n",signo);
	
	pthread_mutex_lock(&mtx);
	readyFlag = 1;
	pthread_cond_broadcast(&event);
	pthread_mutex_unlock(&mtx);
}

void *client_func(void *arg)
{
	int tid=*((int*)arg);
	int fd;
	char dbfile[16];
	char buf[16] = "Hello World\n";
	int srno=0;

	sprintf(dbfile, CLIENT_FILENAME"%d", tid);
	fd = open(dbfile, O_CREAT | O_RDWR, 0777);	

	while(1) {
		pthread_mutex_lock(&mtx);
		if (readyFlag == 0) {
			printf("Thread-%d Waiting.....\n", tid);
			do {
				pthread_cond_wait(&event,&mtx);
			} while (readyFlag == 0);
		}
		pthread_mutex_unlock(&mtx);

		printf("Thread-%d Writing requested data to journal\n", tid);
		sprintf(buf, "Door Status Number %2d\n", srno++);
		write(fd, buf, 22);

		pthread_mutex_lock(&mtx);
		readyFlag = 0;
		pthread_mutex_unlock(&mtx);
	}
	close(fd);
	pthread_exit(NULL);
}



