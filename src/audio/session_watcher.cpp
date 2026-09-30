// ============================================================================
// Watcher de sessoes: um IAudioSessionManager2 por endpoint render ativo.
//  - RegisterSessionNotification  -> SessionEvent::Created;
//  - IAudioSessionEvents por sessao -> VolumeChanged / StateChanged / Removed;
//  - GetSessionEnumerator = estado inicial (SEM publicar Created).
// Os callbacks chegam em threads de sistema: trabalho minimo + publish.
// ============================================================================
#include "audio/session_watcher.h"

#include "audio/audio_util.h"

#include <algorithm>
#include <set>

#include <shlwapi.h>

namespace soundint::audio
{

namespace
{

// --- leituras auxiliares (COM) --------------------------------------------

std::wstring readInstanceId(IAudioSessionControl2* control)
{
    if (control == nullptr)
    {
        return L"";
    }
    LPWSTR value = nullptr;
    if (FAILED(control->GetSessionInstanceIdentifier(&value)) || value == nullptr)
    {
        return L"";
    }
    std::wstring result(value);
    CoTaskMemFree(value);
    return result;
}

std::wstring queryProcessPath(uint32_t pid)
{
    if (pid == 0)
    {
        return L"";
    }
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (process == nullptr)
    {
        return L"";
    }
    std::wstring result;
    std::vector<wchar_t> buffer(1024);
    DWORD size = static_cast<DWORD>(buffer.size());
    if (QueryFullProcessImageNameW(process, 0, buffer.data(), &size))
    {
        result.assign(buffer.data(), size);
    }
    CloseHandle(process);
    return result;
}

// Resolve "@caminho,-id" (SHLoadIndirectString); vazio em falha.
std::wstring resolveIndirectString(const std::wstring& raw)
{
    wchar_t buffer[512];
    buffer[0] = L'\0';
    const HRESULT hr = SHLoadIndirectString(raw.c_str(), buffer, 512, nullptr);
    if (FAILED(hr) || buffer[0] == L'\0')
    {
        return L"";
    }
    return buffer;
}

// Preenche SessionInfo completo. Chamado SEMPRE com m_mutex travado: as
// chamadas aqui sao apenas leituras (nao disparam notificacao de volta).
void buildSessionInfo(const std::wstring& deviceId, const std::wstring& instanceId,
                      IAudioSessionControl2* control, ISimpleAudioVolume* volume, SessionInfo& out)
{
    out = SessionInfo{};
    out.instanceId = instanceId;
    out.deviceId = deviceId;
    if (control == nullptr)
    {
        return;
    }

    out.systemSounds = (control->IsSystemSoundsSession() == S_OK);
    if (out.systemSounds)
    {
        out.pid = 0; // contrato: 0 = sons do sistema
    }
    else
    {
        DWORD pid = 0;
        if (SUCCEEDED(control->GetProcessId(&pid)) && pid != 0)
        {
            out.pid = pid;
            out.processPath = queryProcessPath(out.pid);
            out.processName = util::pathBaseNameLower(out.processPath);
        }
    }

    LPWSTR displayName = nullptr;
    if (SUCCEEDED(control->GetDisplayName(&displayName)) && displayName != nullptr)
    {
        const std::wstring raw(displayName);
        CoTaskMemFree(displayName);
        std::wstring source = raw;
        if (!raw.empty() && raw.front() == L'@')
        {
            const std::wstring resolved = resolveIndirectString(raw);
            source = resolved.empty() ? raw : resolved;
        }
        out.displayName = util::normalizeDisplayName(source, out.processName);
    }
    else
    {
        out.displayName = util::normalizeDisplayName(L"", out.processName);
    }

    AudioSessionState state = AudioSessionStateInactive;
    if (SUCCEEDED(control->GetState(&state)))
    {
        out.active = (state == AudioSessionStateActive);
    }

    if (volume != nullptr)
    {
        float level = 1.0f;
        BOOL muted = FALSE;
        if (SUCCEEDED(volume->GetMasterVolume(&level)))
        {
            out.volume = util::clampVolume(level);
        }
        if (SUCCEEDED(volume->GetMute(&muted)))
        {
            out.muted = (muted != FALSE);
        }
    }
}

} // namespace

// ============================================================================
// SessionEvents - IAudioSessionEvents de uma sessao
// ============================================================================

class SessionEvents final : public IAudioSessionEvents
{
  public:
    SessionEvents(std::weak_ptr<SessionWatcher> watcher, std::wstring instanceId)
        : m_watcher(std::move(watcher)), m_instanceId(std::move(instanceId))
    {
    }

    SessionEvents(const SessionEvents&) = delete;
    SessionEvents& operator=(const SessionEvents&) = delete;

    // --- IUnknown ---------------------------------------------------------
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
    {
        if (ppv == nullptr)
        {
            return E_POINTER;
        }
        *ppv = nullptr;
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IAudioSessionEvents))
        {
            *ppv = static_cast<IAudioSessionEvents*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return m_ref.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        const ULONG value = m_ref.fetch_sub(1, std::memory_order_acq_rel) - 1;
        if (value == 0)
        {
            delete this;
        }
        return value;
    }

    // --- IAudioSessionEvents ----------------------------------------------
    HRESULT STDMETHODCALLTYPE OnDisplayNameChanged(LPCWSTR newName, LPCGUID) override
    {
        const Guard guard(*this);
        if (const std::shared_ptr<SessionWatcher> watcher = m_watcher.lock())
        {
            watcher->handleDisplayNameChanged(m_instanceId, (newName != nullptr) ? newName : L"");
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnIconPathChanged(LPCWSTR, LPCGUID) override
    {
        const Guard guard(*this);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnSimpleVolumeChanged(float volume, BOOL muted, LPCGUID) override
    {
        const Guard guard(*this);
        if (const std::shared_ptr<SessionWatcher> watcher = m_watcher.lock())
        {
            watcher->handleVolumeChanged(m_instanceId, volume, (muted != FALSE));
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnChannelVolumeChanged(DWORD, float[], DWORD, LPCGUID) override
    {
        // Canal a canal nao afeta o master volume exibido no mixer.
        const Guard guard(*this);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnGroupingParamChanged(LPCGUID, LPCGUID) override
    {
        const Guard guard(*this);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnStateChanged(AudioSessionState state) override
    {
        const Guard guard(*this);
        if (const std::shared_ptr<SessionWatcher> watcher = m_watcher.lock())
        {
            watcher->handleStateChanged(m_instanceId, state == AudioSessionStateActive);
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnSessionDisconnected(AudioSessionDisconnectReason) override
    {
        const Guard guard(*this);
        if (const std::shared_ptr<SessionWatcher> watcher = m_watcher.lock())
        {
            watcher->handleSessionDisconnected(m_instanceId);
        }
        return S_OK;
    }

  private:
    // AddRef/Release enquanto o callback executa: se o teardown adiado
    // soltar a ultima referencia no meio da chamada, o objeto sobrevive.
    struct Guard
    {
        explicit Guard(SessionEvents& owner) : owner(owner) { owner.AddRef(); }
        ~Guard() { owner.Release(); }
        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;
        SessionEvents& owner;
    };

    std::weak_ptr<SessionWatcher> m_watcher;
    std::wstring m_instanceId;
    std::atomic<ULONG> m_ref{1};
};

// ============================================================================
// SessionNotification - IAudioSessionNotification de um endpoint
// ============================================================================

class SessionNotification final : public IAudioSessionNotification
{
  public:
    SessionNotification(std::weak_ptr<SessionWatcher> watcher, std::wstring deviceId)
        : m_watcher(std::move(watcher)), m_deviceId(std::move(deviceId))
    {
    }

    SessionNotification(const SessionNotification&) = delete;
    SessionNotification& operator=(const SessionNotification&) = delete;

    // --- IUnknown ---------------------------------------------------------
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
    {
        if (ppv == nullptr)
        {
            return E_POINTER;
        }
        *ppv = nullptr;
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IAudioSessionNotification))
        {
            *ppv = static_cast<IAudioSessionNotification*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return m_ref.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        const ULONG value = m_ref.fetch_sub(1, std::memory_order_acq_rel) - 1;
        if (value == 0)
        {
            delete this;
        }
        return value;
    }

    // --- IAudioSessionNotification -----------------------------------------
    HRESULT STDMETHODCALLTYPE OnSessionCreated(IAudioSessionControl* newSession) override
    {
        const Guard guard(*this);
        if (const std::shared_ptr<SessionWatcher> watcher = m_watcher.lock())
        {
            watcher->handleSessionCreated(m_deviceId, newSession);
        }
        return S_OK;
    }

  private:
    struct Guard
    {
        explicit Guard(SessionNotification& owner) : owner(owner) { owner.AddRef(); }
        ~Guard() { owner.Release(); }
        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;
        SessionNotification& owner;
    };

    std::weak_ptr<SessionWatcher> m_watcher;
    std::wstring m_deviceId;
    std::atomic<ULONG> m_ref{1};
};

// ============================================================================
// SessionWatcher
// ============================================================================

SessionWatcher::SessionWatcher(IMMDeviceEnumerator* enumerator, EventBus& bus)
    : m_enumerator(enumerator), m_bus(bus)
{
    if (m_enumerator != nullptr)
    {
        m_enumerator->AddRef();
    }
}

SessionWatcher::~SessionWatcher()
{
    stop();
    if (m_enumerator != nullptr)
    {
        m_enumerator->Release();
        m_enumerator = nullptr;
    }
}

bool SessionWatcher::start()
{
    if (m_started)
    {
        return true;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopping = false;
    }
    m_started = true;
    return syncInternal(/*initial=*/true);
}

bool SessionWatcher::sync()
{
    if (!m_started)
    {
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stopping)
        {
            return false;
        }
    }
    return syncInternal(/*initial=*/false);
}

void SessionWatcher::stop()
{
    std::map<std::wstring, EndpointRecord> endpoints;
    std::vector<DeadSession> dead;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopping = true;
        endpoints.swap(m_endpoints);
        dead.swap(m_dead);
    }

    // Teardown fora do mutex: o OS pode esperar callbacks em voo, e esses
    // callbacks precisam do mutex para terminar.
    for (auto& [deviceId, endpoint] : endpoints)
    {
        (void)deviceId;
        for (auto& [instanceId, record] : endpoint.sessions)
        {
            (void)instanceId;
            releaseDead(takeDead(record));
        }
        releaseEndpoint(endpoint);
    }
    for (const DeadSession& item : dead)
    {
        releaseDead(item);
    }
    m_started = false;
}

std::vector<SessionInfo> SessionWatcher::sessions(Flow flow)
{
    drainDead();
    std::vector<SessionInfo> out;
    if (flow != Flow::Render)
    {
        // Apenas os endpoints render sao observados (contrato: sessions(flow)).
        return out;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& [deviceId, endpoint] : m_endpoints)
    {
        (void)deviceId;
        out.reserve(out.size() + endpoint.sessions.size());
        for (const auto& [instanceId, record] : endpoint.sessions)
        {
            (void)instanceId;
            out.push_back(record.info);
        }
    }
    return out;
}

bool SessionWatcher::setVolume(const std::wstring& instanceId, float volume)
{
    drainDead();
    ISimpleAudioVolume* target = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        SessionRecord* record = findSessionLocked(instanceId);
        if (record == nullptr || record->volume == nullptr)
        {
            return false;
        }
        target = record->volume;
        target->AddRef(); // referencia propria: solta o mutex antes da chamada
    }

    const HRESULT hr = target->SetMasterVolume(util::clampVolume(volume), nullptr);
    target->Release();
    // O VolumeChanged volta pelo IAudioSessionEvents registrado.
    return SUCCEEDED(hr);
}

bool SessionWatcher::setMute(const std::wstring& instanceId, bool mute)
{
    drainDead();
    ISimpleAudioVolume* target = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        SessionRecord* record = findSessionLocked(instanceId);
        if (record == nullptr || record->volume == nullptr)
        {
            return false;
        }
        target = record->volume;
        target->AddRef();
    }

    const HRESULT hr = target->SetMute(mute ? TRUE : FALSE, nullptr);
    target->Release();
    return SUCCEEDED(hr);
}

// --- callbacks das threads de sistema --------------------------------------

void SessionWatcher::handleSessionCreated(const std::wstring& deviceId,
                                          IAudioSessionControl* control)
{
    if (control == nullptr)
    {
        return;
    }
    IAudioSessionControl2* control2 = nullptr;
    if (FAILED(control->QueryInterface(__uuidof(IAudioSessionControl2),
                                       reinterpret_cast<void**>(&control2))) ||
        control2 == nullptr)
    {
        return;
    }
    const std::wstring instanceId = readInstanceId(control2);
    if (!instanceId.empty())
    {
        // Dedup + publicacao de Created rodam dentro de createSession.
        createSession(deviceId, control2, instanceId, /*publish=*/true);
    }
    control2->Release();
}

void SessionWatcher::handleVolumeChanged(const std::wstring& instanceId, float volume, bool muted)
{
    SessionInfo snapshot;
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        SessionRecord* record = findSessionLocked(instanceId);
        if (record != nullptr)
        {
            record->info.volume = util::clampVolume(volume);
            record->info.muted = muted;
            snapshot = record->info;
            found = true;
        }
    }
    if (found)
    {
        publishSession(SessionChange::VolumeChanged, snapshot);
    }
}

void SessionWatcher::handleStateChanged(const std::wstring& instanceId, bool active)
{
    SessionInfo snapshot;
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        SessionRecord* record = findSessionLocked(instanceId);
        if (record != nullptr)
        {
            record->info.active = active;
            snapshot = record->info;
            found = true;
        }
    }
    if (found)
    {
        publishSession(SessionChange::StateChanged, snapshot);
    }
}

void SessionWatcher::handleDisplayNameChanged(const std::wstring& instanceId,
                                              const std::wstring& displayName)
{
    // Nao existe SessionChange para nome: atualiza em silencio e a UI
    // enxerga no proximo sessions().
    std::lock_guard<std::mutex> lock(m_mutex);
    SessionRecord* record = findSessionLocked(instanceId);
    if (record != nullptr)
    {
        record->info.displayName =
            util::normalizeDisplayName(displayName, record->info.processName);
    }
}

void SessionWatcher::handleSessionDisconnected(const std::wstring& instanceId)
{
    DeadSession dead;
    SessionInfo info;
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& [deviceId, endpoint] : m_endpoints)
        {
            (void)deviceId;
            const auto it = endpoint.sessions.find(instanceId);
            if (it != endpoint.sessions.end())
            {
                dead = takeDead(it->second);
                info = it->second.info;
                endpoint.sessions.erase(it);
                found = true;
                break;
            }
        }
        if (found)
        {
            m_dead.push_back(dead);
        }
    }
    // O Unregister/Release real roda na main thread (drainDead): nunca
    // dentro do proprio callback.
    if (found)
    {
        publishSession(SessionChange::Removed, info);
    }
}

// --- sincronizacao ---------------------------------------------------------

bool SessionWatcher::syncInternal(bool initial)
{
    std::vector<std::wstring> wanted;
    if (!listActiveRender(wanted))
    {
        util::debugFailure(L"IMMDeviceEnumerator::EnumAudioEndpoints", E_FAIL);
        return false;
    }
    const bool publish = !initial;

    // 1) endpoints que sairam do conjunto ativo.
    std::vector<EndpointRecord> removedEndpoints;
    std::vector<DeadSession> dead;
    std::vector<SessionInfo> removedSessions;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_endpoints.begin();
        while (it != m_endpoints.end())
        {
            if (std::find(wanted.begin(), wanted.end(), it->first) == wanted.end())
            {
                EndpointRecord endpoint = std::move(it->second);
                it->second = EndpointRecord{}; // zera antes de apagar (sem duplo release)
                for (auto& [instanceId, record] : endpoint.sessions)
                {
                    (void)instanceId;
                    removedSessions.push_back(record.info);
                    dead.push_back(takeDead(record));
                }
                removedEndpoints.push_back(std::move(endpoint));
                it = m_endpoints.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    for (EndpointRecord& endpoint : removedEndpoints)
    {
        releaseEndpoint(endpoint);
    }
    for (const DeadSession& item : dead)
    {
        releaseDead(item);
    }
    if (publish)
    {
        for (const SessionInfo& info : removedSessions)
        {
            publishSession(SessionChange::Removed, info);
        }
    }

    // 2) cria/fresca cada endpoint ativo e diffa as sessoes dele.
    for (const std::wstring& deviceId : wanted)
    {
        syncEndpoint(deviceId, publish);
    }

    // 3) pendencias deixadas por callbacks de desconexao.
    drainDead();
    return true;
}

void SessionWatcher::syncEndpoint(const std::wstring& deviceId, bool publish)
{
    IAudioSessionManager2* manager = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto it = m_endpoints.find(deviceId);
        if (it != m_endpoints.end())
        {
            manager = it->second.manager;
        }
    }
    if (manager == nullptr)
    {
        manager = setupEndpoint(deviceId);
        if (manager == nullptr)
        {
            return; // endpoint sumiu entre a enumeracao e o GetDevice
        }
    }

    // Snapshot do que ja existia: somente essas sessoes podem "sumir" aqui.
    // Sessoes criadas durante a enumeracao ficam de fora do diff (elas ja
    // entraram pelo OnSessionCreated e seriam removidas por engano).
    std::set<std::wstring> known;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto it = m_endpoints.find(deviceId);
        if (it == m_endpoints.end())
        {
            return;
        }
        for (const auto& [instanceId, record] : it->second.sessions)
        {
            (void)record;
            known.insert(instanceId);
        }
    }

    IAudioSessionEnumerator* enumerator = nullptr;
    if (FAILED(manager->GetSessionEnumerator(&enumerator)) || enumerator == nullptr)
    {
        util::debugFailure(L"IAudioSessionManager2::GetSessionEnumerator", E_FAIL);
        return;
    }

    std::set<std::wstring> found;
    int count = 0;
    enumerator->GetCount(&count);
    for (int index = 0; index < count; ++index)
    {
        IAudioSessionControl* control = nullptr;
        if (FAILED(enumerator->GetSession(index, &control)) || control == nullptr)
        {
            continue;
        }
        IAudioSessionControl2* control2 = nullptr;
        const HRESULT hr = control->QueryInterface(__uuidof(IAudioSessionControl2),
                                                   reinterpret_cast<void**>(&control2));
        control->Release();
        if (FAILED(hr) || control2 == nullptr)
        {
            continue;
        }
        const std::wstring instanceId = readInstanceId(control2);
        if (!instanceId.empty())
        {
            found.insert(instanceId);
            createSession(deviceId, control2, instanceId, publish); // dedup interno
        }
        control2->Release();
    }
    enumerator->Release();

    // Remove o que deixou de existir (apenas sessoes do snapshot).
    std::vector<DeadSession> dead;
    std::vector<SessionInfo> removedSessions;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto it = m_endpoints.find(deviceId);
        if (it != m_endpoints.end())
        {
            auto session = it->second.sessions.begin();
            while (session != it->second.sessions.end())
            {
                if (known.count(session->first) != 0 && found.count(session->first) == 0)
                {
                    removedSessions.push_back(session->second.info);
                    dead.push_back(takeDead(session->second));
                    session = it->second.sessions.erase(session);
                }
                else
                {
                    ++session;
                }
            }
        }
    }
    for (const DeadSession& item : dead)
    {
        releaseDead(item);
    }
    if (publish)
    {
        for (const SessionInfo& info : removedSessions)
        {
            publishSession(SessionChange::Removed, info);
        }
    }
}

IAudioSessionManager2* SessionWatcher::setupEndpoint(const std::wstring& deviceId)
{
    if (m_enumerator == nullptr)
    {
        return nullptr;
    }
    IMMDevice* device = nullptr;
    if (FAILED(m_enumerator->GetDevice(deviceId.c_str(), &device)) || device == nullptr)
    {
        return nullptr;
    }

    IAudioSessionManager2* manager = nullptr;
    HRESULT hr = device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr,
                                  reinterpret_cast<void**>(&manager));
    if (FAILED(hr) || manager == nullptr)
    {
        util::debugFailure(L"IMMDevice::Activate(IAudioSessionManager2)", hr);
        device->Release();
        return nullptr;
    }

    // Registra ANTES de publicar o registro: sessoes criadas nessa janela
    // sao resgatadas pela enumeracao logo em seguida.
    auto* notification = new SessionNotification(shared_from_this(), deviceId); // ref = 1
    hr = manager->RegisterSessionNotification(notification);
    if (FAILED(hr))
    {
        util::debugFailure(L"RegisterSessionNotification", hr);
        notification->Release();
        manager->Release();
        device->Release();
        return nullptr;
    }

    bool inserted = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_stopping)
        {
            EndpointRecord& record = m_endpoints[deviceId];
            if (record.manager == nullptr)
            {
                record.device = device;
                record.manager = manager;
                record.notification = notification;
                inserted = true;
            }
        }
    }
    if (inserted)
    {
        return manager;
    }

    manager->UnregisterSessionNotification(notification);
    notification->Release();
    manager->Release();
    device->Release();
    return nullptr;
}

bool SessionWatcher::createSession(const std::wstring& deviceId, IAudioSessionControl2* control,
                                   const std::wstring& instanceId, bool publish)
{
    // 1) Registra os eventos ANTES de publicar o registro. Mudancas nessa
    //    janela nao acham o registro e caiem no estado lido no passo 2, que
    //    e sempre posterior - nada se perde.
    ISimpleAudioVolume* volume = nullptr;
    control->QueryInterface(__uuidof(ISimpleAudioVolume), reinterpret_cast<void**>(&volume));

    auto* events = new SessionEvents(shared_from_this(), instanceId); // ref = 1
    const HRESULT hr = control->RegisterAudioSessionNotification(events);
    const bool registered = SUCCEEDED(hr);
    if (!registered)
    {
        util::debugFailure(L"IAudioSessionControl::RegisterAudioSessionNotification", hr);
        events->Release();
        events = nullptr;
    }

    // 2) Leitura do estado + insercao atomicas sob o mutex: um callback que
    //    chegue agora bloqueia e reescreve por cima, entao nao ha corrida.
    bool inserted = false;
    SessionInfo created;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_stopping)
        {
            const auto endpoint = m_endpoints.find(deviceId);
            if (endpoint != m_endpoints.end() && endpoint->second.sessions.count(instanceId) == 0)
            {
                SessionRecord record;
                record.info.instanceId = instanceId;
                record.info.deviceId = deviceId;
                buildSessionInfo(deviceId, instanceId, control, volume, record.info);
                control->AddRef(); // referencia propria do registro
                record.control = control;
                record.volume = volume; // o QI ja devolveu a referencia
                record.events = events;
                created = record.info;
                endpoint->second.sessions.emplace(instanceId, std::move(record));
                inserted = true;
            }
        }
    }

    if (!inserted)
    {
        if (registered)
        {
            control->UnregisterAudioSessionNotification(events);
        }
        if (events != nullptr)
        {
            events->Release();
        }
        if (volume != nullptr)
        {
            volume->Release();
        }
        return false;
    }

    if (publish)
    {
        publishSession(SessionChange::Created, created);
    }
    return true;
}

// --- infra -----------------------------------------------------------------

bool SessionWatcher::listActiveRender(std::vector<std::wstring>& out) const
{
    if (m_enumerator == nullptr)
    {
        return false;
    }
    IMMDeviceCollection* collection = nullptr;
    if (FAILED(m_enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection)) ||
        collection == nullptr)
    {
        return false;
    }

    UINT count = 0;
    collection->GetCount(&count);
    for (UINT index = 0; index < count; ++index)
    {
        IMMDevice* device = nullptr;
        if (FAILED(collection->Item(index, &device)) || device == nullptr)
        {
            continue;
        }
        LPWSTR id = nullptr;
        if (SUCCEEDED(device->GetId(&id)) && id != nullptr)
        {
            out.emplace_back(id);
            CoTaskMemFree(id);
        }
        device->Release();
    }
    collection->Release();
    return true;
}

SessionWatcher::SessionRecord* SessionWatcher::findSessionLocked(const std::wstring& instanceId)
{
    for (auto& [deviceId, endpoint] : m_endpoints)
    {
        (void)deviceId;
        const auto it = endpoint.sessions.find(instanceId);
        if (it != endpoint.sessions.end())
        {
            return &it->second;
        }
    }
    return nullptr;
}

void SessionWatcher::drainDead()
{
    std::vector<DeadSession> dead;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_dead.empty())
        {
            return;
        }
        dead.swap(m_dead);
    }
    for (const DeadSession& item : dead)
    {
        releaseDead(item);
    }
}

void SessionWatcher::publishSession(SessionChange what, const SessionInfo& info)
{
    SessionEvent event;
    event.what = what;
    event.session = info;
    m_bus.publish(event);
}

SessionWatcher::DeadSession SessionWatcher::takeDead(SessionRecord& record)
{
    DeadSession dead;
    dead.control = record.control;
    dead.volume = record.volume;
    dead.events = record.events;
    record.control = nullptr;
    record.volume = nullptr;
    record.events = nullptr;
    return dead;
}

void SessionWatcher::releaseDead(const DeadSession& dead)
{
    if (dead.control != nullptr && dead.events != nullptr)
    {
        dead.control->UnregisterAudioSessionNotification(dead.events);
    }
    if (dead.events != nullptr)
    {
        dead.events->Release();
    }
    if (dead.volume != nullptr)
    {
        dead.volume->Release();
    }
    if (dead.control != nullptr)
    {
        dead.control->Release();
    }
}

void SessionWatcher::releaseEndpoint(EndpointRecord& endpoint)
{
    if (endpoint.manager != nullptr && endpoint.notification != nullptr)
    {
        endpoint.manager->UnregisterSessionNotification(endpoint.notification);
    }
    if (endpoint.notification != nullptr)
    {
        endpoint.notification->Release();
    }
    if (endpoint.manager != nullptr)
    {
        endpoint.manager->Release();
    }
    if (endpoint.device != nullptr)
    {
        endpoint.device->Release();
    }
    endpoint.notification = nullptr;
    endpoint.manager = nullptr;
    endpoint.device = nullptr;
}

} // namespace soundint::audio
