#include "thread_pool.h"
#include <iostream>

ThreadPool::ThreadPool(int size) : pool_size(size), stop(false), active_count(0) {
    if (size <= 0) {
        throw std::invalid_argument("Thread pool size must be positive");
    }
    
    try {
        workers.reserve(pool_size);
        for (int i = 0; i < pool_size; ++i) {
            workers.emplace_back(&ThreadPool::worker_thread, this);
        }
        std::cout << "[ThreadPool] Created with " << pool_size << " worker threads" << std::endl;
    } catch (const std::exception& e) {
        // If thread creation fails, clean up and rethrow
        stop = true;
        condition.notify_all();
        for (auto& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        throw;
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();
    
    for (std::thread& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    std::cout << "[ThreadPool] All workers terminated" << std::endl;
}

void ThreadPool::enqueue(std::function<void()> task) {
    if (!task) {
        throw std::invalid_argument("Cannot enqueue null task");
    }
    
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        if (stop) {
            throw std::runtime_error("Cannot enqueue task on stopped thread pool");
        }
        tasks.push(std::move(task));
    }
    condition.notify_one();
}

int ThreadPool::get_active_count() const {
    return active_count.load();
}

int ThreadPool::get_pool_size() const {
    return pool_size;
}

size_t ThreadPool::get_queue_size() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(queue_mutex));
    return tasks.size();
}

void ThreadPool::worker_thread() {
    while (true) {
        std::function<void()> task;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            condition.wait(lock, [this] { return stop || !tasks.empty(); });
            
            if (stop && tasks.empty()) {
                return;
            }
            
            if (!tasks.empty()) {
                task = std::move(tasks.front());
                tasks.pop();
            }
        }
        
        if (task) {
            active_count++;
            try {
                task();
            } catch (const std::exception& e) {
                std::cerr << "[ThreadPool] Exception in worker thread: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "[ThreadPool] Unknown exception in worker thread" << std::endl;
            }
            active_count--;
        }
    }
}