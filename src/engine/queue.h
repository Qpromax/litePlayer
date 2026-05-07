#pragma once

#include <condition_variable>
#include <deque>
#include <optional>

template <typename T>
struct TSDeque
{
private:
    std::deque<T>           data_;
    mutable std::mutex      mtx_;
    std::condition_variable cond_;
    const size_t            max_size_;
    bool                    is_running_ = true;

    // todo: use circle queue

public:
    explicit TSDeque(size_t maxSize = 60) : max_size_(maxSize)
    {
    }

    TSDeque(const TSDeque&)              = delete;
    TSDeque& operator=(const TSDeque&)   = delete;
    TSDeque(TSDeque&&)                   = delete;
    TSDeque& operator=(TSDeque&&)        = delete;
    auto     operator<=>(const TSDeque&) = delete;

    ~TSDeque()
    {
        close();
    }

    // todo: add push_batch()
    template <typename Y>
        requires std::constructible_from<T, Y&&>
    bool push(Y&& item)
    {
        std::unique_lock<std::mutex> lock(mtx_);
        cond_.wait(lock,
                   [&]
                   {
                       return data_.size() < max_size_ || !is_running_;
                   });
        if (!is_running_)
        {
            return false;
        }
        data_.emplace_back(std::forward<Y>(item));
        lock.unlock();
        cond_.notify_one();
        return true;
    }

    std::optional<T> pop_front()
    {
        std::unique_lock<std::mutex> lock(mtx_);
        cond_.wait(lock,
                   [&]
                   {
                       return !data_.empty() || !is_running_;
                   });
        if (data_.empty())
        {
            return std::nullopt;
        }
        std::optional<T> item = (std::move(data_.front()));
        data_.pop_front();
        lock.unlock();
        cond_.notify_one();
        return item;
    }

    // T front_view() const
    // {
    //     std::lock_guard<std::mutex> lock(mtx_);
    //     if (data_.empty()) {
    //         return T();
    //     }
    //     return data_.front();
    // }

    [[nodiscard]] size_t size() const
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return data_.size();
    }

    void clear()
    {
        std::unique_lock<std::mutex> lock(mtx_);
        while (data_.empty() == false)
        {
            data_.pop_front();
        }
        lock.unlock();
        cond_.notify_all();
    }

    void close()
    {
        // lock space
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (is_running_ == false)
            {
                return;
            }
            is_running_ = false;
        }
        cond_.notify_all();
    }

    [[nodiscard]] bool is_running() const
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return is_running_;
    }
};