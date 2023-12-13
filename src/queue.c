#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t *q)
{
        if (q == NULL)
                return 1;
        return (q->size == 0);
}

void enqueue(struct queue_t *q, struct pcb_t *proc)
{
        /* TODO: put a new process to queue [q] */
        pthread_mutex_lock(&q->lock);
        if (!q->size)
        {
                q->proc[0] = proc;
                q->size++;
        }
        else
        {
                int it = 0;
                while (it < q->size)
                {
                        if (q->proc[it]->priority > proc->priority)
                        {
                                for (int i = q->size - 1; i > it; i--)
                                        q->proc[i] = q->proc[i - 1];
                                q->proc[it] = proc;
                                break;
                        }
                        it++;
                }
                if (it == q->size)
                {
                        q->proc[it] = proc;
                        q->size++;
                }
        }
        pthread_mutex_unlock(&q->lock);
}

struct pcb_t *dequeue(struct queue_t *q)
{
        /* TODO: return a pcb whose prioprity is the highest
         * in the queue [q] and remember to remove it from q
         * */
        struct pcb_t *proc = NULL;
        pthread_mutex_lock(&q->lock);
        if (q->proc[0] != NULL)
        {
                proc = q->proc[0];
                for (int i = 0; i < q->size - 1; i++)
                        q->proc[i] = q->proc[i + 1];
                q->size--;
        }
        pthread_mutex_unlock(&q->lock);

        return proc;
}
