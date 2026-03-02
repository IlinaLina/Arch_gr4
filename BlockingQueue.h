#ifndef BLOCKING_QUEUE_H
#define BLOCKING_QUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>

template<typename T>
class BlockingQueue {
private:
    std::queue<T> queue;
    std::mutex mutex;
    std::condition_variable cv;
    bool stopped = false;

public:
    void push(T item) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            queue.push(item);
        }
        cv.notify_one();
    }

    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [this] { return !queue.empty() || stopped; });
        
        if (stopped && queue.empty()) {
            return false;
        }
        
        item = queue.front();
        queue.pop();
        return true;
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            stopped = true;
        }
        cv.notify_all();
    }

    bool empty() {
        std::lock_guard<std::mutex> lock(mutex);
        return queue.empty();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex);
        while (!queue.empty()) {
            queue.pop();
        }
    }
};

#endif