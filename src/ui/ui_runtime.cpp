#include "ui/control.h"
#include "ui/renderer.h"

namespace soundint::ui {

// ---------------------------------------------------------------------------
// Invalidacao global: a janela ativa registra como re-renderizar.
// ---------------------------------------------------------------------------
namespace {
InvalidateFn g_invalidate = nullptr;
void* g_invalidateContext = nullptr;
}  // namespace

void setInvalidateHandler(InvalidateFn fn, void* context)
{
    g_invalidate = fn;
    g_invalidateContext = context;
}

void requestRender()
{
    if (g_invalidate) {
        g_invalidate(g_invalidateContext);
    }
}

void Control::invalidate()
{
    requestRender();
}

// Ancora do modulo ui.
void uiModuleAnchor() {}

}  // namespace soundint::ui
