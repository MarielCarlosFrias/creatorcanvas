#pragma once
#include <QWidget>
#include "core/document/NewDocumentSpec.h"

class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;

namespace cc {

class I18nService;
class PresetStore;
class RecentFiles;

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

private:
    void retranslateUi();
    void populateRecents();

    I18nService* m_i18n = nullptr;
    PresetStore* m_presets = nullptr;
    RecentFiles* m_recent = nullptr;

    QLabel* m_title = nullptr;
    QLabel* m_tagline = nullptr;
    QLabel* m_createLabel = nullptr;
    QPushButton* m_customButton = nullptr;
    QPushButton* m_openButton = nullptr;
    QLabel* m_recentLabel = nullptr;
    QListWidget* m_recentList = nullptr;
};

} // namespace cc
