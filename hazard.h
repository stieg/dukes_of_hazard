/*
 * Hazard Pointers for Lock-Free and Wait-Free algorithms.
 *
 * For more information on Hazard Pointers, see IEEE Transactions on
 * Parallel and Distributed Systems, Vol. 15, No. 6, June 2004.
 * Hazard Pointers: Safe Memory Reclaimation for Lock-Free Objects
 * by Maged M. Michael.
 */

#ifndef __HAZARD_H__
#define __HAZARD_H__

#include <glib.h>

#ifndef HAZARD_K
#define HAZARD_K 2
#endif

#ifndef HAZARD_R
#define HAZARD_R 10
#endif

#define HAZARD_SET(i,p) G_STMT_START { \
	if (!myhazard) \
		hazard_acquire(); \
	myhazard->hp[(i)] = (p); \
} G_STMT_END

typedef struct _Hazard Hazard;
typedef struct _HList HList;

struct _Hazard {
	gpointer  hp[HAZARD_K];
	Hazard   *next;
	gboolean  active;
	GSList   *rlist;
	gint      rcount;
};

struct _HList {
	gpointer  data;
	HList    *next;
};

__thread Hazard *myhazard = NULL;
static   Hazard *hazards = NULL;
static   gint    H = 0;

static void (*hazard_free_func) (gpointer p);

#define TAS(p) g_atomic_int_compare_and_exchange(p,FALSE,TRUE)

#if 0
static HList*
h_list_push(HList    *list,
            gpointer  data)
{
	HList *node;

	node = g_malloc(sizeof(HList));
	node->next = list;
	node->data = data;

	return node;
}

static HList*
h_list_pop(HList    *list,
           gpointer *data)
{
	HList *node, *result;

	g_assert(data);

	if (!list) {
		*data = NULL;
		return NULL;
	}

	node = list;
	result = list->next;
	*data = node->data;
	g_free(node);

	return result;
}
#endif

static void
hazard_acquire(void)
{
	Hazard *hazard, *oldhead;
	gint oldcount;

	for (hazard = hazards; hazard; hazard = hazard->next) {
		if (hazard->active)
			continue;
		if (!TAS(&hazard->active))
			continue;
		myhazard = hazard;
		return;
	}

	oldcount = g_atomic_int_exchange_and_add(&H, HAZARD_K);
	hazard = g_slice_new0(Hazard);
	hazard->active = TRUE;

	do {
		oldhead = hazards;
		hazard->next = oldhead;
	} while (!g_atomic_pointer_compare_and_exchange(
				(gpointer *)&hazards, oldhead, hazard));
	myhazard = hazard;
}

/*
static void
hazard_release(void)
{
	gint i;

	for (i = 0; i < (HAZARD_K - 1); i++) {
		myhazard->hp[i] = NULL;
	}
	myhazard->active = FALSE;
}
*/

static inline gint
hazard_compare(gconstpointer a,
               gconstpointer b)
{
	return (a == b) ? 0 : a - b;
}

#if 1
static gpointer
g_slist_pop(GSList   *list,
            gpointer *data)
{
	g_assert(data);

	if (!list) {
		*data = NULL;
		return NULL;
	}

	*data = list->data;
	return g_slist_delete_link(list, list);
}
#endif

static void
hazard_free(gpointer data)
{
	hazard_free_func(data);
}

static void
hazard_scan(Hazard *head)
{
	Hazard *hazard;
	GTree *plist;
	gpointer hptr = NULL;
	gint i;
	GSList *tmplist;

	/* Stage 1 */
	plist = g_tree_new(hazard_compare);
	hazard = head;
	while (hazard != NULL) {
		for (i = 0; i < (HAZARD_K - 1); i++) {
			hptr = g_atomic_pointer_get(&hazard->hp[i]);
			if (hptr != NULL) {
				g_tree_insert(plist, hptr, hptr);
			}
		}
		hazard = hazard->next;
	}

	/* Stage 2 */
	myhazard->rcount = 0;
	tmplist = myhazard->rlist;
	myhazard->rlist = NULL;
	tmplist = g_slist_pop(tmplist, &hptr);
	while (hptr != NULL) {
		if (g_tree_lookup(plist, hptr)) {
			myhazard->rlist = g_slist_prepend(myhazard->rlist, hptr);
			myhazard->rcount++;
		} else {
			hazard_free(hptr);
		}
		tmplist = g_slist_pop(tmplist, &hptr);
	}
	g_tree_unref(plist);
}

static void
hazard_help_scan(void)
{
	Hazard *hazard, *head;
	gpointer hptr;

	for (hazard = hazards; hazard; hazard = hazard->next) {
		if (hazard->active)
			continue;
		if (!g_atomic_int_compare_and_exchange(&hazard->active, FALSE, TRUE))
			continue;
		while (hazard->rcount > 0) {
			hazard->rlist = g_slist_pop(hazard->rlist, &hptr);
			hazard->rcount--;
			myhazard->rlist = g_slist_prepend(myhazard->rlist, hptr);
			myhazard->rcount++;
			head = hazards;
			if (myhazard->rcount >= (H + HAZARD_R)) {
				hazard_scan(head);
			}
		}
		hazard->active = FALSE;
	}
}

static void
hazard_retire(gpointer data)
{
	Hazard *head;

	if (!myhazard)
		hazard_acquire();

	myhazard->rlist = g_slist_prepend(myhazard->rlist, data);
	myhazard->rcount++;
	head = hazards;

	if (myhazard->rcount >= (H + HAZARD_R)) {
		hazard_scan(head);
		hazard_help_scan();
	}
}

#endif /* __HAZARD_H__ */
