#pragma once

#include <QColor>
#include <QDialog>
#include "core/document/NewDocumentSpec.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QSpinBox;
class QTabBar;
class QToolButton;

namespace cc {

class I18nService;
class PresetStore;

class NewDocumentDialog final : public QDialog
{
    Q_OBJECT
public:
    explicit NewDocumentDialog(I18nService* i18n, PresetStore* presets,
                               QWidget* parent = nullptr);

    NewDocumentSpec spec() const;

private:
    void buildUi();
    void populatePresetList();
    void applyPreset(QListWidgetItem* current);
    void updateColorButtonText();
    void savePreset();
    void retranslateUi();
    void onCategoryChanged(int index);
    void swapOrientation();

    I18nService* m_i18n = nullptr;
    PresetStore* m_presets = nullptr;

    // --- Top category tabs ---
    QTabBar* m_categoryTabs = nullptr;

    // --- Left: preset cards grid ---
    QLabel* m_presetsTitle = nullptr;
    QListWidget* m_presetList = nullptr;

    // --- Right: details form ---
    QLabel* m_detailsTitle = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QLabel* m_widthLabel = nullptr;
    QSpinBox* m_width = nullptr;
    QLabel* m_heightLabel = nullptr;
    QSpinBox* m_height = nullptr;
    QToolButton* m_orientPortrait = nullptr;
    QToolButton* m_orientLandscape = nullptr;
    QLabel* m_dpiLabel = nullptr;
    QSpinBox* m_dpi = nullptr;
    QLabel* m_colorModeLabel = nullptr;
    QComboBox* m_colorMode = nullptr;
    QLabel* m_backgroundLabel = nullptr;
    QComboBox* m_backgroundCombo = nullptr;
    QPushButton* m_colorButton = nullptr;
    QCheckBox* m_transparent = nullptr;
    QPushButton* m_savePresetButton = nullptr;

    QColor m_backgroundColor = Qt::white;

    QStringList m_categories;
};

} // namespace cc
