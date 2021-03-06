//fc_queue.c

#include <errno.h>
#include <pthread.h>
#include <inttypes.h>
#include "logger.h"
#include "shared_func.h"
#include "pthread_func.h"
#include "fc_queue.h"

#define FC_QUEUE_NEXT_PTR(queue, data) \
    *((void **)(((char *)data) + queue->next_ptr_offset))

int fc_queue_init(struct fc_queue *queue, const int next_ptr_offset)
{
	int result;

	if ((result=init_pthread_lock_cond_pair(&queue->lc_pair)) != 0)
	{
		return result;
	}

	queue->head = NULL;
	queue->tail = NULL;
    queue->next_ptr_offset = next_ptr_offset;
	return 0;
}

void fc_queue_destroy(struct fc_queue *queue)
{
    destroy_pthread_lock_cond_pair(&queue->lc_pair);
}

void fc_queue_push_ex(struct fc_queue *queue, void *data, bool *notify)
{
    PTHREAD_MUTEX_LOCK(&queue->lc_pair.lock);
	FC_QUEUE_NEXT_PTR(queue, data) = NULL;
	if (queue->tail == NULL) {
		queue->head = data;
        *notify = true;
	} else {
		FC_QUEUE_NEXT_PTR(queue, queue->tail) = data;
        *notify = false;
	}
	queue->tail = data;

    PTHREAD_MUTEX_UNLOCK(&queue->lc_pair.lock);
}

void *fc_queue_pop_ex(struct fc_queue *queue, const bool blocked)
{
	void *data;

    PTHREAD_MUTEX_LOCK(&queue->lc_pair.lock);
    do {
        data = queue->head;
        if (data == NULL) {
            if (!blocked) {
                break;
            }

            pthread_cond_wait(&queue->lc_pair.cond, &queue->lc_pair.lock);
            data = queue->head;
        }

        if (data != NULL) {
            queue->head = FC_QUEUE_NEXT_PTR(queue, data);
            if (queue->head == NULL) {
                queue->tail = NULL;
            }
        }
    } while (0);

    PTHREAD_MUTEX_UNLOCK(&queue->lc_pair.lock);
	return data;
}

void *fc_queue_pop_all_ex(struct fc_queue *queue, const bool blocked)
{
	void *data;

    PTHREAD_MUTEX_LOCK(&queue->lc_pair.lock);
    do {
        data = queue->head;
        if (data == NULL) {
            if (!blocked) {
                break;
            }

            pthread_cond_wait(&queue->lc_pair.cond, &queue->lc_pair.lock);
            data = queue->head;
        }

        if (data != NULL) {
            queue->head = queue->tail = NULL;
        }
    } while (0);

    PTHREAD_MUTEX_UNLOCK(&queue->lc_pair.lock);
	return data;
}

void fc_queue_push_queue_to_head_ex(struct fc_queue *queue,
        struct fc_queue_info *qinfo, bool *notify)
{
    if (qinfo->head == NULL) {
        *notify = false;
        return;
    }

    PTHREAD_MUTEX_LOCK(&queue->lc_pair.lock);
    FC_QUEUE_NEXT_PTR(queue, qinfo->tail) = queue->head;
    queue->head = qinfo->head;
    if (queue->tail == NULL) {
        queue->tail = qinfo->tail;
        *notify = true;
    } else {
        *notify = false;
    }
    PTHREAD_MUTEX_UNLOCK(&queue->lc_pair.lock);
}

void fc_queue_pop_to_queue(struct fc_queue *queue,
        struct fc_queue_info *qinfo)
{
    PTHREAD_MUTEX_LOCK(&queue->lc_pair.lock);
    if (queue->head != NULL) {
        qinfo->head = queue->head;
        qinfo->tail = queue->tail;
        queue->head = queue->tail = NULL;
    } else {
        qinfo->head = qinfo->tail = NULL;
    }
    PTHREAD_MUTEX_UNLOCK(&queue->lc_pair.lock);
}
