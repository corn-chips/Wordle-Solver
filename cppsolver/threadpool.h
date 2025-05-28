#pragma once
#include <functional>
#include <mutex>
#include <vector>
#include <queue>
#include <thread>
#include <condition_variable>
#include <atomic>

struct JobRecipe {
    void* Parameter;
    void (*callback)(void* param, unsigned char* threadlocalstorage);

    JobRecipe(void* param, void(*cb)(void*, unsigned char*)) : Parameter{ param }, callback{ cb } {}
    JobRecipe() = default;
};

class ThreadPool
{
public:
    ThreadPool(int threads);
    ~ThreadPool();

    int getThreadCount();

    void QueueTask(const JobRecipe task);
    void QueueBatchTask(const JobRecipe* tasks, int num);
    void WaitCompletion();
    bool isBusy();

    void allocateThreadLocalStorage(const void* mem, size_t memsize); //copies across all threads
private:
    void ThreadLoop(int threadIndex);

    bool terminate;
    std::atomic<int> jobsCompleted;
    std::atomic<int> jobsAssigned;

    std::vector<std::thread> pool;
    std::vector<std::vector<unsigned char>> threadLocalStorage;

    std::condition_variable dispatch;

    std::mutex queuelock;
    std::queue<JobRecipe> jobs; //optimize with a lockless queue later maybe

    std::condition_variable jobsEmpty;
};