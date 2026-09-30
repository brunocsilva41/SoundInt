#include "core/event_bus.h"

#include <algorithm>

namespace soundint {

void EventBus::setWakeup(Wakeup wakeup)
{
    std::lock_guard<std::mutex> lock(mutex_);
    wakeup_ = std::move(wakeup);
}

EventBus::SubscriptionId EventBus::subscribe(Handler handler)
{
    std::lock_guard<std::mutex> lock(mutex_);
    const SubscriptionId id = nextId_++;
    handlers_.emplace_back(id, std::move(handler));
    return id;
}

void EventBus::unsubscribe(SubscriptionId id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    handlers_.erase(std::remove_if(handlers_.begin(), handlers_.end(),
                                   [id](const auto& p) { return p.first == id; }),
                    handlers_.end());
}

void EventBus::publish(AppEvent ev)
{
    Wakeup wakeup;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back(std::move(ev));
        if (!wakePosted_) {
            wakePosted_ = true;
            wakeup = wakeup_;  // copia: chamada fora do lock
        }
    }
    if (wakeup) {
        wakeup();
    }
}

size_t EventBus::pump()
{
    std::deque<AppEvent> batch;
    std::vector<std::pair<SubscriptionId, Handler>> handlers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        batch.swap(queue_);
        wakePosted_ = false;
        handlers = handlers_;
    }

    for (const AppEvent& ev : batch) {
        for (const auto& [id, handler] : handlers) {
            (void)id;
            if (handler) {
                handler(ev);
            }
        }
    }

    // Se novos eventos chegaram durante o despacho, re-sinaliza a wakeup.
    bool needWake = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!queue_.empty() && !wakePosted_) {
            wakePosted_ = true;
            needWake = true;
        }
    }
    if (needWake) {
        Wakeup wakeup;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            wakeup = wakeup_;
        }
        if (wakeup) {
            wakeup();
        }
    }
    return batch.size();
}

}  // namespace soundint
