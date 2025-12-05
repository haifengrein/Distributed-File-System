#ifndef DFS_THREAD_SAFE_QUEUE_H
#define DFS_THREAD_SAFE_QUEUE_H

#include <mutex>
#include <condition_variable>
#include <vector>
#include <algorithm>
#include <functional>

/**
 * ThreadSafeQueue
 * 
 * A generic thread-safe queue wrapper that supports:
 * - Blocking wait for new items (Producer-Consumer pattern)
 * - Thread-safe push and access
 */
template <typename T>
class ThreadSafeQueue {
private:
    std::vector<T> queue;
    std::mutex mtx;
    std::condition_variable cv;
    bool shutdown = false;

public:
    ThreadSafeQueue() = default;

    /**
     * Push an item into the queue and notify one waiter
     */
    void push(const T& item) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            queue.push_back(item);
        }
        cv.notify_one();
    }

    /**
     * Push an item (move semantics)
     */
    void push(T&& item) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            queue.push_back(std::move(item));
        }
        cv.notify_one();
    }

    /**
     * Wait until queue is not empty, then execute the processor function 
     * on the entire queue content (to allow batch processing if needed),
     * or process one by one. 
     * 
     * NOTE: This design mimics the existing logic where the server iterates 
     * over the *entire* vector of tags.
     */
    void process_all(std::function<void(T&)> processor) {
        std::unique_lock<std::mutex> lock(mtx);
        
        // Wait until queue has items or shutdown is requested
        cv.wait(lock, [this] { return !queue.empty() || shutdown; });

        if (shutdown && queue.empty()) return;

        // Process items
        for (auto& item : queue) {
            processor(item);
        }

        // Remove finished items (assuming processor marks them, or we clear all?)
        // The original code checked for `finished` flag.
        // To make this generic, we'll assume we keep items until explicitly removed,
        // OR we supply a cleanup predicate.
    }

    /**
     * Direct access with lock (for complex custom logic like the original erase-remove_if)
     */
    void with_lock(std::function<void(std::vector<T>&, std::condition_variable&)> action) {
        std::unique_lock<std::mutex> lock(mtx);
        action(queue, cv);
    }

    void signal_shutdown() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            shutdown = true;
        }
        cv.notify_all();
    }
};

#endif // DFS_THREAD_SAFE_QUEUE_H
