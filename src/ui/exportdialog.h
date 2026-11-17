#pragma once

#include <QDialog>
#include <QUuid>
#include <QImage>
#include <QMap>

class QCheckBox;
class QComboBox;
class QLabel;
class QSpinBox;
class QRadioButton;
class QButtonGroup;
class QPushButton;
class QTimer;

namespace cc {

class Document;
class I18nService;

struct ExportSettings
{
    QString format;   // "png" | "jpeg" | "webp"
    int quality = 95;
    double scale = 1.0;
    bool exportSelectedLayerOnly = false;
    QUuid selectedLayerId;
    /// Keep the alpha channel (PNG/WebP). JPEG is always opaque.
    bool transparent = true;
};

/// Modernized export dialog: platform presets on quick chips, live preview
/// with checkerboard transparency, real-time estimated file size, output
/// resolution, transparency toggle and selection scope.
class ExportDialog final : public QDialog
{
    Q_OBJECT
public:
    explicit ExportDialog(const Document* doc,
                          I18nService* i18n,
                          const QUuid& selectedLayerId = {},
                          QWidget* parent = nullptr);

    ExportSettings settings() const;
    void applyPresetById(const QString& presetId);

private:
    void buildUi();
    void retranslateUi();
    void updateControlsEnabled();
    void applyPreset(const QString& presetId);
    void updatePresetChips();
    void updatePreview();
    void estimateFileSize();
    /// Translated string with an English fallback when i18n is unavailable.
    QString tr_(const QString& key, const QString& fallback) const;
    /// i18n key holding the full description of a quick preset.
    static QString presetTooltipKey(const QString& presetId);

    const Document* m_doc = nullptr;
    I18nService* m_i18n = nullptr;
    QUuid m_selectedLayerId;

    // Controls
    QString m_currentPreset = QStringLiteral("original");
    QMap<QString, QPushButton*> m_presetChips;
    QButtonGroup* m_presetChipsGroup = nullptr;
    QComboBox* m_format = nullptr;
    QComboBox* m_scale = nullptr;
    QSpinBox* m_quality = nullptr;
    QCheckBox* m_transparent = nullptr;
    QRadioButton* m_scopeEntire = nullptr;
    QRadioButton* m_scopeLayer = nullptr;
    QButtonGroup* m_scopeGroup = nullptr;

    // Labels & Information
    QLabel* m_formatLabel = nullptr;
    QLabel* m_qualityLabel = nullptr;
    QLabel* m_scaleLabel = nullptr;
    QLabel* m_transparentLabel = nullptr;
    QLabel* m_dimensionLabel = nullptr;
    QLabel* m_fileSizeLabel = nullptr;
    QLabel* m_previewImageLabel = nullptr;
    QLabel* m_presetsCaption = nullptr;
    QLabel* m_scopeCaption = nullptr;
    QLabel* m_outputCaption = nullptr;
    QLabel* m_previewCaption = nullptr;

    // Cached rendered preview thumbnail
    QImage m_cachedBaseThumbnail;

    // Index of the reusable scale slot filled in by platform presets
    int m_dynamicScaleIndex = -1;
    // Guard so preset application is not treated as a manual user change
    bool m_applyingPreset = false;
    /// Transparency the user asked for, kept across formats without alpha
    /// (JPEG) so the choice is restored when alpha-capable formats return.
    bool m_transparentWanted{true};

    // Debounce timer for preview re-render (avoid heavy continuous renders)
    QTimer* m_previewDebounceTimer = nullptr;
};

} // namespace cc
