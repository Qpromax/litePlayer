#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "../engine/decoder.h"
#include "../engine/queue.h"

// stdexec
#include <stdexec/execution.hpp>

class MediaLogic
{
public:
    using pkt_t   = std::unique_ptr<AVPacket>;
    using frame_t = std::unique_ptr<AVFrame>;

public:
    MediaLogic(SPCQueue<pkt_t>& pkt_q, SPCQueue<frame_t>& frm_q, Decoder& decoder)
        : m_pkt_queue(pkt_q), m_frame_queue(frm_q), m_decoder(decoder)
    {
    }

    void start()
    {
        m_running = true;

        // 1. Packet consumption pipeline
        m_decode_task = exec::just()
                      | exec::then(
                            [this]()
                            {
                                this->packet_loop();
                            });

        // 2. Frame draining pipeline
        m_drain_task = exec::just()
                     | exec::then(
                           [this]()
                           {
                               this->frame_loop();
                           });

        // launch on pool
        exec::schedule(m_pool.get_scheduler())
            | exec::then(
                [this]
                {
                    start_loops();
                })
            | exec::sync_wait();
    }

    void stop()
    {
        m_running = false;
        m_pkt_queue.close();
    }

private:
    //------------------------------------------------------------------
    // Packet side (upstream → decoder)
    //------------------------------------------------------------------
    void packet_loop()
    {
        while (m_running)
        {
            auto pkt_opt = m_pkt_queue.pop();
            if (!pkt_opt)
            {
                break;
            }

            m_decoder.send_packet(std::move(*pkt_opt));
        }

        m_decoder.flush();
    }

    //------------------------------------------------------------------
    // Frame side (decoder → downstream)
    //------------------------------------------------------------------
    void frame_loop()
    {
        while (m_running)
        {
            auto frame_opt = m_decoder.receive_frame();

            if (!frame_opt)
            {
                // no frame available (EAGAIN)
                continue;
            }

            if (!m_frame_queue.push(std::move(*frame_opt)))
            {
                break;
            }
        }

        m_frame_queue.close();
    }

    //------------------------------------------------------------------
    // stdexec bootstrap
    //------------------------------------------------------------------
    void start_loops()
    {
        // 在 thread pool 上并发执行两个 loop
        exec::schedule(m_pool.get_scheduler())
            | exec::then(
                [this]
                {
                    packet_loop();
                })
            | exec::run_detached();

        exec::schedule(m_pool.get_scheduler())
            | exec::then(
                [this]
                {
                    frame_loop();
                })
            | exec::run_detached();
    }

private:
    //------------------------------------------------------------------
    // state
    //------------------------------------------------------------------
    bool m_running = false;

    SPCQueue<pkt_t>&   m_pkt_queue;
    SPCQueue<frame_t>& m_frame_queue;
    Decoder&           m_decoder;

    //------------------------------------------------------------------
    // execution
    //------------------------------------------------------------------
    exec::static_thread_pool m_pool {2};

    exec::task<> m_decode_task;
    exec::task<> m_drain_task;
};