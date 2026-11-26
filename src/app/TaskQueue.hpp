#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace blitzdj::app
{
    class MainThread
    {
    public:
        void post(std::function<void()> job)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            jobs_.push_back(std::move(job));
        }

        void drain()
        {
            std::vector<std::function<void()>> jobs;

            {
                std::lock_guard<std::mutex> lock(mutex_);
                jobs.swap(jobs_);
            }

            for (auto& job : jobs)
            {
                job();
            }
        }

    private:
        std::mutex mutex_;
        std::vector<std::function<void()>> jobs_;
    };

    class TaskQueue
    {
    public:
        explicit TaskQueue(int threads = 1)
        {
            for (int i = 0; i < threads; ++i)
            {
                workers_.emplace_back([this] { run(); });
            }
        }

        ~TaskQueue()
        {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                stopping_ = true;
                jobs_.clear();
            }

            condition_.notify_all();

            for (auto& worker : workers_)
            {
                if (worker.joinable())
                {
                    worker.join();
                }
            }
        }

        TaskQueue(const TaskQueue&) = delete;
        TaskQueue& operator=(const TaskQueue&) = delete;

        void post(std::function<void()> job)
        {
            {
                std::lock_guard<std::mutex> lock(mutex_);

                if (stopping_)
                {
                    return;
                }

                jobs_.push_back(std::move(job));
            }

            condition_.notify_one();
        }

        std::size_t pending() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return jobs_.size();
        }

    private:
        mutable std::mutex mutex_;
        std::condition_variable condition_;
        std::deque<std::function<void()>> jobs_;
        std::vector<std::thread> workers_;
        bool stopping_ = false;

        void run()
        {
            while (true)
            {
                std::function<void()> job;

                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    condition_.wait(lock, [this] { return stopping_ || !jobs_.empty(); });

                    if (stopping_)
                    {
                        return;
                    }

                    job = std::move(jobs_.front());
                    jobs_.pop_front();
                }

                job();
            }
        }
    };
}
