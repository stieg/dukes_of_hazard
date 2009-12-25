/* lf-queue.c
 *
 * Copyright (c) 2009 Christian Hergert
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "lf-queue.h"
#include "lf-hazard.h"

typedef struct _LfNode LfNode;

struct _LfNode {
	gpointer  data;
	LfNode   *next;
};

struct _LfQueue {
	LfNode        *head;
	LfNode        *tail;
	volatile gint  ref_count;
};

static void
lf_node_free(LfNode *node)
{
	g_return_if_fail(node != NULL);
	g_slice_free(LfNode, node);
}

static void
lf_queue_destroy(LfQueue *queue)
{
	LfNode *node;

	g_return_if_fail(queue != NULL);

	for (node = queue->head; node; node = node->next) {
		lf_node_free(node);
	}
}

/**
 * lf_queue_new:
 *
 * Creates a new instance of #LfQueue.  The #LfQueue structure is reference
 * counted and should be freed using lf_queue_unref().
 *
 * Returns: The newly created #LfQueue.
 * Side effects: None.
 */
LfQueue*
lf_queue_new(void)
{
	static gboolean initialized = FALSE;
	LfQueue *queue;

	if (g_once_init_enter((gsize *)&initialized)) {
		lf_hazard_free = (gpointer)lf_node_free;
		g_once_init_leave((gsize *)&initialized, TRUE);
	}

	queue = g_slice_new(LfQueue);
	queue->head = queue->tail = g_slice_new0(LfNode);
	queue->ref_count = 1;

	return queue;
}

/**
 * lf_queue_ref:
 * @queue: A #LfQueue
 *
 * Atomically increments the reference count of @queue by one.
 *
 * Returns: A reference to @queue.
 * Side effects: None.
 */
LfQueue*
lf_queue_ref(LfQueue *queue)
{
	g_return_val_if_fail(queue != NULL, NULL);
	g_return_val_if_fail(queue->ref_count > 0, NULL);

	g_atomic_int_inc(&queue->ref_count);
	return queue;
}

/**
 * lf_queue_unref:
 * @queue: A #LfQUeue
 *
 * Decrements the reference count of @queue by one.  When the reference count
 * reaches zero, the structures allocations are released and the queue is
 * freed.
 */
void
lf_queue_unref(LfQueue *queue)
{
	g_return_if_fail(queue != NULL);
	g_return_if_fail(queue->ref_count > 0);

	if (g_atomic_int_dec_and_test(&queue->ref_count)) {
		lf_queue_destroy(queue);
		g_slice_free(LfQueue, queue);
	}
}

/**
 * lf_queue_get_type:
 *
 * Retrieves the GObject type system's type identifier for #LfQueue.
 *
 * Returns: A #GType containing the type id.
 * Side effects: Registers the #LfQueue type if not already.
 */
GType
lf_queue_get_type(void)
{
	static GType type_id = 0;
	GType tmp_id;

	if (g_once_init_enter((gsize *)&type_id)) {
		tmp_id = g_boxed_type_register_static("LfQueue",
		                                      (GBoxedCopyFunc)lf_queue_ref,
		                                      (GBoxedFreeFunc)lf_queue_unref);
		g_once_init_leave((gsize *)&type_id, tmp_id);
	}

	return type_id;
}

/**
 * lf_queue_enqueue:
 * @queue: A #LfQueue.
 * @data: a non-NULL pointer.
 *
 * Enqueues an item into the #LfQueue.  The pointer must be non-%NULL.
 *
 * Side effects: None.
 */
void
lf_queue_enqueue(LfQueue       *queue,
                 gconstpointer  data)
{
	LfNode *node, *tail, *next;
	LF_HAZARD_INIT;

	g_return_if_fail(queue != NULL);
	g_return_if_fail(data != NULL);

	/*
	 * Create a new LfNode to add to the queue's linked-list.
	 */
	node = g_slice_new(LfNode);
	node->data = (gpointer)data;
	node->next = NULL;

	/*
	 * Attempt to add our new LfNode to the linked list until we succeed.
	 * If the queue is only half-consistent due to another thread only
	 * completing half of its work, we will clean up after it.
	 */
	while (TRUE) {
		tail = queue->tail;          /* Retrieve the current tail */
		LF_HAZARD_SET(0, tail);      /* Mark the pointer as hazardous */
		if (queue->tail != tail)     /* Ensure tail is still valid */
			continue;
		next = tail->next;           /* Check for possible new tail */
		if (queue->tail != tail)     /* Ensure (again) tail is still vaild */
			continue;
		if (next != NULL) {          /* Inconsistent state, help it along */
			g_atomic_pointer_compare_and_exchange((gpointer *)&queue->tail,
			                                      tail, next);
			continue;
		}
		if (g_atomic_pointer_compare_and_exchange(      /* Attempt to add    */
				(gpointer *)&tail->next, NULL, node)) { /* ourself to end of */
			break;                                      /* queue.            */
		}
	}

	/*
	 * Attempt to update the tail to point at our new node.  If this fails
	 * it is because another thread has beaten us.  Not to worry, readers
	 * and future writers can move the queue into a consistent state.
	 */
	g_atomic_pointer_compare_and_exchange((gpointer *)&queue->tail, tail, node);
}

/**
 * lf_queue_dequeue:
 * @queue: A #LfQUeue
 *
 * Dequeues an item from the queue.  If the queue is empty, %NULL is returned.
 *
 * Returns: An item from the queue or %NULL.
 *
 * Side effects: Hazard pointer reclaimation can occur meaning that structures
 *   no longer in use may be freed by the system.
 */
gpointer
lf_queue_dequeue(LfQueue *queue)
{
	LfNode *head, *tail, *next;
	gpointer data;
	LF_HAZARD_INIT;

	g_return_val_if_fail(queue != NULL, NULL);

	/*
	 * Attempt to retrieve an LfNode off the linked-list until we succeed.
	 * If the queue is in an inconsistent state we will attempt to clean
	 * up after the last operation before retreiving our item.
	 */
	while (TRUE) {
		head = queue->head;      /* Retrieve the current head of queue */
		LF_HAZARD_SET(0, head);  /* Notify threads that head is a hazard */
		if (queue->head != head) /* Ensure head is still the queues head */
			continue;
		tail = queue->tail;      /* Retreive the current tail of queue */
		next = head->next;       /* Retreive heads next (to become new head) */
		LF_HAZARD_SET(1, next);  /* Notify threads next is a hazard */ 
		if (queue->head != head) /* Ensure head is still the queues head */
			continue;
		if (next == NULL)        /* If there is no next, queue is empty */
			return NULL;
		if (head == tail) {      /* Inconsistent state, help thread along */
			g_atomic_pointer_compare_and_exchange((gpointer *)&queue->tail,
			                                      tail, next);
			continue;
		}
		data = next->data;       /* Retrieve data for the removing node */
		                         /* Take the head of the queue */
		if (g_atomic_pointer_compare_and_exchange((gpointer *)&queue->head,
		                                          head, next))
			break;
	}

	/*
	 * head is no longer a hazard.  Potentially do a reclaimation of
	 * memory no longer hazardous.
	 */
	LF_HAZARD_UNSET(head);

	return data;
}
