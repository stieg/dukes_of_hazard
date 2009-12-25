/* lf-hazard.h
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

#ifndef __LF_HAZARD_H__
#define __LF_HAZARD_H__

#include <glib.h>

G_BEGIN_DECLS

/**
 * @LF_HAZARD_K: The maximum number of hazard pointers needed by a thread at
 *               any given time.  This is 2 for most of the lock-free data
 *               structures out there.
 */
#ifndef LF_HAZARD_K
#define LF_HAZARD_K (2)
#endif

/**
 * @LF_HAZARD_R: An arbitrary number of hazard pointer list size at which
 *               reclaimation should occur.  You can tune this to your work
 *               loads for a small but noticable boost.  The ideal value will
 *               help you reach a worst-case running time of O(log N).
 */
#ifndef LF_HAZARD_R
#define LF_HAZARD_R (8)
#endif

#define LF_HAZARD_INIT LfHazard *myhazard = (g_static_private_get(&_lf_myhazard))
#define LF_HAZARD_TLS (myhazard)

#define LF_HAZARD_SET(i,p) G_STMT_START {                            \
    if (!myhazard) {                                                 \
        lf_hazard_thread_acquire();                                  \
        myhazard = g_static_private_get(&_lf_myhazard);              \
        g_assert(myhazard);                                          \
    }                                                                \
    myhazard->hp[(i)] = (p);                                         \
} G_STMT_END

#define LF_HAZARD_UNSET(p) G_STMT_START {                            \
    LfHazard *_head;                                                 \
    myhazard->rlist = g_slist_prepend(myhazard->rlist, (p));         \
    myhazard->rcount++;                                              \
    _head = _lf_hazards;                                             \
    if (myhazard->rcount >= (_LF_H + LF_HAZARD_R)) {                 \
        lf_hazard_scan(_head);                                       \
        lf_hazard_help_scan();                                       \
    }                                                                \
} G_STMT_END

#define G_SLIST_POP(l,d) G_STMT_START {                              \
    if (!(l)) {                                                      \
        *(d) = NULL;                                                 \
    } else {                                                         \
        *(d) = (l)->data;                                            \
        (l) = g_slist_delete_link((l), (l));                         \
    }                                                                \
} G_STMT_END

typedef struct _LfHazard LfHazard;

struct _LfHazard {
	gpointer  hp[LF_HAZARD_K];
	LfHazard *next;
	gboolean  active;
	GSList   *rlist;
	gint      rcount;
	GTree    *plist;
};

/*
 * Thread local hazard pointers.
 */
static GStaticPrivate _lf_myhazard = G_STATIC_PRIVATE_INIT;

/*
 * Global linked-list of all hazard pointers.
 */
static LfHazard *_lf_hazards = NULL;

/*
 * Total count of all potential hazard pointers.
 */
static gint _LF_H = 0;

/*
 * Hazard pointer callback for reclaimation of memory.  This should
 * be set by the data structure consuming lf-hazard.h.
 */
static void (*lf_hazard_free) (gpointer data) = NULL;

/*
 * Sorting method for GTree.
 */
static gint
lf_hazard_pointer_compare(gconstpointer a,
                          gconstpointer b)
{
	return (a == b) ? 0 : a - b;
}

/*
 * Method to acquire thread local data structures for hazard pointer
 * operation.  This is called automatically as needed when a new thread
 * enters the arena.
 *
 * Your data structure should provide a method for threads leaving the arena
 * to lose their allocated resources which should in turn call
 * lf_hazard_thread_release().
 */
static void
lf_hazard_thread_acquire(void)
{
	LfHazard *hazard, *old_head;
	gint old_count;

	/*
	 * Try to reclaim an existing, unused LfHazard structure.
	 */
	for (hazard = _lf_hazards; hazard; hazard = hazard->next) {
		if (hazard->active)
			continue;
		if (!g_atomic_int_compare_and_exchange(&hazard->active, FALSE, TRUE))
			continue;
		g_static_private_set(&_lf_myhazard, hazard, NULL);
		return;
	}

	/*
	 * No LfHazard could be reused.  We will create one and push it onto
	 * the head of the linked-list.
	 */
	old_count = g_atomic_int_exchange_and_add(&_LF_H, LF_HAZARD_K);
	hazard = g_slice_new0(LfHazard);
	hazard->active = TRUE;
	hazard->plist = g_tree_new(lf_hazard_pointer_compare);
	do {
		old_head = _lf_hazards;
		hazard->next = old_head;
	} while (!g_atomic_pointer_compare_and_exchange((gpointer *)&_lf_hazards,
	                                                old_head, hazard));
	g_static_private_set(&_lf_myhazard, hazard, NULL);
}

/*
 * This method will release the resources acquired for a thread to participate
 * in the lock-free arena.  This method should be called when a thread is no
 * longer going to participate in the consuming algorithm.  It is suggested
 * that you wrap a call to this method in your algorithm.
 */
#if 0
static void
lf_hazard_thread_release(void)
{
	gint i;

	for (i = 0; i < (LF_HAZARD_K - 1); i++) /* Clear any lingering hazards */
		myhazard->hp[i] = NULL;
	myhazard->active = FALSE;           /* Notify structure is avaialble */
}
#endif

/*
 * This method works in two stages.  The first stage scans all neighbor threads
 * for hazard pointers and stores them in a worst-case O(log N) balanced-binary
 * GTree.  The second stage looks to see if any hazard pointers in the threads
 * local hazard pointers are found in the GTree.  If they are not, they are
 * ready to be reclaimed.  If they are found, we store them back into our
 * list of hazard pointers for the next round of reclaimation.
 */
static void
lf_hazard_scan(LfHazard *head)
{
	LfHazard *hazard;
	GSList *tmplist;
	gpointer data = NULL;
	gint i;

	LF_HAZARD_INIT;

	/*
	 * Stage 1: Collect all the current hazard pointers from active threads.
	 */
	hazard = head;
	while (hazard != NULL) {
		for (i = 0; i < (LF_HAZARD_K - 1); i++) {
			data = g_atomic_pointer_get(&hazard->hp[i]);
			if (data != NULL)
				g_tree_insert(LF_HAZARD_TLS->plist, data, data);
		}
		hazard = hazard->next;
	}

	/*
	 * Stage 2: Reclaim expired hazard pointers.
	 */
	LF_HAZARD_TLS->rcount = 0;
	tmplist = LF_HAZARD_TLS->rlist;
	LF_HAZARD_TLS->rlist = NULL;
	G_SLIST_POP(tmplist, &data);
	while (data != NULL) {
		if (g_tree_lookup(LF_HAZARD_TLS->plist, data)) {
			LF_HAZARD_TLS->rlist = g_slist_prepend(LF_HAZARD_TLS->rlist, data);
			LF_HAZARD_TLS->rcount++;
		} else {
			lf_hazard_free(data);
		}
		G_SLIST_POP(tmplist, &data);
	}

	/*
	 * This is part of a micro-optimization for GLib 2.22+.  GTree became
	 * reference counted and allows us to reuse the structure and quickly
	 * remove all of the existing pointers from the tree.  However, it also
	 * decrements the reference count in the process.  Therefore, we increment
	 * it before-hand.
	 */
	LF_HAZARD_TLS->plist = g_tree_ref(LF_HAZARD_TLS->plist);
	g_tree_destroy(LF_HAZARD_TLS->plist);
}

static void
lf_hazard_help_scan(void)
{
	LfHazard *hazard, *head;
	gpointer data;

	LF_HAZARD_INIT;

	for (hazard = _lf_hazards; hazard; hazard = hazard->next) {
		if (hazard->active)
			continue;
		if (!g_atomic_int_compare_and_exchange(&hazard->active, FALSE, TRUE))
			continue;
		while (hazard->rcount > 0) {
			G_SLIST_POP(hazard->rlist, &data);
			hazard->rcount--;
			LF_HAZARD_TLS->rlist = g_slist_prepend(LF_HAZARD_TLS->rlist, data);
			LF_HAZARD_TLS->rcount++;
			head = _lf_hazards;
			if (LF_HAZARD_TLS->rcount >= (_LF_H + LF_HAZARD_R))
				lf_hazard_scan(head);
		}
		hazard->active = FALSE;
	}
}

#undef G_SLIST_POP

G_END_DECLS

#endif /* __LF_HAZARD_H__ */
