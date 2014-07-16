
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <assert.h>

typedef int tpool_atomic_t;

struct tpool_job {
    void (*func)(void *data);
    void        * arg;
    struct tpool_job *next;
};

typedef struct tpool_job tpool_job_t;

typedef struct {
    tpool_job_t             *head;
    pthread_mutex_t         queue_mutex;
    tpool_atomic_t          curr_num;
    tpool_atomic_t          max_num;
}tpool_work_queue_t;


struct  tpool_thread_info_s{
    pthread_t               pid;
    int                     need_exit;
    int                     status;
    void (*func)(void *data);
    void                    *data;
    pthread_mutex_t         mutex;
    pthread_cond_t          cond;
    struct tpool_thread_info_s *next;//*prev;
};

typedef struct tpool_thread_info_s  tpool_thread_info_t;

struct tpool_s {
    tpool_atomic_t          current_num;
    tpool_atomic_t          max_num;
    tpool_atomic_t          min_num;
    //tpool_atomic_t          idle_num;

    pthread_cond_t          queue_not_full;
    pthread_cond_t          queue_empty;
    pthread_cond_t          queue_not_empty;
    pthread_mutex_t         tpool_mutex;
    pthread_t               manager_id;
    tpool_work_queue_t      *queue;
    tpool_thread_info_t     *threads;
    tpool_thread_info_t     *idle_threads;
};

enum {
    TPOOL_OK                    = 0,
    TPOOL_CREATE_THREAD_FAILED  = -1,
    TPOOL_INIT_FAILED           = -2,
    TPOOL_DESDROY_FAILED        = -3,
    TPOOL_MALLOC_FAILED         = -4,
    TPOOL_ADD_FAILED            = -5
};

typedef struct tpool_s tpool_t;

tpool_t * create_pool(int thread_num);
tpool_t * tpool_init(int thread_num);                                                                         
void * tpool_work_func(void *data);
int tpool_create_thread(tpool_thread_info_t * thread, tpool_t * pool);
tpool_thread_info_t * tpool_add_thread(tpool_t *pool);
int tpool_add_workqueue(tpool_t *pool);
tpool_thread_info_t * tpool_add_thread(tpool_t *pool);
void *  manager_work_func(void *data);
