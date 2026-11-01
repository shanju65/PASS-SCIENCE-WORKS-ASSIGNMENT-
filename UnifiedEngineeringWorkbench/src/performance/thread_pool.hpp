#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

namespace uew {

class ThreadPool {
public:
    explicit ThreadPool(int n_workers) {
        if (n_workers < 1) throw std::runtime_error("Worker count must be at least 1.");
        workers_.reserve(n_workers);
        for (int i = 0; i < n_workers; ++i) {
            workers_.emplace_back([this] {
                for (;;) {
                    std::function<void()> job;
                    {
                        std::unique_lock<std::mutex> lock(mu_);
                        cv_.wait(lock, [this] { return stop_ || !jobs_.empty(); });
                        if (stop_ && jobs_.empty()) return;
                        job = std::move(jobs_.front());
                        jobs_.pop();
                    }
                    job();
                }
            });
        }
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template <typename F>
    auto submit(F&& f) -> std::future<decltype(f())> {
        using R = decltype(f());
        auto task = std::make_shared<std::packaged_task<R()>>(std::forward<F>(f));
        std::future<R> fut = task->get_future();
        {
            std::lock_guard<std::mutex> lock(mu_);
            if (stop_) throw std::runtime_error("Thread pool is shutting down.");
            jobs_.emplace([task] { (*task)(); });
        }
        cv_.notify_one();
        return fut;
    }

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(mu_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& t : workers_) t.join();
    }

    int size() const { return static_cast<int>(workers_.size()); }

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> jobs_;
    std::mutex mu_;
    std::condition_variable cv_;
    bool stop_ = false;
};

}  // namespace uew
