#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>

template <typename T>
struct TSDeque
{
private:
    std::deque<T>           m_data;
    mutable std::mutex      m_mtx;
    std::condition_variable m_cond;
    const size_t            m_max_size;
    bool                    m_is_running = true;

    // todo: use circle queue

public:
    explicit TSDeque(size_t max_size = 60) : m_max_size(max_size)
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
        std::unique_lock<std::mutex> lock(m_mtx);
        m_cond.wait(lock,
                    [&]
                    {
                        return m_data.size() < m_max_size || !m_is_running;
                    });
        if (!m_is_running)
        {
            return false;
        }
        m_data.emplace_back(std::forward<Y>(item));
        lock.unlock();
        m_cond.notify_one();
        return true;
    }

    std::optional<T> pop_front()
    {
        std::unique_lock<std::mutex> lock(m_mtx);
        m_cond.wait(lock,
                    [&]
                    {
                        return !m_data.empty() || !m_is_running;
                    });
        if (m_data.empty())
        {
            return std::nullopt;
        }
        std::optional<T> item = (std::move(m_data.front()));
        m_data.pop_front();
        lock.unlock();
        m_cond.notify_one();
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
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_data.size();
    }

    void clear()
    {
        std::unique_lock<std::mutex> lock(m_mtx);
        while (m_data.empty() == false)
        {
            m_data.pop_front();
        }
        lock.unlock();
        m_cond.notify_all();
    }

    void close()
    {
        // lock space
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            if (m_is_running == false)
            {
                return;
            }
            m_is_running = false;
        }
        m_cond.notify_all();
    }

    [[nodiscard]] bool is_running() const
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_is_running;
    }
};