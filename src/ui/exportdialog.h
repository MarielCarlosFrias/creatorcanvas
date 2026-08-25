#pragma once

#include <QDialog>

class QComboBox;
class QLabel;
class QSpinBox;

namespace cc {

class I18nService;

struct ExportSettings
{
    QString format;   // "png" | "jpeg" | "webp"
    int quality = 95;
    double scale = 1.0;
};

/// Export dialog: format, quality (jpeg/webp) and scale multiplier.
class ExportDialog final : public QDialog
{
    Q_OBJECT
public:
    explicit ExportDialog(I18nService* i18n, QWidget* parent = nullptr);

    ExportSettings settings() const;

private:
    void buildUi();
    void retranslateUi();
    void updateQualityEnabled();

    I18nService* m_i18n = nullptr;
    QComboBox* m_format = nullptr;
    QComboBox* m_scale = nullptr;
    QSpinBox* m_quality = nullptr;
    QLabel* m_formatLabel = nullptr;
    QLabel* m_qualityLabel = nullptr;
    QLabel* m_scaleLabel = nullptr;
};

} // namespace cc
