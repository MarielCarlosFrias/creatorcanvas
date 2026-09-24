#pragma once

#include <QWidget>
#include <QPointer>

class QListWidget;
class QListWidgetItem;
class QToolButton;
class QLabel;

namespace cc {

class I18nService;
class CommandStack;

/// Painel de Histórico Visual (Lista de Ações com Desfazer Múltiplo estilo Photoshop):
/// - Exibe o estado inicial do documento e todas as ações gravadas
/// - Destaca visualmente a etapa atual no tempo
/// - Permite clicar em qualquer ação passada ou futura para pular instantaneamente
/// - Botões de ação rápida para desfazer (passo anterior) e refazer (próximo passo)
class HistoryPanel final : public QWidget
{
    Q_OBJECT
public:
    explicit HistoryPanel(I18nService* i18n, CommandStack* history = nullptr,
                         QWidget* parent = nullptr);

    void setHistory(CommandStack* history);
    void refresh();

private slots:
    void onItemClicked(QListWidgetItem* item);

private:
    void buildUi();
    void retranslateUi();
    QString friendlyCommandName(const QString& rawName) const;

    I18nService* m_i18n = nullptr;
    QPointer<CommandStack> m_history;

    QListWidget* m_list = nullptr;
    QToolButton* m_undoBtn = nullptr;
    QToolButton* m_redoBtn = nullptr;
    QLabel* m_statusLabel = nullptr;
    bool m_updating = false;
};

} // namespace cc
