extern "C"
{
#include "libavcodec/packet.h"
}

#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

#include <iostream>

#include "../engine/decoder.h"
#include "../engine/demuxer.h"
#include "../engine/queue.h"
#include "../utils/ffmpeg_deleter.h"

int demux_sander()
{
    using ptr_packet_t = std::unique_ptr<AVPacket, av_packet_deleter>;
    using ptr_frame_t  = std::unique_ptr<AVFrame, av_frame_deleter>;
    // ... 前置参数处理和初始化 ...

    // 1. 初始化线程池（建议至少 2 个线程，一个给 Demux，一个给 Decode）
    exec::static_thread_pool pool(4);
    auto                     sched = pool.get_scheduler();

    QueueAtomic<ptr_packet_t> video_packet_queue;
    QueueAtomic<ptr_packet_t> audio_packet_queue;
    QueueAtomic<ptr_frame_t>  video_frame_queue;

    Demuxer demux(video_packet_queue, audio_packet_queue, "../../../../ example.mp4");
    Decoder decode(video_packet_queue, video_frame_queue, demux.video_codecpar());

    // ... OpenGL / GLFW 初始化 ...

    // 2. 使用 stdexec 启动异步任务
    // 我们不希望阻塞主线程，因为主线程要跑渲染循环
    auto demux_sender  = demux.schedule_run(sched);
    auto decode_sender = stdexec::schedule(sched)
                       | stdexec::then(
                             [&]()
                             {
                                 decode.run();
                             });

    // 这里使用 start_detached 让他俩在后台跑起来
    // 或者使用 when_all 配合一个异步 scope
    stdexec::start_detached(std::move(demux_sender));
    stdexec::start_detached(std::move(decode_sender));

    // 3. 主线程渲染循环 (Sender/Receiver 分界点)
    // 渲染循环本身通常不建议封装进 stdexec 链条，因为 GLFW 要求在主线程操作窗口
    while (!quit)
    {
        // ... 原有的渲染逻辑 (glfwPollEvents, pop frame, sleep, render) ...
    }

    // 4. 清理
    demux.stop();
    decode.stop();
    // 线程池会在析构时自动 join，或者可以显式等待
    return 0;
}

int fun()
{
    // 1. 显式指定执行上下文和调度器
    // exec 是 stdexec 参考实现中提供的线程池扩展
    exec::static_thread_pool pool(8);
    stdexec::scheduler auto  sched = pool.get_scheduler();

    // 2. 纯粹的显式调用链
    // 注意：管道符 | 依赖于 ADL（参数相关查找），因此不需要对每个算子写完整命名空间，
    // 但为了绝对的明确性，我们在链条的起始点和转换点使用完整路径。
    auto work = stdexec::schedule(sched)
              | stdexec::then(
                    []()
                    {
                        return 100;
                    })
              | stdexec::then(
                    [](int val)
                    {
                        return val * val;
                    })
              | stdexec::let_value(
                    [](int val)
                    {
                        // 返回一个新的 sender
                        return stdexec::just(val + 1);
                    });

    // 3. 显式启动并处理结果
    // sync_wait 是一个阻塞调用，返回一个 std::optional<std::tuple<...>>
    auto result = stdexec::sync_wait(work);

    if (result.has_value())
    {
        // 使用结构化绑定获取 tuple 中的值
        auto [final_value] = result.value();
        std::cout << "Computed Result: " << final_value << std::endl;
    }

    return 0;
}
