#pragma once

#include <QWidget>
#include <QString>

class QToolButton;
class QWidget;
class QVBoxLayout;

namespace cc {

/// Reusable modern collapsible accordion section for inspector sidebars:
/// - Clickable header with indicator (▼ / ▶) and bold title
/// - Collapsible content container
/// - Smooth state toggle and i18n dynamic retranslation
class CollapsibleSection final : public QWidget
{
    Q_OBJECT
public:
    explicit CollapsibleSection(const QString& title, bool expanded = true, QWidget* parent = nullptr);

    void setTitle(const QString& title);
    QString title() const;

    void setExpanded(bool expanded);
    bool isExpanded() const { return m_expanded; }

    void setContentLayout(QLayout* layout);
    QWidget* contentWidget() const { return m_contentArea; }

signals:
    void toggled(bool expanded);

private:
    void updateHeader();

    bool m_expanded = true;
    QString m_title;
    QToolButton* m_headerButton = nullptr;
    QWidget* m_contentArea = nullptr;
    QVBoxLayout* m_mainLayout = nullptr;
};

} // namespace cc
