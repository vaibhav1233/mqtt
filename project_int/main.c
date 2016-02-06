
#include "mqtt_header.h"

int main(void)
{	
	int ret, i=0;
	struct sigaction s1,s2;
	sigset_t set1,set2;
	pthread_t client_td[CLIENT_NUMBER];	
	pthread_mutex_init(&mtx,NULL);
	pthread_cond_init(&event,NULL);
	pthread_cond_init(&sevent,NULL);
	creat_client(CLIENTID);

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

    MQTTClient_destroy(&client);
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

	fd = open(CLIENT_FILENAME, O_CREAT | O_TRUNC | O_SYNC | O_RDWR, 0666);	

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
		if(client_connect()) {
			pthread_cond_broadcast(&sevent);
			syncFlag = 1;
		}
		else
			printf("Server not connected !\n");
	
		pthread_mutex_unlock(&mtx);
	}
	close(fd);
	pthread_exit(NULL);
}

void *sync_func(void *arg)
{
	int fd_cl;
	char serverfile[16], buf[22];

	fd_cl = open(CLIENT_FILENAME, O_SYNC | O_RDONLY, 0666);	

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
		
		printf("Buffer to publish: %s\n", buf);


		publish(22, buf);
		MQTTClient_disconnect(client, 10000);

		syncFlag = 0;
		pthread_mutex_unlock(&mtx);
	}

	close(fd_cl);
	pthread_exit(NULL);	
}

int creat_client(char *client_id)
{
	int ret;

	ret = MQTTClient_create(&client, ADDRESS, client_id,MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

	return ret;
}

int client_connect(void)
{
    return (MQTTClient_connect(client, &conn_opts) == MQTTCLIENT_SUCCESS);
}

int publish(int payloadLen, char *payload)
{
	int ret;
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;

    pubmsg.payload = payload;
    pubmsg.payloadlen = payloadLen;
    pubmsg.qos = QOS;
    pubmsg.retained = 0;
    MQTTClient_publishMessage(client, TOPIC, &pubmsg, &token);
    printf("Waiting for up to %d seconds for publication of %s\n"
            "on topic %s for client with ClientID: %s\n",
            (int)(TIMEOUT/1000), payload, TOPIC, CLIENTID);
    ret = MQTTClient_waitForCompletion(client, token, TIMEOUT);
    printf("Message with delivery token %d delivered\n", token);

	return ret;
}

