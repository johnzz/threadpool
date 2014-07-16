#include "threadpool.h"

tpool_t * tpool_init(int thread_num);
void * tpool_work_func(void *data);
int tpool_create_thread(tpool_thread_info_t * thread, tpool_t * pool);
tpool_thread_info_t * tpool_add_thread(tpool_t *pool);
int tpool_add_workqueue(tpool_t *pool);
tpool_thread_info_t * tpool_add_thread(tpool_t *pool);
void *  manager_work_func(void *data);
tpool_t * create_pool(int thread_num);
int tpool_add_job(tpool_t *pool, void (*callback_function)(void *), void *arg);

#define MAX_JOB     100

#define MAX_THREADS 10
#define MIN_THREADS 5

#define IDLE_STATUS 2

#define POOL    1
#define QUEUE   2

#define RUN     1
#define IDLE    2

tpool_thread_info_t * get_thread_by_pid(tpool_t *pool, pthread_t  pid)
{
    tpool_thread_info_t * ret_thread;
    ret_thread = pool->threads;
    while (ret_thread) {
        if ( pthread_equal(ret_thread->pid, pid)) {
            return ret_thread;
        }
    }
    return  NULL;
}

int tpool_create_thread(tpool_thread_info_t * thread, tpool_t * pool)
{
    int ret;
    pthread_cond_init(&thread->cond, NULL);
    pthread_mutex_init(&thread->mutex, NULL);

    ret = pthread_create(&(thread->pid), NULL, tpool_work_func, (void *)pool);
    if ( ret != TPOOL_OK ) {
        printf("tpool_create_thread failed\n");
        return TPOOL_CREATE_THREAD_FAILED;
    }

    pthread_detach(thread->pid);
    thread->status = IDLE;    //idle
    return TPOOL_OK;
}

tpool_t * tpool_init(int thread_num)
{
    tpool_t  * pool;
    int index ,ret ;
    tpool_work_queue_t  * queue;
    tpool_thread_info_t *threads;//,*idle_threads;

    pool = malloc(sizeof(tpool_t));
    if ( pool == NULL ) {
        printf("tpool_init failed,malloc pool failed\n");
        return NULL;
    }

    pool->queue = malloc(sizeof(tpool_work_queue_t));
    if ( pool->queue == NULL ) {
        printf("tpool_init failed,malloc queue failed\n");
        return NULL ;
    }

    pool->queue->head = (tpool_job_t *)malloc(sizeof(struct tpool_job));
    if ( pool->queue->head == NULL ) {
        printf("tpool_init failed,malloc tpool_job_t failed\n");
        return NULL ;
    }

    threads = malloc(sizeof(tpool_thread_info_t));
    if ( threads == NULL ) {
        printf("tpool_init failed, malloc tpool_thread_info_t failed\n");
        return NULL;
    }

    threads = malloc(sizeof(tpool_thread_info_t));
    if ( threads == NULL ) {
        printf("tpool_init failed, malloc tpool_thread_info_t failed\n");
        return NULL;
    }


    // num init
    pool->current_num   = thread_num;
    pool->max_num       = MAX_THREADS;
    pool->min_num       = MIN_THREADS;
  //  pool->idle_num      = thread_num;

    //cond mutex init
    pthread_cond_init(&pool->queue_not_full, NULL);
    pthread_cond_init(&pool->queue_empty, NULL);
    pthread_cond_init(&pool->queue_not_empty, NULL);

    pthread_mutex_init(&(pool->queue->queue_mutex), NULL);
    pthread_mutex_init(&pool->tpool_mutex, NULL);

    //queue init
    pool->queue->head->next = NULL;
    pool->queue->curr_num = 0;
    pool->queue->max_num = 100;

    //pool init
    pool->queue     = NULL;
    pool->threads   = threads;
    //pool->idle_threads = idle_threads;
    pool->threads->next   = NULL;
    //pool->threads->next = NULL;

    return pool;
}

int tpool_list_add(void * head, void * current, int flag)
{
    if (head == NULL || current == NULL ) {
        return  TPOOL_ADD_FAILED;
    }
    if ( flag == POOL ) {
        tpool_t *pool = (tpool_t *)(head);
        tpool_thread_info_t * current_thread = (tpool_thread_info_t *) current;
        
        if ( pool->threads->next == NULL ) {
            pool->threads->next = current_thread;
            current_thread->next = NULL;
        } else {
            current_thread->next        = pool->threads->next;
            pool->threads->next    = current_thread;
        }
    }
    if ( flag == QUEUE ) {
        tpool_work_queue_t * queue = (tpool_work_queue_t *)(head);
        tpool_job_t * job = (tpool_job_t *)(current);

        if ( queue->head->next == NULL ) {
            queue->head->next = job;
            job->next = NULL ;
        } else {
            job->next = queue->head->next;
            queue->head->next = job;
        }
    }

    return TPOOL_OK;
}

void * tpool_list_del(void * head, int flag) {
    tpool_job_t * ret_job = NULL;
    if (flag == QUEUE ) {
        tpool_work_queue_t * job_queue = (tpool_work_queue_t *)head;
        if ( job_queue->head->next != NULL ) {
            ret_job = job_queue->head->next;
            job_queue->head->next = ret_job->next;
            job_queue->curr_num--;
        }

        return ret_job;
    }

}
tpool_t * create_pool(int thread_num)
{
    tpool_t * pool = NULL;
    int index, ret;
    int num = thread_num > MAX_THREADS ? MAX_THREADS :thread_num;
    pool = tpool_init(num);
    if ( pool == NULL ) {
        printf("create_pool:tpool_init failed\n");
        return NULL;
    }

    for(index=0; index < num; index++) {
        tpool_thread_info_t     *current_thread, *before_thread;
        current_thread  = malloc(sizeof(tpool_thread_info_t));
        if ( current_thread == NULL ) {
            printf("create_pool:malloc tpool_thread_info_t error\n");
            return NULL;
        }

        tpool_list_add((void *)pool, (void *)(current_thread), POOL);

        ret = tpool_create_thread(current_thread, pool);
        if ( ret != TPOOL_OK ) {
            printf("tpool_init create thread err! have created %d threads\n",(index+1));
        }
    }

    //manager thread init
    ret = pthread_create(&pool->manager_id, NULL, manager_work_func, pool);
    if ( ret != TPOOL_OK ) {
        printf("create_pool:pthread_create manager failed\n");
    }

    return pool;
}

int tpool_add_job(tpool_t *pool, void (*callback_function)(void *), void *arg)
{
    int ret;
    assert(callback_function != NULL );
    assert(arg != NULL );
    assert(pool != NULL );

    printf("test");
    pthread_mutex_lock(&(pool->queue->queue_mutex));
    while ( pool->queue->curr_num == pool->queue->max_num) {
        pthread_cond_wait(&(pool->queue_not_full), &(pool->tpool_mutex));
    }

    tpool_job_t * job = NULL ;
    job = malloc(sizeof(tpool_job_t));
    if ( job == NULL ) {
        printf("tpool_add_job: malloc job error\n");
        pthread_mutex_unlock(&(pool->queue->queue_mutex));
        return TPOOL_ADD_FAILED;
    }


    job->func = callback_function;
    job->arg = arg;
    if ( pool->queue->curr_num == 0 ) {
        pthread_cond_broadcast(&(pool->queue_not_empty));
    }

    ret = tpool_list_add(pool->queue, job, QUEUE);
    if ( ret < 0 ){
        printf("tpool_add_job:tpool_list_add failed\n");
        pthread_mutex_unlock(&(pool->queue->queue_mutex));
        return TPOOL_ADD_FAILED;
    }
    pool->queue->curr_num ++ ;

    pthread_mutex_unlock(&(pool->queue->queue_mutex));

    return TPOOL_OK;
}


tpool_thread_info_t * tpool_add_thread(tpool_t *pool)
{
    tpool_thread_info_t * new_thread;
    int ret;

    ret =  tpool_create_thread(new_thread, pool);
    if ( ret != TPOOL_OK ) {
        printf("tpool_add_thread:tpool_create_thread failed\n");
        return NULL;
    }

    pthread_mutex_lock(&(pool->tpool_mutex));
    ret = tpool_list_add((void *)pool, (void *)new_thread, POOL);
    if ( ret < 0 ) {
        printf("tpool_add_thread:tpool_list_add failed\n");
        return NULL;
    }

    pool->current_num ++;

    pthread_mutex_unlock(&(pool->tpool_mutex));

    return new_thread;
}

void *  manager_work_func(void *data)
{
    tpool_t * pool = (tpool_t * )data;
    tpool_thread_info_t * thread;
    pthread_t   pid;

    pthread_mutex_lock(&(pool->queue->queue_mutex));
    if (pool->queue->curr_num !=0 ) {
         pthread_cond_wait(&(pool->queue_empty), &(pool->queue->queue_mutex));
    }

    pthread_mutex_unlock(&(pool->queue->queue_mutex)); 
    printf("Current Thread NUM is %d \n",pool->current_num);

}

void * tpool_work_func(void *data)
{
    tpool_t * pool = (tpool_t * )data;
    tpool_job_t *job = NULL;
    tpool_thread_info_t * thread;
    pthread_t   pid;

    pid = pthread_self();
    thread = get_thread_by_pid(pool, pid);
    if ( thread == NULL ) {
        printf("tpool_work_func:get_thread_by_pid failed\n");
    }
    thread->status = RUN;

    for (;;) {
        pthread_mutex_lock(&(pool->queue->queue_mutex));
        if ( pool->queue->curr_num == 0 ) {
            pthread_cond_wait(&(pool->queue_not_empty), &(pool->queue->queue_mutex));
        }

        job = tpool_list_del(pool->queue,QUEUE);
        if ( job == NULL ){
            printf("tpool_work_func:tpool_list_del job failed\n");
        } else {
            if ( pool->queue->curr_num == 0 ) {
                pool->queue->head->next = NULL;
                pthread_cond_signal(&(pool->queue_empty));
            }
        }
        if (pool->queue->curr_num < pool->queue->max_num ) {
            pthread_cond_broadcast(&(pool->queue_not_full));
        }

        if ( job != NULL ) {
            (*(job->func))(job->arg);
            free(job);
            job = NULL;
        }

    }
}
void  my_process(void* arg){
    int val=*(int*)arg;
    printf("hello :%d\n",val);
    sleep(1);
    printf("%d exit!!\n",val);
}

int main(){
    int num=100;
    int workingnum[100];
    tpool_t *pool = create_pool(4);//creat_thread_pool(5,20);
    void * arg;

    tpool_job_t *worker =(tpool_job_t*)malloc(sizeof(tpool_job_t));
    worker->func=my_process;

    int i;
    for (i = 0; i < num; i++){
        arg = &i;
        printf("arg %d\n",(void *)arg);
        tpool_add_job(pool, my_process, arg);
    }
}
