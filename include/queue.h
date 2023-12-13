
#ifndef QUEUE_H
#define QUEUE_H

#include "common.h"

#include <pthread.h>

#define MAX_QUEUE_SIZE 10

struct queue_t {
	struct pcb_t * proc[MAX_QUEUE_SIZE];
	int size;

	pthread_mutex_t lock;
};

void enqueue(struct queue_t * q, struct pcb_t * proc);

struct pcb_t * dequeue(struct queue_t * q);

int empty(struct queue_t * q);

#endif

