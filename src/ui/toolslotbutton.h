#pragma once

#include <QAction>
#include <QList>
#include <QMenu>
#include <QToolButton>

namespace cc {

/// Botão de slot de ferramentas no estilo Photoshop / Illustrator.
/// Agrupa múltiplas ferramentas da mesma categoria em um único ícone da barra lateral.
/// - Clique esquerdo normal: ativa a ferramenta atualmente selecionada no slot.
/// - Clique com Botão Direito ou Clique Longo: abre o menu flutuante para trocar de ferramenta.
/// - Desenha um indicador triangular sutil no canto inferior direito quando há ferramentas irmãs no grupo.
/// - Quando qualquer ferramenta do grupo é ativada (via atalho ou menu), o botão atualiza seu ícone para ela.
class ToolSlotButton : public QToolButton
{
    Q_OBJECT
public:
    explicit ToolSlotButton(QWidget* parent = nullptr);
    explicit ToolSlotButton(const QList<QAction*>& actions, QWidget* parent = nullptr);

    void setToolActions(const QList<QAction*>& actions);
    QList<QAction*> toolActions() const { return m_actions; }

    void setCurrentAction(QAction* action);
    QAction* currentAction() const { return defaultAction(); }

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void setupMenu();

    QList<QAction*> m_actions;
    QMenu* m_menu = nullptr;
};

} // namespace cc
