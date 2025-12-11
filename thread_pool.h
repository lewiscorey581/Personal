#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <stdexcept>

/**
 * Thread pool implementation for handling concurrent client connections
 * Uses a fixed number of worker threads to process tasks from a queue
 */
class ThreadPool {
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    
    std::mutex queue_mutex;
    std::condition_variable condition;
    std::atomic<bool> stop;
    std::atomic<int> active_count;
    
    int pool_size;
    
    void worker_thread();

public:
    explicit ThreadPool(int size);
    ~ThreadPool();
    
    // Delete copy constructor and assignment operator
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    
    // Enqueue a task to be executed by the thread pool
    void enqueue(std::function<void()> task);
    
    // Get the number of currently active worker threads
    int get_active_count() const;
    
    // Get the total size of the thread pool
    int get_pool_size() const;
    
    // Get the number of pending tasks in the queue
    size_t get_queue_size() const;
};

#endif