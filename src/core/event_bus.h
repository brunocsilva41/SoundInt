// ============================================================================
// CONTRACT — fila de eventos thread-safe. CONGELADO na Wave 0.
// Publishers (threads de audio) chamam publish(); a UI dreina na main thread
// com pump(). publish() aciona o wakeup registrado (PostMessage na janela
// hidden do app) quando a fila transiciona de vazia para nao-vazia.
// ============================================================================
#pragma once

#include "core/types.h"

#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <utility>
#include <vector>

namespace soundint {

class EventBus {
public:
    using Handler = std::function<void(const AppEvent&)>;
    using Wakeup = std::function<void()>;
    using SubscriptionId = uint64_t;

    EventBus() = default;
    ~EventBus() = default;
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    // Instalado uma vez pelo app (main thread) antes do start dos watchers.
    void setWakeup(Wakeup wakeup);

    // Main thread.
    SubscriptionId subscribe(Handler handler);
    void unsubscribe(SubscriptionId id);

    // Qualquer thread. Encoda e dispara wakeup se a fila estava vazia.
    void publish(AppEvent ev);

    // Main thread: dreina e despacha todos os eventos enfileirados.
    // Retorna quantos foram despachados.
    size_t pump();

private:
    std::mutex mutex_;
    std::deque<AppEvent> queue_;
    std::vector<std::pair<SubscriptionId, Handler>> handlers_;
    Wakeup wakeup_;
    SubscriptionId nextId_ = 1;
    bool wakePosted_ = false;
};

}  // namespace soundint
