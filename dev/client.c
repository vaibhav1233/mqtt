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
#define CLIENT_FILENAME	"./db-client"
#define SERVER_FILENAME	"./db-server"

void event_handler(int signo);
void *client_func(void *arg);
void *sync_func(void *arg);
int readyFlag = 0;				/* flag to tell the threads to proceed when signaled */
int syncFlag = 0;				/* sync flag to tell the threads to proceed when signaled */
int activeclients = 0;
pthread_cond_t  event;    		/* event to wait on / signal */
pthread_cond_t  sevent;    		/* sync event to wait on / signal */
pthread_mutex_t mtx;     		/* mutex for the above */

int main(void)
{	
	int ret, i=0;
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

	ret = pthread_create(&client_td[0],NULL,client_func,(void *)&i);
	if(ret) {
		printf("error in thread creation1\n");
		return -1;
	}
	i++;
	ret = pthread_create(&client_td[1],NULL,sync_func,(void *)&i);
	if(ret) {
		printf("error in thread creation1\n");
		return -1;
	}

	pthread_join(client_td[0],NULL);
	pthread_join(client_td[1],NULL);

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
	int fd;
	char buf[22];
	int srno=0;

	fd = open(CLIENT_FILENAME, O_CREAT | O_SYNC | O_RDWR, 0666);	

	while(1) {
		pthread_mutex_lock(&mtx);
		if (readyFlag == 0) {
			printf("Client-Thread Waiting.....\n");
			do {
				pthread_cond_wait(&event,&mtx);
			} while (readyFlag == 0);
		}

		printf("Client-Thread Writing requested data to journal\n");
		sprintf(buf, "Door Status Number %2d\n", srno++);

		write(fd, buf, 22);
		readyFlag = 0;

		/* Trigger Sync */
		pthread_cond_broadcast(&sevent);
		syncFlag = 1;
	
		pthread_mutex_unlock(&mtx);
	}
	close(fd);
	pthread_exit(NULL);
}

void *sync_func(void *arg)
{
	int fd_cl, fd_sr;
	char serverfile[16], buf[22];

	fd_cl = open(CLIENT_FILENAME, O_RDONLY, 0666);	
	fd_sr = open(SERVER_FILENAME, O_CREAT | O_RDWR, 0666);	

	while(1) {
		pthread_mutex_lock(&mtx);
		if (syncFlag == 0) {
			printf("SyncThread Waiting.....\n");
			do {
				pthread_cond_wait(&sevent,&mtx);
			} while (syncFlag == 0);
		}

		printf("Sync-Thread Writing requested data to journal\n");
		read(fd_cl, buf, 22);
		write(fd_sr, buf, 22);

		syncFlag = 0;
		pthread_mutex_unlock(&mtx);
	}

	close(fd_cl);
	close(fd_sr);
	pthread_exit(NULL);	
}


