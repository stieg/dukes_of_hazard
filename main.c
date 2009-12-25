/* main.c
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

#include <glib.h>

#ifdef __linux__
#include <sys/sysinfo.h>
#else
#ifdef __APPLE__
#include <sys/param.h>
#include <sys/sysctl.h>
#else
#error "Building on platforms other than Linux or OSX is not yet supported."
#endif /* __APPLE__ */
#endif /* __linux__ */

#include "lf-queue.h"

static gint
get_num_cpu(void)
{
#ifdef __linux__
	return get_nprocs();
#endif

#ifdef __APPLE__
	gint i = 0;
	size_t s = sizeof(i);
	if (sysctlbyname("hw.ncpu", &i, &s, NULL, 0))
		return 1;
	return i;
#endif
}

static void
test_LfQueue_basic(void)
{
	LfQueue *q;

	q = lf_queue_new();
	g_assert(q);

	lf_queue_enqueue(q, "String 1");
	lf_queue_enqueue(q, "String 2");
	lf_queue_enqueue(q, "String 3");
	lf_queue_enqueue(q, "String 4");

	g_assert_cmpstr(lf_queue_dequeue(q), ==, "String 1");
	g_assert_cmpstr(lf_queue_dequeue(q), ==, "String 2");
	g_assert_cmpstr(lf_queue_dequeue(q), ==, "String 3");
	g_assert_cmpstr(lf_queue_dequeue(q), ==, "String 4");

	g_assert(!lf_queue_dequeue(q));
}

static gpointer
test_LfQueue_threaded_alternate_enq_deq_thread_func(gpointer data)
{
	LfQueue *q = data;
	gint i, n;
	g_assert(q);

	n = g_test_perf() ? 10000000 : 1000000;

	for (i = 1; i <= n; i++) {
		if (i % 2 == 1) {
			lf_queue_enqueue(q, GINT_TO_POINTER(i));
		} else {
			g_assert(lf_queue_dequeue(q));
		}
	}

	return NULL;
}

/*
 * This test will spawn (N_CPU * 2) threads and have them all work on adding
 * and removing many items concurrently.  Each thread will alternate between
 * adding an item and removing an item such that the typical potential for
 * an ABA[1] problem is increased.  The hazard pointers should always prevent
 * the ABA however.
 *
 * [1] http://en.wikipedia.org/wiki/ABA_problem
 */
static void
test_LfQueue_threaded_alternate_enq_deq(void)
{
	gint n_threads = get_num_cpu() * 2;
	LfQueue *q;
	GThread **threads;
	gint i;

	threads = g_malloc(sizeof(gpointer) * n_threads);
	q = lf_queue_new();
	g_assert(q);

	for (i = 0; i < n_threads; i++) {
		threads[i] = g_thread_create(
			test_LfQueue_threaded_alternate_enq_deq_thread_func,
			q, TRUE, NULL);
	}

	for (i = 0; i < n_threads; i++) {
		g_thread_join(threads[i]);
	}

	g_free(threads);
}

gint
main(gint   argc,
     gchar *argv[])
{
	g_test_init(&argc, &argv, NULL);

	g_thread_init(NULL);

	g_test_add_func("/LfQueue/basic", test_LfQueue_basic);
	g_test_add_func("/LfQueue/threaded_alternate_enq_deq",
		            test_LfQueue_threaded_alternate_enq_deq);

	return g_test_run();
}
