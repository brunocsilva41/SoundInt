// ============================================================================
// Instancia unica do SoundInt (Track H). Mutex nomeado + aviso a janela da
// primeira instancia quando ha uma segunda tentativa de abrir o app.
// ============================================================================
#pragma once

#include <windows.h>

namespace soundint {

class SingleInstance {
public:
    SingleInstance() = default;
    ~SingleInstance();

    SingleInstance(const SingleInstance&) = delete;
    SingleInstance& operator=(const SingleInstance&) = delete;

    // Cria/reclama o mutex "SoundInt.SingleInstance".
    // true = somos a unica instancia em execucao (mutex mantido aberto).
    // false = ja existe outra instancia (o chamador deve notificar e sair).
    bool acquire();

    // Segunda instancia: localiza a janela oculta SoundIntMainWindow da
    // primeira e pede a exibicao da UI. Retorna false se nao achou.
    bool notifyExisting() const;

    // Fecha o mutex (no fim do processo).
    void release();

    bool primary() const { return primary_; }

private:
    HANDLE mutex_ = nullptr;
    bool primary_ = false;
};

}  // namespace soundint
