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
#include <stdatomic.h>

#define BUFFER_SIZE 1000
static atomic_int pCountLock = 0;

//producer count storage
volatile unsigned int producedCount = 0;

pthread_t producers[4];

static void *produce(void *params);

/**
* Semaphores and Mutex
*/
static MSG_BLOCK buffer[BUFFER_SIZE];
static int write_index = 0; 
static int read_index = 0;

static sem_t semFull;
static sem_t semEmpty;
static pthread_mutex_t mutexBuffer;
/*
* Creates the synchronization elements.
* @return ERROR_SUCCESS if the init is ok, ERROR_INIT otherwise
*/
static unsigned int createSynchronizationObjects(void);

static void pCountLockTake(void) {
    int expected = 0;
    
    while (!atomic_compare_exchange_weak(&pCountLock, &expected, 1)) {
       expected = 0;
    }
}

static void pCountLockRelease(void) {
    atomic_store(&pCountLock, 0);
}

/*
* Increments the produce count.
*/
static void incrementProducedCount(void);

static unsigned int createSynchronizationObjects(void)
{
	if (sem_init(&semEmpty, 0, BUFFER_SIZE) != 0) return ERROR_INIT;
    if (sem_init(&semFull, 0, 0) != 0) { sem_destroy(&semEmpty); return ERROR_INIT; }
    if (pthread_mutex_init(&mutexBuffer, NULL) != 0) {
        sem_destroy(&semEmpty); sem_destroy(&semFull); return ERROR_INIT;
    }
	atomic_store(&pCountLock, 0);
	printf("[acquisitionManager]Semaphore created\n");
	return ERROR_SUCCESS;
}

static void incrementProducedCount(void)
{
	pCountLockTake();
    producedCount++;
    pCountLockRelease();
}

unsigned int getProducedCount(void)
{
	unsigned int p = 0;
	pCountLockTake();
    p = producedCount;
    pCountLockRelease();
	return p;
}


MSG_BLOCK getMessage(void){
	sem_wait(&semFull);
    pthread_mutex_lock(&mutexBuffer);
    MSG_BLOCK msg = buffer[read_index];
    read_index = (read_index + 1) % BUFFER_SIZE;

    pthread_mutex_unlock(&mutexBuffer);
    sem_post(&semEmpty);
    return msg;
}

static void putMessage(MSG_BLOCK msg){
    sem_wait(&semEmpty);
    pthread_mutex_lock(&mutexBuffer);

    buffer[write_index] = msg;
    write_index = (write_index + 1) % BUFFER_SIZE;

    pthread_mutex_unlock(&mutexBuffer);
    sem_post(&semFull);
}


//TODO create accessors to limit semaphore and mutex usage outside of this C module.

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
		if(pthread_create(&producers[i], NULL, produce, (void*)(size_t)i) != 0) {
            return ERROR_INIT;
        }
	}

	return ERROR_SUCCESS;
}

void acquisitionManagerJoin(void)
{
	unsigned int i;
	for (i = 0; i < PRODUCER_COUNT; i++)
	{
		pthread_join(producers[i], NULL);
	}

	sem_destroy(&semFull);
    sem_destroy(&semEmpty);
    pthread_mutex_destroy(&mutexBuffer);
	printf("[acquisitionManager]Semaphore cleaned\n");
}

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

        // Toujours notre correction importante : check == 1
        if(messageCheck(&msg) == ERROR_SUCCESS)
        {
            putMessage(msg);
            incrementProducedCount(); // Utilise maintenant notre Spinlock CAS
            D(printf("[acquisitionManagerTestAndSet] Producer %d produced message %d\n", (unsigned int)(size_t)params, i));
        }
        else
        {
            D(printf("[acquisitionManagerTestAndSet] Producer %d produced corrupted message %d\n", (unsigned int)(size_t)params, i));
        }
		sleep(PRODUCER_SLEEP_TIME+(rand() % 5));
	}
	printf("[acquisitionManager] %d termination\n", gettid());
	pthread_exit(NULL);
}