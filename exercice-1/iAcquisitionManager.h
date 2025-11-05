#ifndef I_ACQUISITION_MANAGER_H
#define I_ACQUISITION_MANAGER_H

#include "msg.h"

/**
* Get the number of produced messages.
*/
unsigned int getProducedCount(void);

/**
* Gets a message if any, otherwise waits for a message.
*/
MSG_BLOCK getMessage(void);

/**
* Puts a message into the buffer.
* @param msg The message to put into the buffer.
*/
static void putMessage(MSG_BLOCK msg);
#endif