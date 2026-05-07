//
// Created by Qpromax on 2026/1/12.
//

#pragma once
// NOTE:   here are the testing units

#include "liteP.h"

#include <string>
#include <thread>
#include <atomic>
#include <cassert>
#include <optional>
#include <vector>
#include <iostream>
#include <random>

namespace EXCEPT{
    using namespace liteP;

    // TSDqueue testing
    template<typename T>
    T make_test_value(size_t v)
    {
        if constexpr (std::is_integral_v<T>)
        {
            return static_cast<T>(v);
        }
        else if constexpr (std::is_same_v<T, std::string>)
        {
            return "val_" + std::to_string(v);
        }
        else
        {
            return T{};
        }
    }
    template<typename T>
    void EXCEPT_TSDqueue(size_t scale)
    {
        using namespace std::chrono_literals;

        constexpr size_t producer_cnt = 4;
        constexpr size_t consumer_cnt = 4;
        constexpr size_t queue_cap    = 64;

        TSDeque<T> q(queue_cap);

        std::atomic<size_t> push_attempt{0};
        std::atomic<size_t> push_success{0};
        std::atomic<size_t> pop_success{0};

        // ========== Producer ==========
        auto producer = [&](size_t id)
        {
            for (size_t i = 0; i < scale; ++i)
            {
                ++push_attempt;
                if (q.push(make_test_value<T>(id * scale + i)))
                {
                    ++push_success;
                }

                // 竞争
                if ((i & 0xFF) == 0)
                {
                    std::this_thread::yield();
                }
            }
        };

        // ========== Consumer ==========
        auto consumer = [&]
        {
            while (true)
            {
                auto v = q.front_pop();
                if (v)
                {
                    ++pop_success;
                }
                else
                {
                    if (!q.on_off())
                    {
                        break;
                    }
                    std::this_thread::yield();
                }
            }
        };

        // ========== Launch ==========
        std::vector<std::thread> threads;

        for (size_t i = 0; i < producer_cnt; ++i)
            threads.emplace_back(producer, i);

        for (size_t i = 0; i < consumer_cnt; ++i)
            threads.emplace_back(consumer);

        // ========== 运行一段时间后 close ==========
        for (size_t i = 0; i < producer_cnt; ++i)
            threads[i].join();

        // 数据排空
        std::this_thread::sleep_for(100ms);
        q.close();

        for (size_t i = producer_cnt; i < threads.size(); ++i)
            threads[i].join();

        // ========== Report ==========
        std::cout << "\nTSDeque EXCEPT TEST\n";
        std::cout << " push attempt : " << push_attempt.load() << '\n';
        std::cout << " push success : " << push_success.load() << '\n';
        std::cout << " pop success  : " << pop_success.load() << '\n';
        std::cout << " queue size   : " << q.size() << '\n';

        assert(push_success >= pop_success);                 // 不可能多 pop
        assert(q.size() == push_success - pop_success);      // 无数据丢失
    }

    // Demux testing

    // Decode testing

    // Renderer testing

}