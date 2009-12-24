#ifndef __QUEUE_H__
#define __QUEUE_H__

#include <glib.h>

typedef struct _Queue Queue;

Queue*   queue_new     (void);
Queue*   queue_ref     (Queue *q);
void     queue_unref   (Queue *q);
void     queue_enqueue (Queue *q, gconstpointer p);
gpointer queue_dequeue (Queue *q);

#endif /* __QUEUE_H__ */
