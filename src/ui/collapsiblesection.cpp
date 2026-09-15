#include "collapsiblesection.h"

#include <QToolButton>
#include <QVBoxLayout>
#include <QFrame>

namespace cc {

CollapsibleSection::CollapsibleSection(const QString& title, bool expanded, QWidget* parent)
    : QWidget(parent)
    , m_expanded(expanded)
    , m_title(title)
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 2, 0, 4);
    m_mainLayout->setSpacing(2);

    m_headerButton = new QToolButton(this);
    m_headerButton->setCheckable(true);
    m_headerButton->setChecked(expanded);
    m_headerButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_headerButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_headerButton->setStyleSheet(QStringLiteral(
        "QToolButton {"
        "  border: none;"
        "  background-color: #2b303c;"
        "  color: #e0e0e0;"
        "  font-weight: 600;"
        "  font-size: 11px;"
        "  text-align: left;"
        "  padding: 6px 8px;"
        "  border-radius: 4px;"
        "}"
        "QToolButton:hover {"
        "  background-color: #353b49;"
        "  color: #ffffff;"
        "}"
        "QToolButton:pressed {"
        "  background-color: #242832;"
        "}"
    ));

    connect(m_headerButton, &QToolButton::toggled, this, &CollapsibleSection::setExpanded);

    m_contentArea = new QWidget(this);
    m_contentArea->setVisible(expanded);

    m_mainLayout->addWidget(m_headerButton);
    m_mainLayout->addWidget(m_contentArea);

    updateHeader();
}

void CollapsibleSection::setTitle(const QString& title)
{
    m_title = title;
    updateHeader();
}

QString CollapsibleSection::title() const
{
    return m_title;
}

void CollapsibleSection::setExpanded(bool expanded)
{
    if (m_expanded == expanded && m_contentArea->isVisible() == expanded)
        return;

    m_expanded = expanded;
    if (m_headerButton->isChecked() != expanded) {
        const bool prev = m_headerButton->blockSignals(true);
        m_headerButton->setChecked(expanded);
        m_headerButton->blockSignals(prev);
    }

    m_contentArea->setVisible(expanded);
    updateHeader();
    emit toggled(expanded);
}

void CollapsibleSection::setContentLayout(QLayout* layout)
{
    if (!layout)
        return;
    layout->setContentsMargins(4, 4, 4, 4);
    m_contentArea->setLayout(layout);
}

void CollapsibleSection::updateHeader()
{
    const QString arrow = m_expanded ? QStringLiteral("▼  ") : QStringLiteral("▶  ");
    m_headerButton->setText(arrow + m_title);
}

} // namespace cc
