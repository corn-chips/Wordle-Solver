#include "threadpool.h"
#include "words.h"
#include <iostream>

ThreadPool::ThreadPool(int threads)
{
    this->terminate = false;

    for (int i = 0; i < threads; i++) {
        this->pool.emplace_back(&ThreadPool::ThreadLoop, this, i);
    }
    this->threadLocalStorage.reserve(threads);
    this->threadLocalStorage.resize(threads);
}
ThreadPool::~ThreadPool()
{
    WaitCompletion();
    this->terminate = true;

    this->dispatch.notify_all();

    for (int i = 0; i < this->pool.size(); i++) {
        this->pool[i].join();
    }
}

int ThreadPool::getThreadCount()
{
    return this->pool.size();
}

void ThreadPool::QueueTask(const JobRecipe task)
{
    {
        std::unique_lock<std::mutex> lock(queuelock);
        jobsAssigned++;
        jobs.push(task);
    }

    dispatch.notify_one();
}
void ThreadPool::QueueBatchTask(const JobRecipe* tasks, int num)
{
    {
        std::unique_lock<std::mutex> lock(queuelock);
        jobsAssigned += num;
        for (int i = 0; i < num; i++) {
            jobs.push(tasks[i]);
        }
    }

    dispatch.notify_all();
}

bool ThreadPool::isBusy()
{
    std::unique_lock<std::mutex> lock(queuelock);
    return !jobs.empty();
}

void ThreadPool::WaitCompletion()
{
    std::unique_lock<std::mutex> lock(queuelock);
    this->jobsEmpty.wait(lock, [this] { return jobs.empty(); });
}
void ThreadPool::ThreadLoop(int threadIndex)
{
    while (true)
    {
        JobRecipe task;

        {
            std::unique_lock<std::mutex> lock(queuelock);
            if (jobs.empty() && (jobsAssigned == jobsCompleted)) {
                jobsEmpty.notify_all();
            }

            dispatch.wait(lock, [this] { return !jobs.empty() || terminate; });
            if (terminate) return;

            task = jobs.front();
            jobs.pop();
        }

        task.callback(task.Parameter, this->threadLocalStorage[threadIndex].data());
        jobsCompleted++;

    }
}

void ThreadPool::allocateThreadLocalStorage(const void* mem, size_t size)
{
    for (int i = 0; i < this->pool.size(); i++) {
        this->threadLocalStorage[i].reserve(size);
        this->threadLocalStorage[i].resize(size);
        std::memcpy(this->threadLocalStorage[i].data(), mem, size);
    }
}