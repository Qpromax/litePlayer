#pragma once

#include <concepts>
#include <cstddef>
#include <optional>
#include <vector>

template <typename T>
class RingBuffer
{
private:
    std::vector<T> m_data;
    const size_t   m_mask;

public:
    explicit RingBuffer(size_t max_size) : m_mask(max_size - 1)
    {
        if ((max_size & m_mask) != 0)
        {
            throw std::invalid_argument("Size must be power of 2");
        }
        m_data.resize(max_size);
    }

    RingBuffer(const RingBuffer&)              = delete;
    RingBuffer& operator=(const RingBuffer&)   = delete;
    RingBuffer(RingBuffer&&)                   = delete;
    RingBuffer& operator=(RingBuffer&&)        = delete;
    auto        operator<=>(const RingBuffer&) = delete;

    ~RingBuffer() = default;

    T& operator[](size_t index)
    {
        return m_data[index];
    }

    const T& operator[](size_t index) const
    {
        return m_data[index];
    }

    inline size_t next_index(size_t current) const
    {
        return (current + 1) & m_mask;
    }

    inline size_t calc_size(size_t head, size_t tail) const
    {
        return (tail - head) & m_mask;
    }

    void set_item(size_t index, T&& item)
    {
        m_data[index] = std::move(item);
    }

    T move_item(size_t index)
    {
        return std::move(m_data[index]);
    }

    void clear_range(size_t head, size_t tail)
    {
        while (head != tail)
        {
            m_data[head] = T {};
            head         = next_index(head);
        }
    }

    size_t get_size() const
    {
        return m_mask;
    }
};

//

//
#include <condition_variable>
#include <mutex>

template <typename T>
class QueueMutex
{
private:
    RingBuffer<T>           m_data;
    mutable std::mutex      m_mtx;
    std::condition_variable m_cond;
    size_t                  m_head       = 0;
    size_t                  m_tail       = 0;
    bool                    m_is_running = true;

public:
    explicit QueueMutex(size_t max_size = 128) : m_data(max_size)
    {
    }

    QueueMutex(const QueueMutex&)              = delete;
    QueueMutex& operator=(const QueueMutex&)   = delete;
    QueueMutex(QueueMutex&&)                   = delete;
    QueueMutex& operator=(QueueMutex&&)        = delete;
    auto        operator<=>(const QueueMutex&) = delete;

    ~QueueMutex()
    {
        close();
    }

    template <typename Y>
        requires std::constructible_from<T, Y&&>
    bool push(Y&& item)
    {
        std::unique_lock<std::mutex> lock(m_mtx);

        m_cond.wait(lock,
                    [&]
                    {
                        size_t next = m_data.next_index(m_tail);
                        return next != m_head || !m_is_running;
                    });

        if (!m_is_running)
        {
            return false;
        }

        m_data.set_item(m_tail, std::forward<Y>(item));
        m_tail = m_data.next_index(m_tail);

        lock.unlock();
        m_cond.notify_one();
        return true;
    }

    size_t push_batch(std::vector<T>& items)
    {
        std::unique_lock<std::mutex> lock(m_mtx);
        size_t                       count = 0;

        for (auto& item : items)
        {
            size_t next = m_data.next_index(m_tail);
            if (next == m_head || !m_is_running)
            {
                break;
            }

            m_data.set_item(m_tail, std::move(item));
            m_tail = next;
            count++;
        }

        if (count > 0)
        {
            lock.unlock();
            m_cond.notify_all();
        }
        return count;
    }

    std::optional<T> pop()
    {
        std::unique_lock<std::mutex> lock(m_mtx);

        m_cond.wait(lock,
                    [&]
                    {
                        return m_head != m_tail || !m_is_running;
                    });

        if (m_head == m_tail)
        {
            return std::nullopt;
        }

        T item = m_data.move_item(m_head);
        m_head = m_data.next_index(m_head);

        lock.unlock();
        m_cond.notify_one();
        return item;
    }

    [[nodiscard]] size_t size() const
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_data.calc_size(m_head, m_tail);
    }

    void clear()
    {
        std::unique_lock<std::mutex> lock(m_mtx);
        m_data.clear_range(m_head, m_tail);
        m_head = 0;
        m_tail = 0;
        lock.unlock();
        m_cond.notify_all();
    }

    void close()
    {
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            if (!m_is_running)
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

//
#include <atomic>

template <typename T>
class QueueAtomic
{
private:
    RingBuffer<T> m_data;
    alignas(64) std::atomic<size_t> m_head_o {0};
    alignas(64) std::atomic<size_t> m_tail_o {0};
    alignas(64) std::atomic<bool> m_is_running_o {true};

public:
    explicit QueueAtomic(size_t size = 128) : m_data(size)
    {
    }

    QueueAtomic(const QueueAtomic&)              = delete;
    QueueAtomic& operator=(const QueueAtomic&)   = delete;
    QueueAtomic(QueueAtomic&&)                   = delete;
    QueueAtomic& operator=(QueueAtomic&&)        = delete;
    auto         operator<=>(const QueueAtomic&) = delete;

    ~QueueAtomic()
    {
        close();
    }

    template <typename Y>
        requires std::constructible_from<T, Y&&>
    bool push(Y&& item)
    {
        size_t tail;
        size_t next;

        do
        {
            tail = m_tail_o.load(std::memory_order_acquire);
            next = m_data.next_index(tail);

            // 检查队列是否已满或已停止
            // 注意：此处 load head 必须用 acquire 确保看到消费者的最新进度
            if (!m_is_running_o.load(std::memory_order_acquire)
                || next == m_head_o.load(std::memory_order_acquire))
            {
                return false;
            }

            // 尝试“认领”这个 tail 索引。如果失败，说明其他生产者抢先了一步
        }
        while (!m_tail_o.compare_exchange_weak(
            tail, next, std::memory_order_release, std::memory_order_relaxed));

        // 认领成功后，写入数据
        // 注意：在多生产者环境下，这要求 RingBuffer 的槽位能处理并发写入（或者通过索引解耦）
        m_data.set_item(tail, std::forward<Y>(item));

        return true;
    }

    size_t push_batch(std::vector<T>& items)
    {
        size_t t     = m_tail_o.load(std::memory_order_relaxed);
        size_t count = 0;
        for (auto& item : items)
        {
            size_t next = m_data.next_index(t);
            if (next == m_head_o.load(std::memory_order_acquire))
            {
                break;
            }

            m_data.set_item(t, std::move(item));
            t = next;
            ++count;
        }
        m_tail_o.store(t, std::memory_order_release);
        return count;
    }

    std::optional<T> pop()
    {
        size_t head;
        size_t next;

        do
        {
            head = m_head_o.load(std::memory_order_acquire);

            // 检查队列是否为空
            // 注意：此处 load tail 必须用 acquire 确保看到生产者的最新进度
            if (head == m_tail_o.load(std::memory_order_acquire))
            {
                return std::nullopt;
            }

            next = m_data.next_index(head);

            // 尝试“认领”这个 head 索引。如果失败，说明其他消费者抢先拿走了这笔数据
        }
        while (!m_head_o.compare_exchange_weak(
            head, next, std::memory_order_release, std::memory_order_relaxed));

        // 认领成功后，移动数据
        return m_data.move_item(head);
    }

    size_t pop_batch(std::vector<T>& out_items, size_t max_count)
    {
        size_t h     = m_head_o.load(std::memory_order_relaxed);
        size_t t     = m_tail_o.load(std::memory_order_acquire);
        size_t count = 0;

        while (h != t && count < max_count)
        {
            out_items.push_back(m_data.move_item(h));
            h = m_data.next_index(h);
            ++count;
        }

        if (count > 0)
        {
            m_head_o.store(h, std::memory_order_release);
        }
        return count;
    }

    // todo: may cause racing
    bool clear()
    {
        if (m_is_running_o.load(std::memory_order_relaxed))
        {
            return false;
        }

        size_t h = m_head_o.load(std::memory_order_relaxed);
        size_t t = m_tail_o.load(std::memory_order_relaxed);

        m_data.clear_range(h, t);

        m_head_o.store(0, std::memory_order_relaxed);
        m_tail_o.store(0, std::memory_order_relaxed);
        return true;
    }

    void close()
    {
        if (m_is_running_o.load(std::memory_order_acquire) == false)
        {
            return;
        }
        m_is_running_o.store(false, std::memory_order_release);
    }

    [[nodiscard]] size_t size() const
    {
        auto tail = m_tail_o.load(std::memory_order_acquire);
        auto head = m_head_o.load(std::memory_order_acquire);
        return (tail - head) & m_data.get_size();
    }

    [[nodiscard]] bool running_status() const
    {
        return m_is_running_o.load();
    }
};