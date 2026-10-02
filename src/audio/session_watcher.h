// ============================================================================
// Watcher de sessoes de audio por endpoint (IAudioSessionManager2).
// Nao e header de contrato - Track A (Wave 1).
//
// Modelo: um SessionWatcher (shared_ptr) por AudioService. Os objetos de
// callback (IAudioSessionNotification/IAudioSessionEvents) guardam um
// weak_ptr: um callback em thread de sistema nunca acessa um watcher ja
// destruido, e o watcher sobrevive enquanto algum callback estiver em voo.
//
// Regras de concorrencia:
//  - todo o estado mutavel fica sob m_mutex;
//  - registracao/unregistracao COM acontecem SEM o mutex (o OS pode esperar um
//    callback que precisa do mesmo mutex => deadlock); leituras COM de estado
//    podem rodar sob o mutex (nao disparam notificacao);
//  - RegisterAudioSessionNotification NUNCA roda dentro de OnSessionCreated
//    (contrato WASAPI: o manager segura o lock interno no callback; registrar
//    ali trava nele). O caminho do callback insere o registro com events
//    nulo, publica Created na hora (latencia do modal preservada) e enfileira
//    o instanceId em m_pendingRegistrations; a main thread registra depois,
//    via processPendingRegistrations() (chamado nos mesmos pontos que
//    drainDead: sessions/setVolume/setMute/sync);
//  - teardown disparado DENTRO de um callback vira pendencia (m_dead) e roda
//    na main thread, via drainDead().
// ============================================================================
#pragma once

#include "core/event_bus.h"
#include "core/types.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <audiopolicy.h>
#include <mmdeviceapi.h>

namespace soundint::audio
{

class SessionWatcher;
class SessionEvents;       // IAudioSessionEvents de uma sessao
class SessionNotification; // IAudioSessionNotification de um endpoint

class SessionWatcher : public std::enable_shared_from_this<SessionWatcher>
{
  public:
    // `enumerator` recebe um AddRef proprio. Criar sempre com std::make_shared.
    SessionWatcher(IMMDeviceEnumerator* enumerator, EventBus& bus);
    ~SessionWatcher();

    SessionWatcher(const SessionWatcher&) = delete;
    SessionWatcher& operator=(const SessionWatcher&) = delete;

    // Main thread. Monta os endpoints render ativos SEM publicar Created
    // (a UI consulta sessions() para o estado inicial).
    bool start();

    // Main thread. Diffa endpoints/sessoes por instanceId e publica
    // Created/Removed (nao duplica o que ja esta no mapa).
    bool sync();

    // Main thread. Reverte tudo; idempotente; nao publica nada.
    void stop();

    // Snapshot copiado (thread-safe). So o fluxo Render e observado.
    std::vector<SessionInfo> sessions(Flow flow);

    bool setVolume(const std::wstring& instanceId, float volume);
    bool setMute(const std::wstring& instanceId, bool mute);

    // --- chamados pelos objetos de callback (threads de sistema) -----------
    void handleSessionCreated(const std::wstring& deviceId, IAudioSessionControl* control);
    void handleVolumeChanged(const std::wstring& instanceId, float volume, bool muted);
    void handleStateChanged(const std::wstring& instanceId, bool active);
    void handleDisplayNameChanged(const std::wstring& instanceId, const std::wstring& displayName);
    void handleSessionDisconnected(const std::wstring& instanceId);

  private:
    struct SessionRecord
    {
        SessionInfo info;
        IAudioSessionControl2* control = nullptr; // referencia propria
        ISimpleAudioVolume* volume = nullptr;     // referencia propria
        SessionEvents* events = nullptr; // propria + registrada; nulo ate o registro adiado
    };

    struct EndpointRecord
    {
        IMMDevice* device = nullptr;                 // referencia propria
        IAudioSessionManager2* manager = nullptr;    // referencia propria
        SessionNotification* notification = nullptr; // propria + registrada
        std::map<std::wstring, SessionRecord> sessions;
    };

    // Teardown adiado: nao pode rodar dentro do proprio callback.
    struct DeadSession
    {
        IAudioSessionControl2* control = nullptr;
        ISimpleAudioVolume* volume = nullptr;
        SessionEvents* events = nullptr;
    };

    // Move os ponteiros do registro para uma pendencia (e zera o registro).
    static DeadSession takeDead(SessionRecord& record);
    // Unregister + Release (sempre fora do mutex).
    static void releaseDead(const DeadSession& dead);
    static void releaseEndpoint(EndpointRecord& endpoint);

    bool syncInternal(bool initial);
    bool listActiveRender(std::vector<std::wstring>& out) const;
    void syncEndpoint(const std::wstring& deviceId, bool publish);
    IAudioSessionManager2* setupEndpoint(const std::wstring& deviceId);
    bool createSession(const std::wstring& deviceId, IAudioSessionControl2* control,
                       const std::wstring& instanceId, bool publish, bool registerNow);
    SessionRecord* findSessionLocked(const std::wstring& instanceId);
    void drainDead();
    void processPendingRegistrations();
    void publishSession(SessionChange what, const SessionInfo& info);

    IMMDeviceEnumerator* m_enumerator = nullptr;
    EventBus& m_bus;
    mutable std::mutex m_mutex;
    std::map<std::wstring, EndpointRecord> m_endpoints;
    std::vector<DeadSession> m_dead;
    // instanceIds inseridos pelo callback (events nulo) aguardando o Register
    // na main thread. Falha de Register re-enfileira (retry); a entrada sai
    // quando o registro anexa ou a sessao morre.
    std::vector<std::wstring> m_pendingRegistrations;
    bool m_started = false;  // somente main thread
    bool m_stopping = false; // sempre com m_mutex travado
};

} // namespace soundint::audio
