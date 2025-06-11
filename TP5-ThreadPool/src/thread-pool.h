/**
 * File: thread-pool.h
 * -------------------
 * Defines the ThreadPool class, which accepts a collection
 * of thunks (zero-argument functions that return nothing)
 * and schedules them in a FIFO manner to be executed by a fixed
 * number of worker threads.
 */

#ifndef _THREAD_POOL_
#define _THREAD_POOL_

#include <atomic>
#include <cstddef>
#include <functional>
#include <thread>
#include <vector>
#include <mutex>
#include "Semaphore.h"

/**
 * @brief Represents a worker in the thread pool.
 *
 * Contains the thread object, the task to execute,
 * availability status, and a semaphore to wait for work.
 */
struct worker_t {
    std::thread ts;
    std::function<void(void)> thunk;
    Semaphore ready;
    std::atomic<bool> available;

    worker_t() : ready(), available(true) {}

    worker_t(const worker_t&) = delete;
    worker_t& operator=(const worker_t&) = delete;
};

/**
 * @brief A fixed-size thread pool for scheduling and executing tasks concurrently.
 */
class ThreadPool {
  public:
    /**
     * @brief Constructs the thread pool with the specified number of worker threads.
     */
    ThreadPool(std::size_t numThreads);

    /**
     * @brief Schedules a task (thunk) to be executed by a worker thread.
     * @param thunk A function<void(void)> to execute.
     */
    void schedule(const std::function<void(void)>& thunk);

    /**
     * @brief Blocks until all scheduled tasks have completed execution.
     */
    void wait();

    /**
     * @brief Waits for all tasks to complete, then gracefully shuts down the pool.
     */
    ~ThreadPool();

  private:
    std::vector<worker_t> wts;               // Worker threads
    bool done;                               // Pool shutdown flag
    std::atomic<int> tasksInProgress{0};     // Number of tasks currently running
    std::thread dt;                          // Dispatcher thread
    std::mutex queueLock;                    // Protects the task queue

    void dispatcher();                       // Dispatcher function
    void worker(int id);                     // Worker function
};

#endif // _THREAD_POOL_
