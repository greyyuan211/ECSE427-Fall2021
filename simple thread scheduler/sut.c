#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <ucontext.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include "sut.h"
#include "queue/queue.h"

typedef struct thread_context
{
    ucontext_t *context;
    pid_t tid;
    bool isrunning;
} thread_context;

//global

//init number of thread and task
int threadNum, taskNum;
//change the number of C-EXEC here!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
const int cexecNum = 2; //<--------------------------------------
const int iexecNum = 1;

//queue method initialization
struct queue waitQ;
struct queue readyQ;
struct queue threadQ;
//array of parent thread context
struct thread_context *pContext[3] = {NULL, NULL, NULL};
//init mutex locks
pthread_mutex_t waitQLock, readyQLock, threadNumLock, taskNumLock;
//set the thread stack size here
const int tSize = 16384;

//context and queue helper methods

//method to init context
ucontext_t *initContext()
{
    ucontext_t *newContext = (ucontext_t *)malloc(sizeof(ucontext_t));
    newContext->uc_stack.ss_flags = 0;
    newContext->uc_link = 0;
    newContext->uc_stack.ss_sp = (char *)malloc(tSize);
    newContext->uc_stack.ss_size = tSize;

    getcontext(newContext);
    return newContext;
}

//method to add a task to the waiting queue
void addTaskToWaitQ(struct queue_entry *newTask)
{
    pthread_mutex_lock(&waitQLock);
    queue_insert_tail(&waitQ, newTask);
    pthread_mutex_unlock(&waitQLock);
}

//method that can append the new task to ready queue
void addTaskToReadyQ(struct queue_entry *newTask)
{
    pthread_mutex_lock(&readyQLock);
    queue_insert_tail(&readyQ, newTask);
    pthread_mutex_unlock(&readyQLock);
}

//method to get the context from parent thread
struct thread_context *getPThreadContext(pid_t thread_id)
{
    pthread_mutex_lock(&threadNumLock);
    struct thread_context *pThreadContext = NULL;
    for (int i = 0; i < cexecNum + iexecNum; i++)
    {
        if (pContext[i])
        {
            if (!(1 && pContext[i]->tid == thread_id))
                continue;
        }
        else
        {
            continue;
        }
        pThreadContext = pContext[i];
        break;
    }
    pthread_mutex_unlock(&threadNumLock);
    return pThreadContext;
}

//method to get the current thread id
pid_t getTid(void)
{
    return syscall(SYS_gettid);
}

// I-exct and C-exec

//I-exec: we should define the task processing details for I/O
void *IEXEC()
{
    //init current context
    ucontext_t *ccontext = initContext();
    //get the current thread id
    pid_t ctid = getTid();
    //malloc space for container of context
    thread_context *containerOfContext = (thread_context *)malloc(sizeof(thread_context));

    containerOfContext->tid = ctid;
    containerOfContext->context = ccontext;
    containerOfContext->isrunning = true;
    //adding context
    pthread_mutex_lock(&threadNumLock);
    pContext[threadNum++] = containerOfContext;
    pthread_mutex_unlock(&threadNumLock);
    //output if C-exec is running
    printf("******I-EXEC IS RUNNING******tid: %d\n", ctid);

    while (true)
    {
        if (containerOfContext->isrunning != false)
        {
        }
        else
        {
            if (!(taskNum != 0))
            {
                pthread_exit(NULL);
            }
        }
        // pop task from waiting queue
        struct queue_entry *Tnext;
        pthread_mutex_lock(&waitQLock);
        Tnext = queue_pop_head(&waitQ);
        pthread_mutex_unlock(&waitQLock);

        if (!(Tnext != NULL))
        {
            usleep(100);
        }

        else
        {
            swapcontext(ccontext, Tnext->data);
            //free up memory after use
            free(Tnext);
        }
    }
}

//C-exec: we should define the task processing details beside I/O
void *CEXEC()
{

    //init current context
    ucontext_t *ccontext = initContext();
    //get the current thread id
    pid_t ctid = getTid();
    //malloc space for container of context
    thread_context *containerOfContext = (thread_context *)malloc(sizeof(thread_context));

    containerOfContext->tid = ctid;
    containerOfContext->context = ccontext;
    containerOfContext->isrunning = true;
    //adding context
    pthread_mutex_lock(&threadNumLock);
    pContext[threadNum++] = containerOfContext;
    pthread_mutex_unlock(&threadNumLock);
    //output if C-exec is running
    printf("******C-EXEC IS RUNNING******tid: %d\n", ctid);

    while (true)
    {
        if (containerOfContext->isrunning != false)
        {
        }
        else
        {
            if (!(taskNum != 0))
            {
                pthread_exit(NULL);
            }
        }
        //pop the next task from ready queue
        struct queue_entry *Tnext;
        pthread_mutex_lock(&readyQLock);
        Tnext = queue_pop_head(&readyQ);
        pthread_mutex_unlock(&readyQLock);

        if (!(Tnext != NULL))
        {
            usleep(100);
        }

        else
        {
            swapcontext(ccontext, Tnext->data);
            //free up memory after use
            free(Tnext);
        }
    }
}

//sut library

//Init sut for user scheduler
void sut_init()
{
    printf("Initializing SUT library\n");
    // Give global variables.
    threadNum = 0;
    taskNum = 0;
    //initialize queues
    waitQ = queue_create();
    readyQ = queue_create();
    threadQ = queue_create();
    queue_init(&readyQ);
    queue_init(&waitQ);
    queue_init(&threadQ);
    //init queues and mutex locks
    pthread_mutex_init(&readyQLock, NULL);
    pthread_mutex_init(&waitQLock, NULL);
    pthread_mutex_init(&threadNumLock, NULL);
    pthread_mutex_init(&taskNumLock, NULL);

    printf("Trying to create kernel level threads\n");

    // Create kernel level threads
    //I-EXEC create and add
    pthread_t *iexec = (pthread_t *)malloc(sizeof(pthread_t));
    pthread_create(iexec, NULL, IEXEC, NULL);
    queue_insert_tail(&threadQ, queue_new_node(iexec));
    //C-EXEC create and add
    for (int i = 0; i < cexecNum; i++)
    {
        pthread_t *c_exe = (pthread_t *)malloc(sizeof(pthread_t));
        pthread_create(c_exe, NULL, CEXEC, NULL);
        queue_insert_tail(&threadQ, queue_new_node(c_exe));
    }

    printf("SUT has been initialized\n");
}

//create a thread using a task
//input a task and return whether created successfully
bool sut_create(sut_task_f task)
{
    printf("Start to create a new user thread\n");
    ucontext_t *newContext = initContext();
    makecontext(newContext, task, 0);
    addTaskToReadyQ(queue_new_node(newContext));
    printf("New user thread is successfully created\n");
    //increment task number
    pthread_mutex_lock(&taskNumLock);
    taskNum++;
    pthread_mutex_unlock(&taskNumLock);
    return 1;
}

//method to yield the running task
void sut_yield()
{
    ucontext_t *context = initContext();
    ucontext_t *pContext = getPThreadContext(getTid())->context;
    addTaskToReadyQ(queue_new_node(context));
    swapcontext(context, pContext);
}

//terminate the context
void sut_exit()
{
    //decreament task numbers
    pthread_mutex_lock(&taskNumLock);
    taskNum--;
    pthread_mutex_unlock(&taskNumLock);
    setcontext(getPThreadContext(getTid())->context);
    printf("exited(not seen if exit succeeded)\n");
}

//open a file
//input a file dir and output the file descriptor
int sut_open(char *fname)
{
    int descriptor = -1;
    ucontext_t *thisContext = initContext();
    //add the open context to the waiting queue
    addTaskToWaitQ(queue_new_node(thisContext));
    swapcontext(thisContext, getPThreadContext(getTid())->context);
    //open file
    descriptor = open(fname, O_RDWR | O_CREAT, 0777);
    thisContext = initContext();
    //wait for cexec after iexec finsihes
    addTaskToReadyQ(queue_new_node(thisContext));
    swapcontext(thisContext, getPThreadContext(getTid())->context);

    return descriptor;
}

//method to write on a file
//input the file descriptor, the buffer, and the size of the context
void sut_write(int fd, char *buf, int size)
{

    ucontext_t *thisContext = initContext();
    //add this context to the waiting queue
    addTaskToWaitQ(queue_new_node(thisContext));
    //continue to cexec
    swapcontext(thisContext, getPThreadContext(getTid())->context);

    //write on the file
    write(fd, buf, size);
    thisContext = initContext();
    //wait for cexec after iexec has finished
    addTaskToReadyQ(queue_new_node(thisContext));
    swapcontext(thisContext, getPThreadContext(getTid())->context);
}

//This function closes the file that is pointed by the file descriptor.
//input the file descriptor
void sut_close(int fd)
{

    ucontext_t *thisContext = initContext();
    //add this context to the waiting queue
    addTaskToWaitQ(queue_new_node(thisContext));
    //continue to cexec
    swapcontext(thisContext, getPThreadContext(getTid())->context);

    //close file
    close(fd);
    thisContext = initContext();
    //wait for cexec after iexec has finished
    addTaskToReadyQ(queue_new_node(thisContext));
    swapcontext(thisContext, getPThreadContext(getTid())->context);
}

//This function is provided a pre-allocated memory buffer. It is the responsibility of the
//calling program to allocate the memory. The size tells the function the max number of bytes
//that could be copied into the buffer. If the read operation is a success, it returns a non NULL
//value. On error it returns NULL.
char *sut_read(int fd, char *buf, int size)
{

    ucontext_t *thisContext = initContext();
    //add this context to the waiting queue
    addTaskToWaitQ(queue_new_node(thisContext));
    //continue to cexec
    swapcontext(thisContext, getPThreadContext(getTid())->context);
    char *result = "read successfully";
    //if read fails return null
    if (read(fd, buf, size) <= 0)
    {
        result = NULL;
    }

    thisContext = initContext();
    //wait for cexec after iexec has finished
    addTaskToReadyQ(queue_new_node(thisContext));
    swapcontext(thisContext, getPThreadContext(getTid())->context);

    return result;
}

// method to shutdown
void sut_shutdown()
{
    while (cexecNum + iexecNum > threadNum)
        usleep(100);

    pthread_mutex_lock(&threadNumLock);
    // terminate all kernel level threads
    for (int i = 0; i < cexecNum + iexecNum; i++)
    {
        pContext[i]->isrunning = false;
        printf("Thread terminated is %d i=%d\n", pContext[i]->tid, i);
    }
    pthread_mutex_unlock(&threadNumLock);
    // terminate the library after all kernel level threads are terminated
    while (queue_peek_front(&threadQ))
    {
        pthread_t *aThread = (pthread_t *)queue_pop_head(&threadQ)->data;
        pthread_join(*aThread, NULL);
    }
    exit(EXIT_SUCCESS);
}
