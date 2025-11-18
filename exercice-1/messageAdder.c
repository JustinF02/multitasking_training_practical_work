#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h> 
#include <unistd.h>
#include <pthread.h>
#include "messageAdder.h"
#include "msg.h"
#include "iMessageAdder.h"
#include "multitaskingAccumulator.h"
#include "iAcquisitionManager.h"
#include "debug.h"
#include <sys/types.h> 

//consumer thread
pthread_t consumer;
//Message computed
volatile MSG_BLOCK out;
//Consumer count storage
volatile unsigned int consumeCount = 0;

static pthread_mutex_t mutexConsumedCount;
static pthread_mutex_t mutexOut;

/**
 * Increments the consume count.
 */
static void incrementConsumeCount(void){
	pthread_mutex_lock(&mutexConsumedCount);
	consumeCount++;
	pthread_mutex_unlock(&mutexConsumedCount);
}

/**
 * Consumer entry point.
 */
static void *sum( void *parameters );


MSG_BLOCK getCurrentSum(){
	pthread_mutex_lock(&mutexOut);
	MSG_BLOCK currentSum = out;
	pthread_mutex_unlock(&mutexOut);
	return currentSum;
}

unsigned int getConsumedCount(){
	pthread_mutex_lock(&mutexConsumedCount);
	unsigned int count = consumeCount;
	pthread_mutex_unlock(&mutexConsumedCount);
	return count;
}

void getSumAndCount(MSG_BLOCK *sumDest, unsigned int *countDest) {
    pthread_mutex_lock(&mutexOut)
    
    //*sumDest = *((MSG_BLOCK*)&out);
	*sumDest = out;
    *countDest = consumeCount;
    
    pthread_mutex_unlock(&mutexOut); 
}


void messageAdderInit(void){
	out.checksum = 0;
	for (size_t i = 0; i < DATA_SIZE; i++)
	{
		out.mData[i] = 0;
	}
	if(pthread_mutex_init(&mutexConsumedCount, NULL) != 0){
		perror("[messageAdder] Error initializing consumed count mutex");
		return;
	}
	if(pthread_mutex_init(&mutexOut, NULL) != 0){
        perror("[messageAdder] Error initializing out mutex");
        pthread_mutex_destroy(&mutexConsumedCount); // Nettoyage
        return;
    }

	if(pthread_create(&consumer, NULL, sum, NULL) != 0){
		perror("[messageAdder] Error creating sum thread");
		return;
	}
}

void messageAdderJoin(void){
	pthread_join(consumer, NULL);
	printf("[messageAdder] Sum thread joined\n");
	pthread_mutex_destroy(&mutexConsumedCount);
	pthread_mutex_destroy(&mutexOut);
}

static void *sum( void *parameters )
{
	(void)parameters;

	D(printf("[messageAdder]Thread created for sum with id %d\n", gettid()));
	unsigned int i = 0;
	while(i<ADDER_LOOP_LIMIT){
		i++;
		
		MSG_BLOCK msg = getMessage();
		pthread_mutex_lock(&mutexOut);
		messageAdd(&out, &msg);
		incrementConsumeCount();
		pthread_mutex_unlock(&mutexOut);

		sleep(ADDER_SLEEP_TIME);
	}
	printf("[messageAdder] %d termination\n", gettid());
	pthread_exit(NULL);
}


