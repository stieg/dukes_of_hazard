#include <glib.h>
#include <glib/gthread.h>

#define N_THREADS 4

static gpointer
thread_func(gpointer p)
{
	GAsyncQueue *q = p;
	gint i;

	for (i = 1; i <= 1000000; i++)
		g_async_queue_push(q, GINT_TO_POINTER(i));
	for (i = 1; i <= 1000000; i++)
		g_assert(g_async_queue_pop(q) != NULL);

	return NULL;
}

gint
main(gint   argc,
     gchar *argv[])
{
	GAsyncQueue *q;
	gint i;
	GThread *threads[N_THREADS];

	g_thread_init(NULL);

	q = g_async_queue_new();

	g_message("Starting basic tests.");
	for (i = 1; i <= 1000000; i++)
		g_async_queue_push(q, GINT_TO_POINTER(i));

	for (i = 1; i <= 1000000; i++)
		g_assert_cmpint(GPOINTER_TO_INT(g_async_queue_pop(q)), ==, i);

	g_message("Starting threading tests.");
	for (i = 0; i < N_THREADS; i++) {
		threads[i] = g_thread_create(thread_func, q, TRUE, NULL);
		g_assert(threads[i]);
	}
	for (i = 0; i < N_THREADS; i++)
		g_thread_join(threads[i]);

	g_message("Tests done.");

	return 0;
}
