#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include "acquisitionManager.h"
#include "msg.h"
#include "iSensor.h"
#include "multitaskingAccumulator.h"
#include "iAcquisitionManager.h"
#include "debug.h"
#include <sys/types.h> 

#define BUFFER_SIZE 1000

//producer count storage
volatile unsigned int produceCount = 0;

pthread_t producers[4];

static void *produce(void *params);

/**
* Semaphores and Mutex
*/
//shared circular buffer between producers and consumers
static MSG_BLOCK buffer[BUFFER_SIZE];
static int write_index = 0; //Head (where to write)
static int read_index = 0;  //Tail (where to read)
static pthread_mutex_t mutexBuffer; // Protects access to write_index and read_index

//synchronization objects
static sem_t semFull;  //count messages in the buffer
static sem_t semEmpty; //count free spaces in the buffer

//produced count
static volatile unsigned int producedCount = 0; // Volatile car partagé et lu sans mutex dans getProducedCount
static pthread_mutex_t mutexProducedCount;      // Mutex pour protéger 'producedCount'

/*
* Creates the synchronization elements.
* @return ERROR_SUCCESS if the init is ok, ERROR_INIT otherwise
*/
static unsigned int createSynchronizationObjects(void){

	//init semaphore semEmpty to BUFFER_SIZE (all buffer slots are free)
    if (sem_init(&semEmpty, 0, BUFFER_SIZE) != 0) {
        perror("[acquisitionManager] error sem_init(semEmpty)");
        return ERROR_INIT;
    }

	//init semaphore semFull to 0 (buffer is empty)
    if (sem_init(&semFull, 0, 0) != 0) {
        perror("[acquisitionManager] error sem_init(semFull)");
        sem_destroy(&semEmpty);
        return ERROR_INIT;
    }

	//init mutexBuffer
    if (pthread_mutex_init(&mutexBuffer, NULL) != 0) {
        perror("[acquisitionManager] error pthread_mutex_init(mutexBuffer)");
        sem_destroy(&semEmpty);
        sem_destroy(&semFull);
        return ERROR_INIT;
    }

	//init mutexProducedCount
    if (pthread_mutex_init(&mutexProducedCount, NULL) != 0) {
        perror("[acquisitionManager] error pthread_mutex_init(mutexProducedCount)");
        sem_destroy(&semEmpty);
        sem_destroy(&semFull);
        pthread_mutex_destroy(&mutexBuffer);
        return ERROR_INIT;
    }

    printf("[acquisitionManager]Semaphore created\n");
    return ERROR_SUCCESS;

}

//increments the produced count safely
static void incrementProducedCount(void){
	pthread_mutex_lock(&mutexProducedCount);
	producedCount++;
	pthread_mutex_unlock(&mutexProducedCount);
}

//accessor to get the produced count
unsigned int getProducedCount(void)
{
	unsigned int p = 0;
	pthread_mutex_lock(&mutexProducedCount);
	p = producedCount;
	pthread_mutex_unlock(&mutexProducedCount);
	return p;
}

//accessor to get a message from the buffer that limits the semaphore and mutex usage
MSG_BLOCK getMessage(void){
	sem_wait(&semFull); // Wait for at least one full slot
	pthread_mutex_lock(&mutexBuffer);

	MSG_BLOCK msg = buffer[read_index];
	read_index = (read_index + 1) % BUFFER_SIZE; // Move to the next slot
	pthread_mutex_unlock(&mutexBuffer);
	sem_post(&semEmpty); // Signal that there is an empty slot
	return msg;
}

//accessor to put a message in the buffer that limits the semaphore and mutex usage
static void putMessage(MSG_BLOCK msg){
	sem_wait(&semEmpty); // Wait for at least one empty slot
	pthread_mutex_lock(&mutexBuffer);

	buffer[write_index] = msg;
	write_index = (write_index + 1) % BUFFER_SIZE; // Move to the next slot

	pthread_mutex_unlock(&mutexBuffer);
	sem_post(&semFull); // Signal that there is a full slot
}

//Initializes the acquisition manager
unsigned int acquisitionManagerInit(void)
{
	unsigned int i;
	printf("[acquisitionManager]Synchronization initialization in progress...\n");
	fflush( stdout );
	if (createSynchronizationObjects() == ERROR_INIT)
		return ERROR_INIT;
	
	printf("[acquisitionManager]Synchronization initialization done.\n");

	for (i = 0; i < PRODUCER_COUNT; i++)
	{
		//create producer threads
		if(pthread_create(&producers[i], NULL, produce, (void*)(size_t)i) != 0)
		{
			perror("[acquisitionManager] Error creating producer thread");
			return ERROR_INIT;
		}
	}

	return ERROR_SUCCESS;
}

//Joins the acquisition manager threads and cleans up synchronization objects
void acquisitionManagerJoin(void)
{
	unsigned int i;
	for (i = 0; i < PRODUCER_COUNT; i++)
	{
		pthread_join(producers[i], NULL);
	}

	//clean up synchronization objects
	sem_destroy(&semFull);
	sem_destroy(&semEmpty);
	pthread_mutex_destroy(&mutexBuffer);
	pthread_mutex_destroy(&mutexProducedCount);

	printf("[acquisitionManager]Semaphore cleaned\n");
}

//producer thread function
void *produce(void* params)
{
	D(printf("[acquisitionManager]Producer created with id %d\n", gettid()));
	unsigned int i = 0;
	unsigned int sensorId = (unsigned int)(size_t)params;
	while (i < PRODUCER_LOOP_LIMIT)
	{
		i++;
		
		MSG_BLOCK msg;
		getInput(sensorId, &msg);

		if(messageCheck(&msg) == ERROR_SUCCESS)
		{
			putMessage(msg);
			incrementProducedCount();
			D(printf("[acquisitionManager] Producer %d produced message %d\n", (unsigned int)(size_t)params, i));
		}
		else
		{
			D(printf("[acquisitionManager] Producer %d produced a corrupted message %d\n", (unsigned int)(size_t)params, i));
		}

		sleep(PRODUCER_SLEEP_TIME+(rand() % 5));
	}
	printf("[acquisitionManager] %d termination\n", gettid());
	//clean up resources
	pthread_exit(NULL);
}