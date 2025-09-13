#ifndef TASK_QUEUE_H
#define TASK_QUEUE_H

#include <pthread.h>
#include <semaphore.h>

struct Task {
    int row_start;
    int row_end;
};

class TaskQueue {
public:
    TaskQueue(int size);
    ~TaskQueue();

    void push(Task t);
    Task pop();

private:
    Task* queue;
    int size;
    int head;
    int tail;
    int count;
    pthread_mutex_t lock;
    sem_t sem_items;  // tarefas disponíveis
    sem_t sem_space;  // espaço disponível
};

#endif
