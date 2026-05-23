#pragma once
#ifndef _DEFERRED_EVENT_H
#define _DEFERRED_EVENT_H

#include <functional>

// 阶段二并行：worker 内推进 Animator 时遇到帧事件，把 callback 拷贝入此结构；
// 主线程串行 drain 时依次 invoke cb。
struct DeferredEvent {
    std::function<void()> cb;
};

#endif
