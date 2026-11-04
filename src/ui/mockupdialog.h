#pragma once

#include <QDialog>
#include <QImage>
#include <QString>
#include "core/Document.h"

class QComboBox;
class QCheckBox;
class QSlider;
class QLabel;
class QPushButton;

namespace cc {

class I18nService;

struct MockupDialogResult {
    bool accepted = false;
    QImage logo;
    bool freeShape = false;
};

class MockupDialog : public QDialog
{
    Q_OBJECT
public:
    explicit MockupDialog(QWidget* parent, Document* document, const LayerId& currentLayerId,
                          I18nService* i18n = nullptr);

    MockupDialogResult execute();

    /// Processa o logo delegando para ImageProcessing::prepareMockupLogo
    static QImage processLogo(const QImage& logo, bool removeBackground, int tolerance = 18);

private slots:
    void onBrowseClicked();
    void onSourceChanged();
    void updatePreview();

private:
    void loadCurrentSourceLogo();
    QString trText(const QString& key, const QString& fallback) const;

    Document* m_document = nullptr;
    LayerId m_currentLayerId;
    I18nService* m_i18n = nullptr;

    QComboBox* m_sourceCombo = nullptr;
    QPushButton* m_browseBtn = nullptr;
    QComboBox* m_modeCombo = nullptr;
    QCheckBox* m_removeBackground = nullptr;
    QSlider* m_toleranceSlider = nullptr;
    QLabel* m_toleranceValueLabel = nullptr;
    QLabel* m_previewLabel = nullptr;

    QImage m_rawLogo;
    QImage m_processedLogo;
    QString m_importedFilePath;
};

} // namespace cc
