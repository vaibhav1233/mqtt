
#include "mqtt_header.h"

#define	MSG_LEN	22

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
	int db_fd, events_fd;
	char buf[MSG_LEN], events_buf[32];
	int srno=0;

	while(1) {
		pthread_mutex_lock(&mtx);
		if (readyFlag == 0) {
			printf("Client-Thread Waiting.....\n");
			do {
				pthread_cond_wait(&event,&mtx);
			} while (readyFlag == 0);
		}

		sprintf(buf, "Door Status Number %2d\n", srno++);
	
		/* Saving a local copy of the data */
		db_fd = open(CLIENT_FILENAME, O_CREAT | O_APPEND | O_SYNC | O_WRONLY, 0666);	
		write(db_fd, buf, MSG_LEN);
		close(db_fd);

		/* Saving number of events to disk */
		events_fd = open("TOTAL_EVENTS", O_CREAT | O_SYNC | O_WRONLY, 0666);
		sprintf(events_buf, "%d", srno);
		write(events_fd, events_buf, strlen(events_buf));
		close(events_fd);

		readyFlag = 0;
		printf("Client-Thread Wrote requested data to journal (events_buf:%s)\n", events_buf);
		pthread_mutex_unlock(&mtx);
	}
	pthread_exit(NULL);
}

/* Get total events (synced/total)
 * total = 1
 * synced = 2
 */
int current_events(int events)
{
	int fd, ret;
	char filename[32], buf[32]= "\n";
	if(events==1)
			fd = open("TOTAL_EVENTS", O_SYNC | O_RDONLY, 0666);
	else if(events==2)
			fd = open("SYNCED_EVENTS", O_SYNC | O_RDONLY, 0666);
	else return -1;
	ret = read(fd, buf, 32);
	if(ret == -1) {
		printf("VDEBUG__ read failed !\n");
		close(fd);
		return 0;
	}
	else if(!ret) {
		close(fd);
		return 0;
	}
	else {
		close(fd);
		return atoi(buf);
	}
}

/*
 * Return true is data write to server is pending
 * else return flase
 */
int check_data_status(void)
{
	int events_fd=0, synced_fd=0;
	int total_events=0, synced_events=0;
	char buf[32];

	pthread_mutex_lock(&mtx);

	events_fd = open("TOTAL_EVENTS", O_SYNC | O_RDONLY, 0666);
	synced_fd = open("SYNCED_EVENTS", O_SYNC | O_RDONLY, 0666);
	
	if(synced_fd == -1)
		goto skip_synced;

	read(synced_fd, buf, 32);
	synced_events = atoi(buf);
skip_synced:
	read(events_fd, buf, 32);
	total_events = atoi(buf);

	printf("total_events: %d || synced_events: %d\n",total_events, synced_events);

	close(events_fd);
	close(synced_fd);
	pthread_mutex_unlock(&mtx);

	return (total_events-synced_events);
}

void *sync_func(void *arg)
{
	int fd_cl, t_seek=0, synced_events, synced_count=0, total_count=0;
	char buf[MSG_LEN], events_buf[32];
	int ret=0;

	while(1) {
		ret = check_data_status();
		if((ret > 0) &&  client_connect()) {

		//	printf("data_status : %d\n", check_data_status());
			
			pthread_mutex_lock(&mtx);
			fd_cl = open(CLIENT_FILENAME, O_SYNC | O_RDONLY, 0666);	
			do {
				total_count = current_events(1);
				synced_count = current_events(2);
				printf("++ total_events: %d || synced_events: %d\n", total_count, synced_count);
				ret = lseek(fd_cl, 0, SEEK_SET);
				printf("VDEBUG__ 11 Seek position = %d || return %d\n", 0, ret);
				ret = lseek(fd_cl, (MSG_LEN*synced_count), SEEK_SET);
				printf("VDEBUG__ 22 Seek position = %d || return %d\n", (MSG_LEN*synced_count), ret);
				ret = read(fd_cl, buf, MSG_LEN);
				printf("VDEBUG__ sync_func() read %d bytes\n", ret);

				printf("VDEBUG__ Buffer to publish: %s\n", buf);

				if(!publish(MSG_LEN, buf)) {				/* Considering publish will return 0 when sucess */
					/* Saving number of events to disk */
					synced_events = open("SYNCED_EVENTS", O_CREAT | O_TRUNC | O_SYNC | O_WRONLY, 0666);
					ret = lseek(synced_events, 0, SEEK_SET);
					printf("VDEBUG__ lseeked synced count file to %d\n", ret);
					sprintf(events_buf, "%d", ++synced_count);
here:				ret = write(synced_events, events_buf, strlen(events_buf));
					close(synced_events);
					if (ret==-1) {
						printf("VDEBUG__ writing synced count failed !\n");
						printf("Retrying..\n");
						sleep(1);
						goto here;
					}
					printf("VDEBUG__ written synced count %d\n", synced_count);
				}
			} while (synced_count < total_count);
			close(fd_cl);

			MQTTClient_disconnect(client, 10000);
			pthread_mutex_unlock(&mtx);
		}
		else {
			sleep(10);
		}
	}
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

