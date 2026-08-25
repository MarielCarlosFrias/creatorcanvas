#pragma once

#include <QColor>
#include <QDialog>
#include "core/document/NewDocumentSpec.h"

class QCheckBox;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QSpinBox;

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

    I18nService* m_i18n = nullptr;
    PresetStore* m_presets = nullptr;

    QLabel* m_presetsTitle = nullptr;
    QListWidget* m_presetList = nullptr;
    QLabel* m_widthLabel = nullptr;
    QSpinBox* m_width = nullptr;
    QLabel* m_heightLabel = nullptr;
    QSpinBox* m_height = nullptr;
    QLabel* m_dpiLabel = nullptr;
    QSpinBox* m_dpi = nullptr;
    QLabel* m_backgroundLabel = nullptr;
    QPushButton* m_colorButton = nullptr;
    QCheckBox* m_transparent = nullptr;
    QPushButton* m_savePresetButton = nullptr;

    QColor m_backgroundColor = Qt::white;
};

} // namespace cc
