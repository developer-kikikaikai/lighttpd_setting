/**
 * @file dp_mutex.h
 *    @brief  Utility mutex API for design pattern
**/
#ifndef DPUTIL_MUTEX_H_
#define DPUTIL_MUTEX_H_
/*! @name Lock/Unlock API */
#include <pthread.h>
/* @{ */
/*! lock */
void dputil_lock(void *handle);
/*! unlock */
void dputil_unlock(void *handle);
/*! lock macro, to care pthread_cancel */
#define DPUTIL_LOCK(lock) \
        dputil_lock(lock);\
        pthread_cleanup_push(dputil_unlock, lock);
/*! unlock macro, to care pthread_cancel */
#define DPUTIL_UNLOCK pthread_cleanup_pop(1);
/* @} */
#endif
