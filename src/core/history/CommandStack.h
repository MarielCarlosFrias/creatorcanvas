#pragma once

#include <QObject>
#include <QSet>
#include <QString>

#include <functional>
#include <memory>
#include <vector>

#include "Command.h"

namespace cc {

class CommandStack final : public QObject
{
    Q_OBJECT
public:
    /// Tecto de memória por omissão: ~10 snapshots completos de 4000x3000.
    /// O limite por contagem continua a-valer; este orçamento apenas evita
    /// que passos volumosos (seleções, máscaras, image resize) acumulem
    /// gigabytes antes do limite de contagem ser atingido.
    static constexpr qint64 kDefaultByteBudget = 512ll * 1024 * 1024;

    explicit CommandStack(int limit = 100, qint64 byteBudget = kDefaultByteBudget,
                          QObject* parent = nullptr);

    void execute(std::unique_ptr<Command> command);

    bool canUndo() const { return !m_undoStack.empty(); }
    bool canRedo() const { return !m_redoStack.empty(); }

    void undo();
    void redo();
    void clear();

    int undoCount() const { return static_cast<int>(m_undoStack.size()); }
    int redoCount() const { return static_cast<int>(m_redoStack.size()); }
    int limit() const { return m_limit; }
    void setLimit(int limit);

    qint64 byteBudget() const { return m_byteBudget; }
    void setByteBudget(qint64 bytes);

    /// Memória estimada actualmente retida pelo histórico: snapshots dos
    /// comandos + bitmaps que os comandos ainda mantêm vivos.
    qint64 retainedBytes() const;

    /// O dono da pilha (MainWindow) informa quantos bytes os assets vivos
    /// ocupam — só ele conhece o AssetStore.
    using AssetFootprint = std::function<qint64(const QSet<LayerId>&)>;
    void setAssetFootprintProbe(AssetFootprint probe) { m_assetProbe = std::move(probe); }

    QString nextUndoName() const;
    QString nextRedoName() const;

    int totalCount() const { return undoCount() + redoCount(); }
    int currentIndex() const { return undoCount(); }
    QString commandNameAt(int index) const;
    void jumpToState(int targetIndex);

signals:
    void canUndoChanged(bool canUndo);
    void canRedoChanged(bool canRedo);
    void stateChanged();
    /// Emitido quando comandos são descartados (limite da pilha, redo
    /// invalidado ou clear). `orphanedIds` traz os assets que SÓ os
    /// comandos descartados mantinham vivos — o dono do documento decide
    /// se remove do AssetStore (verificando as camadas atuais).
    void assetsOrphaned(const QSet<QUuid>& orphanedIds);

private:
    void emitStateChanged(bool beforeUndoable, bool beforeRedoable);
    /// Descarta os comandos mais antigos enquanto o orçamento de memória
    /// for excedido (nunca deixa a pilha vazia).
    void trimToByteBudget();
    /// Descarta o comando mais antigo do undo, reportando órfãos.
    void dropOldestUndo();
    /// Coleta ids dos comandos descartados, subtrai os que ainda vivem
    /// nas pilhas restantes e emite assetsOrphaned com a diferença.
    void reportOrphanedAssets(std::vector<std::unique_ptr<Command>> dropped);

    std::vector<std::unique_ptr<Command>> m_undoStack;
    std::vector<std::unique_ptr<Command>> m_redoStack;
    int m_limit;
    qint64 m_byteBudget;
    AssetFootprint m_assetProbe;
};

} // namespace cc
