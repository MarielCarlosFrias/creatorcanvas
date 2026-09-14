#pragma once

#include <QDialog>
#include <QImage>
#include "maskeditcanvas.h"

class QComboBox;
class QSlider;
class QLabel;
class QPushButton;
class QProgressBar;
class QThread;

namespace cc {

class BackgroundRemover;
class I18nService;

class AiBackgroundDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AiBackgroundDialog(const QImage& sourceImage, I18nService* i18n = nullptr, QWidget* parent = nullptr);
    ~AiBackgroundDialog() override;

    QImage finalImage() const;

private slots:
    void runAiInference();
    void onModelChanged(int index);
    void onBrowseModel();
    void onBgColorChanged(int index);
    void onModeRestore();
    void onModeErase();

private:
    void setupUi();

    QImage m_originalImage;
    QImage m_processedImage;

    MaskEditCanvas* m_editCanvas = nullptr;
    QComboBox* m_modelCombo = nullptr;
    QPushButton* m_browseModelBtn = nullptr;
    QPushButton* m_runAiButton = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QPushButton* m_restoreBtn = nullptr;
    QPushButton* m_eraseBtn = nullptr;
    QSlider* m_brushSlider = nullptr;
    QLabel* m_brushValLabel = nullptr;
    QComboBox* m_bgColorCombo = nullptr;
    QPushButton* m_undoBtn = nullptr;
    QPushButton* m_resetBtn = nullptr;
    QLabel* m_statusLabel = nullptr;
    I18nService* m_i18n = nullptr;
    QThread* m_workerThread = nullptr;
};

} // namespace cc
