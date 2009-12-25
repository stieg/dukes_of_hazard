/* lf-queue.h
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

#ifndef __LF_QUEUE_H__
#define __LF_QUEUE_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _LfQueue LfQueue;

GType    lf_queue_get_type (void) G_GNUC_CONST;
LfQueue* lf_queue_new      (void);
LfQueue* lf_queue_ref      (LfQueue *queue);
void     lf_queue_unref    (LfQueue *queue);
void     lf_queue_enqueue  (LfQueue *queue, gconstpointer data);
gpointer lf_queue_dequeue  (LfQueue *queue);

G_END_DECLS

#endif /* __LF_QUEUE_H__ */