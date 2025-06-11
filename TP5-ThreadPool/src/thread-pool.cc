/**
 * File: thread-pool.cc
 * --------------------
 * Implements the ThreadPool class.
 */

#include "thread-pool.h"
#include <queue>
#include <mutex>
#include <functional>
#include <thread>

// Global shared resources
static std::queue<std::function<void(void)>> taskQueue;
static std::mutex queueMutex;
static Semaphore taskAvailable(0); // Dispatcher waits on this

ThreadPool::ThreadPool(std::size_t numThreads)
    : wts(numThreads), done(false), tasksInProgress(0) {

    // Launch worker threads
    for (std::size_t i = 0; i < numThreads; ++i) {
        wts[i].available = true;
        wts[i].thunk = nullptr;
        wts[i].ts = std::thread([this, i] { worker(i); });
    }

    // Launch dispatcher thread
    dt = std::thread([this] { dispatcher(); });
}

void ThreadPool::schedule(const std::function<void(void)>& thunk) {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        taskQueue.push(thunk);
    }
    tasksInProgress++;
    taskAvailable.signal();
}

void ThreadPool::dispatcher() {
    while (true) {
        taskAvailable.wait();

        if (done) break;

        std::function<void(void)> task;
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (!taskQueue.empty()) {
                task = taskQueue.front();
                taskQueue.pop();
            }
        }

        if (!task) continue;

        bool assigned = false;
        while (!assigned) {
            for (std::size_t i = 0; i < wts.size(); ++i) {
                if (wts[i].available.exchange(false)) {
                    wts[i].thunk = task;
                    wts[i].ready.signal();
                    assigned = true;
                    break;
                }
            }
            if (!assigned) std::this_thread::yield();
        }
    }
}

void ThreadPool::worker(int id) {
    while (true) {
        wts[id].ready.wait();

        if (done) break;

        if (wts[id].thunk) {
            wts[id].thunk();
            wts[id].thunk = nullptr;
            tasksInProgress--;
        }

        wts[id].available = true;
    }
}

void ThreadPool::wait() {
    while (tasksInProgress > 0) {
        std::this_thread::yield();
    }
}

ThreadPool::~ThreadPool() {
    wait();
    done = true;

    // Wake up workers and dispatcher
    for (auto& w : wts) {
        w.ready.signal();
    }
    taskAvailable.signal();

    for (auto& w : wts) {
        if (w.ts.joinable()) {
            w.ts.join();
        }
    }

    if (dt.joinable()) {
        dt.join();
    }
}
