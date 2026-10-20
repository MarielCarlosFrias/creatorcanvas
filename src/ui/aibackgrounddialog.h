#pragma once

#include <QDialog>
#include <QImage>
#include <QPointer>
#include <atomic>
#include <memory>
#include "maskeditcanvas.h"

class QComboBox;
class QCheckBox;
class QSlider;
class QLabel;
class QPushButton;
class QProgressBar;
class QThread;
class QCloseEvent;

namespace cc {

class BackgroundRemover;
class I18nService;

class AiBackgroundDialog : public QDialog
{
    Q_OBJECT
public:
    /// autoRunInference=false é usado por testes/capturas de layout: sem
    /// isso o diálogo abre a inferência e, sem modelo, um QMessageBox modal
    /// bloqueia qualquer execução headless.
    explicit AiBackgroundDialog(const QImage& sourceImage, I18nService* i18n = nullptr,
                                QWidget* parent = nullptr,
                                bool autoRunInference = true);
    ~AiBackgroundDialog() override;

    QImage finalImage() const;

public slots:
    void reject() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void runAiInference();
    void reapplyMatte();
    void onModelChanged(int index);
    void onBrowseModel();
    void onBgColorChanged(int index);
    void onModeRestore();
    void onModeErase();

private:
    void setupUi();

    QImage m_originalImage;
    QImage m_processedImage;
    QImage m_rawAlpha;   // mascara bruta da IA p/ re-aplicar ao vivo

    MaskEditCanvas* m_editCanvas = nullptr;
    QComboBox* m_modelCombo = nullptr;
    QPushButton* m_browseModelBtn = nullptr;
    QPushButton* m_runAiButton = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QPushButton* m_restoreBtn = nullptr;
    QPushButton* m_eraseBtn = nullptr;
    QSlider* m_brushSlider = nullptr;
    QSlider* m_cutSlider = nullptr;
    QCheckBox* m_specksCheck = nullptr;
    QLabel* m_brushValLabel = nullptr;
    QComboBox* m_bgColorCombo = nullptr;
    QPushButton* m_undoBtn = nullptr;
    QPushButton* m_resetBtn = nullptr;
    QLabel* m_statusLabel = nullptr;
    I18nService* m_i18n = nullptr;
    // QPointer: zera sozinho quando o thread se auto-deleta via deleteLater,
    // blindando os guards de re-entrada contra um QThread destruído.
    QPointer<QThread> m_workerThread;
    // shared_ptr capturado POR VALOR pelo worker: o flag sobrevive ao
    // diálogo (alocado na stack) mesmo que a inferência ultrapasse os
    // waits do destrutor — antes era um ponteiro cru para o membro e o
    // worker lesse memória liberada.
    std::shared_ptr<std::atomic<bool>> m_cancelFlag;
};

} // namespace cc
