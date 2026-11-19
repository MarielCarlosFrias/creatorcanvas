#pragma once

#include <atomic>

namespace cc {

/// Estado pequeno e explícito das operações de prévia do inspetor.
/// A geração invalida resultados antigos; a fila garante que um novo valor
/// de slider será processado depois da operação atual.
class PreviewState final
{
public:
    unsigned invalidate()
    {
        m_queued = false;
        return ++m_generation;
    }

    unsigned nextGeneration() { return ++m_generation; }
    unsigned generation() const { return m_generation.load(); }

    bool inFlight() const { return m_inFlight; }
    void setInFlight(bool value) { m_inFlight = value; }
    bool queued() const { return m_queued; }
    void setQueued(bool value) { m_queued = value; }

private:
    std::atomic_uint m_generation{0};
    bool m_inFlight = false;
    bool m_queued = false;
};

} // namespace cc
