#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "MQTTClient.h"

#define ADDRESS     "tcp://m2m.eclipse.org:1883"
#define CLIENTID    "etron"
#define TOPIC       "Temp"
#define PAYLOAD     "Hello World!"
#define QOS         1
#define TIMEOUT     10000L

/*  database */
#define CLIENT_NUMBER	1
#define CLIENT_FILENAME	"./db-client"

void event_handler(int signo);
void *client_func(void *arg);
void *sync_func(void *arg);
int readyFlag = 0;				/* flag to tell the threads to proceed when signaled */
int syncFlag = 0;				/* sync flag to tell the threads to proceed when signaled */
int activeclients = 0;
pthread_cond_t  event;    		/* event to wait on / signal */
pthread_cond_t  sevent;    		/* sync event to wait on / signal */
pthread_mutex_t mtx;     		/* mutex for the above */
MQTTClient client;
MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;

int publish(int payloadLen, char *payload);
int client_connect(void);

