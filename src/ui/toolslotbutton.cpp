#include "toolslotbutton.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPolygon>
#include <QToolBar>

namespace cc {

ToolSlotButton::ToolSlotButton(QWidget* parent)
    : QToolButton(parent)
{
    setPopupMode(QToolButton::DelayedPopup);
    setAutoRaise(true);
    setStyleSheet(QStringLiteral("QToolButton::menu-indicator { image: none; width: 0px; }"));
}

ToolSlotButton::ToolSlotButton(const QList<QAction*>& actions, QWidget* parent)
    : QToolButton(parent)
{
    setPopupMode(QToolButton::DelayedPopup);
    setAutoRaise(true);
    setStyleSheet(QStringLiteral("QToolButton::menu-indicator { image: none; width: 0px; }"));
    setToolActions(actions);
}

void ToolSlotButton::setToolActions(const QList<QAction*>& actions)
{
    m_actions = actions;
    setupMenu();
}

void ToolSlotButton::setupMenu()
{
    if (m_actions.isEmpty()) {
        if (m_menu) {
            delete m_menu;
            m_menu = nullptr;
        }
        setMenu(nullptr);
        setDefaultAction(nullptr);
        return;
    }

    if (!m_menu) {
        m_menu = new QMenu(this);
    } else {
        m_menu->clear();
    }

    for (QAction* act : m_actions) {
        if (!act) continue;
        m_menu->addAction(act);

        // Se a ação for disparada, define como ativa no botão
        connect(act, &QAction::triggered, this, [this, act]() {
            setCurrentAction(act);
        });

        // Se a ação for marcada como checked (via atalho ou canvas), define como ativa
        connect(act, &QAction::toggled, this, [this, act](bool checked) {
            if (checked) {
                setCurrentAction(act);
            }
        });
    }

    if (m_actions.size() > 1) {
        setMenu(m_menu);
    } else {
        setMenu(nullptr);
    }

    // Define a ação padrão inicial
    if (!defaultAction() && !m_actions.isEmpty()) {
        QAction* initial = m_actions.first();
        for (QAction* act : m_actions) {
            if (act && act->isChecked()) {
                initial = act;
                break;
            }
        }
        setCurrentAction(initial);
    }
}

void ToolSlotButton::setCurrentAction(QAction* action)
{
    if (!action || !m_actions.contains(action))
        return;

    setDefaultAction(action);
    setIcon(action->icon());
    setText(action->text());
    setToolTip(action->toolTip().isEmpty() ? action->text() : action->toolTip());
}

void ToolSlotButton::cycleNextAction()
{
    if (m_actions.size() <= 1) return;
    int currIdx = m_actions.indexOf(defaultAction());
    int nextIdx = (currIdx + 1) % m_actions.size();
    QAction* nextAct = m_actions.at(nextIdx);
    if (nextAct) {
        nextAct->trigger();
    }
}

void ToolSlotButton::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton && m_actions.size() > 1 && m_menu) {
        // Clique com botão direito abre imediatamente o menu flutuante (estilo Photoshop)
        QPoint popupPos;
        if (auto* tb = qobject_cast<QToolBar*>(parentWidget())) {
            if (tb->orientation() == Qt::Vertical) {
                popupPos = mapToGlobal(QPoint(width(), 0));
            } else {
                popupPos = mapToGlobal(QPoint(0, height()));
            }
        } else {
            popupPos = mapToGlobal(QPoint(width(), 0));
        }
        m_menu->exec(popupPos);
        event->accept();
        return;
    }
    QToolButton::mousePressEvent(event);
}

void ToolSlotButton::paintEvent(QPaintEvent* event)
{
    QToolButton::paintEvent(event);

    // Se houver mais de 1 ferramenta no grupo, desenha a setinha triangular clássica do Photoshop no canto
    if (m_actions.size() > 1) {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const int sz = 5;
        const int m = 3;
        QPolygon poly;
        poly << QPoint(width() - m - sz, height() - m)
             << QPoint(width() - m, height() - m)
             << QPoint(width() - m, height() - m - sz);

        p.setPen(Qt::NoPen);
        p.setBrush(isChecked() ? QColor(255, 255, 255, 220) : QColor(160, 165, 175, 180));
        p.drawPolygon(poly);
    }
}

} // namespace cc
