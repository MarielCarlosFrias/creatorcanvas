#pragma once

#include <QWidget>
#include <QVector>
#include "core/document/NewDocumentSpec.h"

class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QStackedWidget;

namespace cc {

class I18nService;
class PresetStore;
class RecentFiles;
struct DocumentPreset;

/// Modern Start / Welcome Screen for CreatorCanvas:
/// - Prominent "Create Design" & "Open Project" actions
/// - Visual preset cards with platform branding (YouTube, Instagram, TikTok, etc.)
/// - Recent projects list with real thumbnail preview
/// - Polished empty state with helpful onboarding hint
class StartScreen final : public QWidget
{
    Q_OBJECT
public:
    explicit StartScreen(I18nService* i18n, PresetStore* presets,
                         RecentFiles* recent, QWidget* parent = nullptr);
    void refreshRecents();

signals:
    void createRequested(const cc::NewDocumentSpec& spec);
    void customCreateRequested();
    void openRequested();
    void recentActivated(const QString& filePath);
    void templateRequested(int templateKind);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void buildUi();
    void retranslateUi();
    void populateRecents();
    QWidget* createVisualCard(const QString& brandName, const QPixmap& icon,
                              const QString& dimensions, const QString& ratio,
                              const NewDocumentSpec& spec);

    I18nService* m_i18n = nullptr;
    PresetStore* m_presets = nullptr;
    RecentFiles* m_recent = nullptr;

    // Header & Action buttons
    QLabel* m_title = nullptr;
    QLabel* m_tagline = nullptr;
    QPushButton* m_createDesignBtn = nullptr;
    QPushButton* m_openProjectBtn = nullptr;
    QPushButton* m_customSizeBtn = nullptr;

    // Visual Cards Section
    QLabel* m_popularLabel = nullptr;
    QWidget* m_cardsHost = nullptr;

    // Templates Section
    QLabel* m_templatesLabel = nullptr;
    QWidget* m_templatesHost = nullptr;

    // Recents Section
    QLabel* m_recentLabel = nullptr;
    QStackedWidget* m_recentStack = nullptr;
    QListWidget* m_recentList = nullptr;
    QWidget* m_emptyStateWidget = nullptr;
    QLabel* m_emptyStateTitle = nullptr;
    QLabel* m_emptyStateSubtitle = nullptr;
    QLabel* m_emptyStateIcon = nullptr;
};

} // namespace cc
