#pragma once

#include <atomic>
#include <concepts>
#include <cstddef>
#include <optional>
#include <vector>

template <typename T>
struct SPCQueue
{
private:
    std::vector<T> m_data;
    const size_t   m_max_size;
    alignas(64) std::atomic<size_t> m_head_ato {0};
    alignas(64) std::atomic<size_t> m_tail_ato {0};
    std::atomic<bool> m_is_running_ato {true};

public:
    explicit SPCQueue(size_t max_size = 128) : m_max_size(max_size)
    {
        if ((m_max_size & (m_max_size - 1)) != 0)
        {
            throw std::invalid_argument("size must be power of two");
        }
        m_data.resize(m_max_size);
    }

    SPCQueue(const SPCQueue&)              = delete;
    SPCQueue& operator=(const SPCQueue&)   = delete;
    SPCQueue(SPCQueue&&)                   = delete;
    SPCQueue& operator=(SPCQueue&&)        = delete;
    auto      operator<=>(const SPCQueue&) = delete;

    ~SPCQueue()
    {
        close();
    }

    template <typename Y>
        requires std::constructible_from<T, Y&&>
    bool push(Y&& item)
    {
        size_t tail = m_tail_ato.load(std::memory_order_relaxed);
        size_t next = (tail + 1) & (m_max_size - 1);

        if (!m_is_running_ato.load(std::memory_order_acquire) || next == m_head_ato.load(std::memory_order_acquire))
        {
            return false;
        }
        m_data[tail] = std::forward<Y>(item);

        m_tail_ato.store(next, std::memory_order_release);
        return true;
    }

    std::optional<T> pop()
    {
        size_t head = m_head_ato.load(std::memory_order_relaxed);

        if (head == m_tail_ato.load(std::memory_order_acquire))
        {
            return std::nullopt;
        }
        std::optional<T> item = std::move(m_data[head]);

        m_head_ato.store((head + 1) & (m_max_size - 1), std::memory_order_release);
        return item;
    }

    bool push_batch()
    {
    }

    bool clear()
    {
        if (m_is_running_ato.load(std::memory_order_acquire))
        {
            return false;
        }
        size_t head = m_head_ato.load(std::memory_order_relaxed);
        size_t tail = m_tail_ato.load(std::memory_order_acquire);

        while (head != tail)
        {
            m_data[head] = T {};
            head         = (head + 1) & (m_max_size - 1);
        }

        m_head_ato.store(head, std::memory_order_release);
        m_tail_ato.store(tail, std::memory_order_release);
        return true;
    }

    void close()
    {
        if (m_is_running_ato.load(std::memory_order_acquire) == false)
        {
            return;
        }
        m_is_running_ato.store(false, std::memory_order_release);
    }

    [[nodiscard]] size_t size() const
    {
        auto tail = m_tail_ato.load(std::memory_order_acquire);
        auto head = m_head_ato.load(std::memory_order_acquire);
        return (tail - head) & (m_max_size - 1);
    }

    [[nodiscard]] bool running_status() const
    {
        return m_is_running_ato.load();
    }
};