#include "threadpool/ThreadPool.h"

ThreadPool::ThreadPool(size_t thread_count) {
    for (size_t i = 0; i < thread_count; ++i) {
        threads_.emplace_back(&ThreadPool::worker, this);
    }
}

ThreadPool::~ThreadPool() {
    stop();
}

void ThreadPool::addTask(const std::function<void()>& task) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tasks_.push(task);
    }
    cond_.notify_one();
}

void ThreadPool::stop() {
    running_ = false;
    cond_.notify_all();
    for (auto& t : threads_) {
        if (t.joinable()) t.join();
    }
}

void ThreadPool::worker() {
    while (running_) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cond_.wait(lock, [this] { return !tasks_.empty() || !running_; });
            if (!running_ && tasks_.empty()) return;
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        task();
    }
}