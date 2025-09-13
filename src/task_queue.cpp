#include "task_queue.h"
#include <iostream>
#include <cstdlib>

TaskQueue::TaskQueue(int s) : size(s), head(0), tail(0), count(0) {
    queue = new Task[size];
    pthread_mutex_init(&lock, nullptr);
    sem_init(&sem_items, 0, 0);
    sem_init(&sem_space, 0, size);
}

TaskQueue::~TaskQueue() {
    delete[] queue;
    pthread_mutex_destroy(&lock);
    sem_destroy(&sem_items);
    sem_destroy(&sem_space);
}

void TaskQueue::push(Task t) {
    sem_wait(&sem_space);
    pthread_mutex_lock(&lock);

    queue[tail] = t;
    tail = (tail + 1) % size;
    count++;

    pthread_mutex_unlock(&lock);
    sem_post(&sem_items);
}

Task TaskQueue::pop() {
    sem_wait(&sem_items);
    pthread_mutex_lock(&lock);

    Task t = queue[head];
    head = (head + 1) % size;
    count--;

    pthread_mutex_unlock(&lock);
    sem_post(&sem_space);

    return t;
}
