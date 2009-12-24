#include <glib.h>

#include "queue.h"
#include "hazard.h"

#define CompareAndSwap(p,o,n) \
	(g_atomic_pointer_compare_and_exchange((gpointer*)(p),(o),(n)))
#define VolatileGet(p) (g_atomic_pointer_get((p)))

typedef struct _Link Link;

struct _Link {
	gconstpointer data;
	Link *next;
};

struct _Queue {
	Link *head;
	Link *tail;
	volatile gint ref_count;
};

static void
link_free(gpointer p)
{
	g_slice_free(Link, p);
}

Queue*
queue_new(void)
{
	static gboolean init = FALSE;
	Queue *q;

	if (g_once_init_enter((gsize *)&init)) {
		hazard_free_func = link_free;
		g_once_init_leave((gsize *)&init, (gsize)TRUE);
	}

	q = g_slice_new0(Queue);
	q->ref_count = 1;
	q->head = q->tail = g_slice_new0(Link);

	return q;
}

Queue*
queue_ref(Queue *q)
{
	g_return_val_if_fail(q != NULL, NULL);
	g_return_val_if_fail(q->ref_count > 0, NULL);

	g_atomic_int_inc(&q->ref_count);
	return q;
}

void
queue_unref(Queue *q)
{
	Link *l;

	g_return_if_fail(q != NULL);
	g_return_if_fail(q->ref_count > 0);

	if (g_atomic_int_dec_and_test(&q->ref_count)) {
		for (l = q->head; l; l = l->next)
			g_slice_free(Link, l);
		g_slice_free(Queue, q);
	}
}

void
queue_enqueue(Queue         *q,
              gconstpointer  p)
{
	Link *l, *t, *next;

	g_return_if_fail(q != NULL);
	g_return_if_fail(p != NULL);

	l = g_slice_new(Link);
	l->data = p;
	l->next = NULL;

	while (TRUE) {
		t = q->tail;
		HAZARD_SET(0, t);
		if (q->tail != t)
			continue;
		next = t->next;
		if (q->tail != t)
			continue;
		if (next != NULL) {
			CompareAndSwap(&q->tail, t, next);
			continue;
		}
		if (CompareAndSwap(&t->next, NULL, l))
			break;
	}

	CompareAndSwap(&q->tail, t, l);
}

gpointer 
queue_dequeue(Queue *q)
{
	Link *h, *t, *next;
	gconstpointer data;

	g_return_val_if_fail(q != NULL, NULL);

	while (TRUE) {
		h = q->head;
		HAZARD_SET(0, h);
		if (q->head != h)
			continue;
		t = q->tail;
		next = h->next;
		HAZARD_SET(1, next);
		if (q->head != h)
			continue;
		if (next == NULL)
			return NULL;
		if (h == t) {
			CompareAndSwap(&q->tail, t, next);
			continue;
		}
		data = next->data;
		if (CompareAndSwap(&q->head, h, next))
			break;
	}

	hazard_retire(h);

	return (gpointer)data;
}
