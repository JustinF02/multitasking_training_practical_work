#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include "displayManager.h"
#include "iDisplay.h"
#include "iAcquisitionManager.h"
#include "iMessageAdder.h"
#include "msg.h"
#include "multitaskingAccumulator.h"
#include "debug.h"

// DisplayManager thread.
pthread_t displayThread;

/**
 * Display manager entry point.
 * */
static void *display( void *parameters );


void displayManagerInit(void){
	if(pthread_create(&displayThread, NULL, display, NULL) != 0){
		perror("[displayManager] Error creating display thread");
	}
}

void displayManagerJoin(void){
	if(pthread_join(displayThread, NULL) != 0){
		perror("[displayManager] Error joining display thread");
	}
}

static void *display( void *parameters )
{

	(void)parameters;

	D(printf("[displayManager]Thread created for display with id %d\n", gettid()));
	unsigned int diffCount = 0;
	while(diffCount < DISPLAY_LOOP_LIMIT){
		

		unsigned int producedCount = getProducedCount();
		unsigned int consumedCount = getConsumedCount();
		MSG_BLOCK currentSum = getCurrentSum();
		printf("[displayManager] Display %d: Produced count: %u, Consumed count: %u, Current sum checksum: %u\n",
		       diffCount,
		       producedCount,
		       consumedCount,
		       currentSum.checksum);

		messageDisplay((volatile MSG_BLOCK *)&currentSum);
        fflush(stdout);

		sleep(DISPLAY_SLEEP_TIME);
		diffCount++;

		sleep(DISPLAY_SLEEP_TIME);
	}
	printf("[displayManager] %d termination\n", gettid());
    pthread_exit(NULL);
}