#pragma once

#include <QDialog>

class QComboBox;
class QCheckBox;
class QLabel;

namespace cc {

class I18nService;
class SettingsService;

/// General settings: language (applied live via I18nService) and autosave
/// interval (persisted; the autosave engine itself lands in M12).
class SettingsDialog final : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(I18nService* i18n, SettingsService* settings,
                            QWidget* parent = nullptr);

private:
    void accept() override;
    void retranslateUi();

    I18nService* m_i18n = nullptr;
    SettingsService* m_settings = nullptr;
    QComboBox* m_language = nullptr;
    QComboBox* m_autosave = nullptr;
    QCheckBox* m_gpuAcceleration = nullptr;
    QCheckBox* m_darkMode = nullptr;
    QCheckBox* m_confirmExit = nullptr;
    QCheckBox* m_showGrid = nullptr;
    QCheckBox* m_snapToGrid = nullptr;
    QLabel* m_languageLabel = nullptr;
    QLabel* m_autosaveLabel = nullptr;
    QLabel* m_gpuAccelerationLabel = nullptr;
    QLabel* m_restartHintLabel = nullptr;
    QLabel* m_appearanceLabel = nullptr;
    QLabel* m_canvasLabel = nullptr;
};

} // namespace cc
