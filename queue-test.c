#include <glib.h>
#include <glib/gthread.h>

#include "queue.h"

#define N_THREADS 4

static gpointer
thread_func(gpointer p)
{
	Queue *q = p;
	gint i;

	for (i = 1; i < 1000000; i++)
		queue_enqueue(q, GINT_TO_POINTER(i));
	for (i = 1; i < 1000000; i++)
		g_assert(queue_dequeue(q) > 0);

	return NULL;
}

gint
main (gint   argc,
      gchar *argv[])
{
	GThread *threads[N_THREADS];
	Queue *q;
	gint i;

	g_thread_init(NULL);

	g_message("Starting basic test.");

	/*
	 * Basic operational test.
	 */
	q = queue_new();
	queue_enqueue(q, "1");
	queue_enqueue(q, "2");
	queue_enqueue(q, "3");
	queue_enqueue(q, "4");
	g_assert_cmpstr(queue_dequeue(q), ==, "1");
	g_assert_cmpstr(queue_dequeue(q), ==, "2");
	g_assert_cmpstr(queue_dequeue(q), ==, "3");
	g_assert_cmpstr(queue_dequeue(q), ==, "4");
	g_assert(queue_dequeue(q) == NULL);

	g_message("Starting sizing test.");

	/*
	 * Mild sizing test.
	 */
	for (i = 1; i <= 1000000; i++) {
		queue_enqueue(q, GINT_TO_POINTER(i));
	}

	for (i = 1; i <= 1000000; i++) {
		g_assert_cmpint(GPOINTER_TO_INT(queue_dequeue(q)), ==, i);
	}

	g_message("Starting concurrent test.");

	/*
	 * Concurrent test for load/contention.
	 */
	for (i = 0; i < N_THREADS; i++) {
		threads[i] = g_thread_create(thread_func, q, TRUE, NULL);
		g_assert(threads[i]);
	}

	for (i = 0; i < N_THREADS; i++) {
		g_thread_join(threads[i]);
	}

	g_message("Tests done.");

	return 0;
}
