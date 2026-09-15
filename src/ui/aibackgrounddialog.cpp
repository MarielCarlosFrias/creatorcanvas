#include "aibackgrounddialog.h"
#include "core/image/BackgroundRemover.h"
#include "localization/i18nservice.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QComboBox>
#include <QProgressBar>
#include <QThread>
#include <QTimer>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QPointer>
#include <QCoreApplication>

namespace cc {

AiBackgroundDialog::AiBackgroundDialog(const QImage& sourceImage, I18nService* i18n, QWidget* parent)
    : QDialog(parent),
      m_originalImage(sourceImage),
      m_i18n(i18n)
{
    setWindowTitle(m_i18n ? m_i18n->t("editor", "aiBackground.title") : QStringLiteral("Remoção de Fundo com IA - CreatorCanvas"));
    setMinimumSize(920, 640);
    resize(1000, 700);

    setupUi();

    m_editCanvas->setImage(m_originalImage, QImage());

    // Inicia o recorte com IA automaticamente ao abrir a janela
    QTimer::singleShot(150, this, &AiBackgroundDialog::runAiInference);
}

AiBackgroundDialog::~AiBackgroundDialog()
{
    m_cancelRequested.store(true);
    if (m_workerThread) {
        if (m_workerThread->isRunning()) {
            m_workerThread->requestInterruption();
            m_workerThread->wait(2000);
        }
    }
}

void AiBackgroundDialog::reject()
{
    m_cancelRequested.store(true);
    if (m_workerThread && m_workerThread->isRunning()) {
        m_workerThread->requestInterruption();
    }
    QDialog::reject();
}

void AiBackgroundDialog::closeEvent(QCloseEvent* event)
{
    m_cancelRequested.store(true);
    if (m_workerThread && m_workerThread->isRunning()) {
        m_workerThread->requestInterruption();
        m_workerThread->wait(1000);
    }
    QDialog::closeEvent(event);
}

void AiBackgroundDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    // --- Barra Superior de Controle de IA ---
    auto* topCard = new QWidget(this);
    topCard->setStyleSheet(QStringLiteral("background-color: #202227; border-radius: 8px;"));
    auto* topLayout = new QHBoxLayout(topCard);
    topLayout->setContentsMargins(12, 8, 12, 8);
    topLayout->setSpacing(12);

    auto* modelLabel = new QLabel(m_i18n ? m_i18n->t("editor", "aiBackground.model") : QStringLiteral("Modelo de IA:"), this);
    modelLabel->setStyleSheet(QStringLiteral("font-weight: bold; color: #E0E0E0;"));
    topLayout->addWidget(modelLabel);

    m_modelCombo = new QComboBox(this);
    m_modelCombo->addItem(m_i18n ? m_i18n->t("editor", "aiBackground.model.u2netp") : QStringLiteral("U²-Netp (Ultraleve e Rápido)"), QStringLiteral("u2netp.onnx"));

    const QString u2netPath = BackgroundRemover::findModelPath(QStringLiteral("u2net.onnx"));
    if (!u2netPath.isEmpty() && QFileInfo::exists(u2netPath)) {
        m_modelCombo->addItem(m_i18n ? m_i18n->t("editor", "aiBackground.model.u2net") : QStringLiteral("U²-Net (Alta Precisão)"), QStringLiteral("u2net.onnx"));
    }
    connect(m_modelCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &AiBackgroundDialog::onModelChanged);
    topLayout->addWidget(m_modelCombo);

    m_browseModelBtn = new QPushButton(m_i18n ? m_i18n->t("editor", "aiBackground.browseModel") : QStringLiteral("Procurar..."), this);
    m_browseModelBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #2A2D34; color: #EEE; padding: 5px 10px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #3C3F46; }"
    ));
    connect(m_browseModelBtn, &QPushButton::clicked, this, &AiBackgroundDialog::onBrowseModel);
    topLayout->addWidget(m_browseModelBtn);

    m_runAiButton = new QPushButton(m_i18n ? m_i18n->t("editor", "aiBackground.runAi") : QStringLiteral("✨ Recortar com IA"), this);
    m_runAiButton->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #0078D4; color: white; font-weight: bold; padding: 6px 16px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #1084E3; }"
        "QPushButton:disabled { background-color: #3C3F46; color: #888888; }"
    ));
    connect(m_runAiButton, &QPushButton::clicked, this, &AiBackgroundDialog::runAiInference);
    topLayout->addWidget(m_runAiButton);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 0); // Modo indeterminado (busy)
    m_progressBar->setFixedHeight(16);
    m_progressBar->setFixedWidth(140);
    m_progressBar->setVisible(false);
    topLayout->addWidget(m_progressBar);

    topLayout->addStretch();

    auto* bgPreviewLabel = new QLabel(m_i18n ? m_i18n->t("editor", "aiBackground.background") : QStringLiteral("Fundo:"), this);
    bgPreviewLabel->setStyleSheet(QStringLiteral("color: #BBB;"));
    topLayout->addWidget(bgPreviewLabel);

    m_bgColorCombo = new QComboBox(this);
    m_bgColorCombo->addItem(m_i18n ? m_i18n->t("editor", "aiBackground.bg.checker") : QStringLiteral("Xadrez Transparente"));
    m_bgColorCombo->addItem(m_i18n ? m_i18n->t("editor", "aiBackground.bg.white") : QStringLiteral("Branco"));
    m_bgColorCombo->addItem(m_i18n ? m_i18n->t("editor", "aiBackground.bg.black") : QStringLiteral("Preto"));
    connect(m_bgColorCombo, &QComboBox::currentIndexChanged, this, &AiBackgroundDialog::onBgColorChanged);
    topLayout->addWidget(m_bgColorCombo);

    mainLayout->addWidget(topCard);

    // --- Barra de Retoque de Máscara (Pincel) ---
    auto* toolsCard = new QWidget(this);
    toolsCard->setStyleSheet(QStringLiteral("background-color: #1A1C20; border-radius: 6px;"));
    auto* toolsLayout = new QHBoxLayout(toolsCard);
    toolsLayout->setContentsMargins(10, 6, 10, 6);
    toolsLayout->setSpacing(10);

    auto* toolsHeader = new QLabel(m_i18n ? m_i18n->t("editor", "aiBackground.touchup") : QStringLiteral("Pincel de Retoque:"), this);
    toolsHeader->setStyleSheet(QStringLiteral("color: #888; font-size: 11px; font-weight: bold;"));
    toolsLayout->addWidget(toolsHeader);

    m_restoreBtn = new QPushButton(m_i18n ? m_i18n->t("editor", "aiBackground.restore") : QStringLiteral("🖌️ Restaurar"), this);
    m_restoreBtn->setCheckable(true);
    m_restoreBtn->setChecked(true);
    m_restoreBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #2A2D34; color: #EEE; padding: 4px 10px; border-radius: 4px; }"
        "QPushButton:checked { background: #2E7D32; color: white; font-weight: bold; }"
    ));
    connect(m_restoreBtn, &QPushButton::clicked, this, &AiBackgroundDialog::onModeRestore);
    toolsLayout->addWidget(m_restoreBtn);

    m_eraseBtn = new QPushButton(m_i18n ? m_i18n->t("editor", "aiBackground.erase") : QStringLiteral("🧹 Apagar"), this);
    m_eraseBtn->setCheckable(true);
    m_eraseBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #2A2D34; color: #EEE; padding: 4px 10px; border-radius: 4px; }"
        "QPushButton:checked { background: #C62828; color: white; font-weight: bold; }"
    ));
    connect(m_eraseBtn, &QPushButton::clicked, this, &AiBackgroundDialog::onModeErase);
    toolsLayout->addWidget(m_eraseBtn);

    auto* sizeLabel = new QLabel(m_i18n ? m_i18n->t("editor", "aiBackground.brushSize") : QStringLiteral("Tamanho:"), this);
    sizeLabel->setStyleSheet(QStringLiteral("color: #BBB; font-size: 11px;"));
    toolsLayout->addWidget(sizeLabel);

    m_brushSlider = new QSlider(Qt::Horizontal, this);
    m_brushSlider->setRange(1, 100);
    m_brushSlider->setValue(15);
    m_brushSlider->setFixedWidth(120);
    toolsLayout->addWidget(m_brushSlider);

    m_brushValLabel = new QLabel(QStringLiteral("15px"), this);
    m_brushValLabel->setFixedWidth(36);
    m_brushValLabel->setStyleSheet(QStringLiteral("color: #888; font-size: 11px;"));
    toolsLayout->addWidget(m_brushValLabel);

    connect(m_brushSlider, &QSlider::valueChanged, this, [this](int val) {
        m_brushValLabel->setText(QString::number(val) + QStringLiteral("px"));
        m_editCanvas->setBrushRadius(val);
    });

    m_undoBtn = new QPushButton(m_i18n ? m_i18n->t("common", "action.undo") : QStringLiteral("↺ Desfazer"), this);
    connect(m_undoBtn, &QPushButton::clicked, m_editCanvas, &MaskEditCanvas::undo);
    toolsLayout->addWidget(m_undoBtn);

    m_resetBtn = new QPushButton(m_i18n ? m_i18n->t("editor", "aiBackground.resetAll") : QStringLiteral("Restaurar Tudo"), this);
    connect(m_resetBtn, &QPushButton::clicked, m_editCanvas, &MaskEditCanvas::clearEdits);
    toolsLayout->addWidget(m_resetBtn);

    toolsLayout->addStretch();
    mainLayout->addWidget(toolsCard);

    // --- Canvas de Edição e Visualização ---
    m_editCanvas = new MaskEditCanvas(this);
    m_editCanvas->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainLayout->addWidget(m_editCanvas, 1);

    // --- Rodapé: Status e Botões de Confirmação ---
    auto* footerLayout = new QHBoxLayout();
    m_statusLabel = new QLabel(m_i18n ? m_i18n->t("editor", "aiBackground.tip") : QStringLiteral("💡 Dica: Passe o pincel para recuperar ou apagar áreas da imagem."), this);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #888; font-size: 11px;"));
    footerLayout->addWidget(m_statusLabel);
    footerLayout->addStretch();

    auto* cancelBtn = new QPushButton(m_i18n ? m_i18n->t("common", "action.cancel") : QStringLiteral("Cancelar"), this);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    footerLayout->addWidget(cancelBtn);

    auto* applyBtn = new QPushButton(m_i18n ? m_i18n->t("editor", "aiBackground.apply") : QStringLiteral("✓ Aplicar ao Canvas"), this);
    applyBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #2E7D32; color: white; font-weight: bold; padding: 6px 18px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #388E3C; }"
    ));
    connect(applyBtn, &QPushButton::clicked, this, &QDialog::accept);
    footerLayout->addWidget(applyBtn);

    mainLayout->addLayout(footerLayout);
}

void AiBackgroundDialog::onModeRestore()
{
    m_restoreBtn->setChecked(true);
    m_eraseBtn->setChecked(false);
    m_editCanvas->setMode(MaskEditCanvas::Restore);
}

void AiBackgroundDialog::onModeErase()
{
    m_eraseBtn->setChecked(true);
    m_restoreBtn->setChecked(false);
    m_editCanvas->setMode(MaskEditCanvas::Erase);
}

void AiBackgroundDialog::onModelChanged(int)
{
    runAiInference();
}

void AiBackgroundDialog::onBrowseModel()
{
    const QString title = m_i18n ? m_i18n->t("editor", "aiBackground.selectModelTitle") : QStringLiteral("Selecionar Arquivo de Modelo ONNX");
    const QString path = QFileDialog::getOpenFileName(this, title, QString(), QStringLiteral("ONNX Models (*.onnx);;All Files (*.*)"));
    if (path.isEmpty())
        return;

    const QFileInfo fi(path);
    const QString displayName = fi.fileName();

    int existingIdx = -1;
    for (int i = 0; i < m_modelCombo->count(); ++i) {
        if (m_modelCombo->itemData(i).toString() == path) {
            existingIdx = i;
            break;
        }
    }

    if (existingIdx >= 0) {
        m_modelCombo->setCurrentIndex(existingIdx);
    } else {
        m_modelCombo->addItem(displayName, path);
        m_modelCombo->setCurrentIndex(m_modelCombo->count() - 1);
    }
}

void AiBackgroundDialog::onBgColorChanged(int index)
{
    switch (index) {
    case 1: m_editCanvas->setBackgroundColor(Qt::white); break;
    case 2: m_editCanvas->setBackgroundColor(Qt::black); break;
    default: m_editCanvas->setBackgroundColor(QColor()); break;
    }
}

void AiBackgroundDialog::runAiInference()
{
    if (m_workerThread && m_workerThread->isRunning())
        return;

    const QString modelData = m_modelCombo->currentData().toString();
    QString modelPath;
    if (QFileInfo::exists(modelData)) {
        modelPath = modelData;
    } else {
        modelPath = BackgroundRemover::findModelPath(modelData);
    }

    if (modelPath.isEmpty() || !QFileInfo::exists(modelPath)) {
        const QMessageBox::StandardButton reply = QMessageBox::warning(
            this,
            m_i18n ? m_i18n->t("editor", "aiBackground.modelNotFound.title") : QStringLiteral("Modelo não encontrado"),
            m_i18n ? m_i18n->t("editor", "aiBackground.modelNotFound.body", {modelData}) : QStringLiteral("Não foi possível encontrar o arquivo do modelo ONNX (%1). Você pode selecionar o arquivo manualmente.").arg(modelData),
            QMessageBox::Open | QMessageBox::Cancel,
            QMessageBox::Open);
        if (reply == QMessageBox::Open) {
            onBrowseModel();
        }
        return;
    }

    m_cancelRequested.store(false);
    m_runAiButton->setEnabled(false);
    m_modelCombo->setEnabled(false);
    m_browseModelBtn->setEnabled(false);
    m_progressBar->setVisible(true);
    m_statusLabel->setText(m_i18n ? m_i18n->t("editor", "aiBackground.processing") : QStringLiteral("⏳ Processando imagem com a rede neural..."));

    const QImage input = m_originalImage;
    QPointer<AiBackgroundDialog> self(this);

    m_workerThread = QThread::create([self, input, modelPath]() {
        BackgroundRemover remover(modelPath);
        const std::atomic<bool>* cancelPtr = self ? &self->m_cancelRequested : nullptr;
        QImage result = remover.removeBackground(input, cancelPtr);

        QMetaObject::invokeMethod(qApp, [self, result]() {
            if (!self)
                return;
            self->m_progressBar->setVisible(false);
            self->m_runAiButton->setEnabled(true);
            self->m_modelCombo->setEnabled(true);
            self->m_browseModelBtn->setEnabled(true);
            self->m_workerThread = nullptr;

            if (self->m_cancelRequested.load() || result.isNull()) {
                self->m_statusLabel->setText(self->m_i18n ? self->m_i18n->t("editor", "aiBackground.tip") : QStringLiteral("Processamento cancelado ou interrompido."));
                return;
            }

            self->m_processedImage = result;
            self->m_editCanvas->setImage(self->m_originalImage, self->m_processedImage);
            self->m_statusLabel->setText(self->m_i18n ? self->m_i18n->t("editor", "aiBackground.done") : QStringLiteral("✓ Recorte concluído! Se desejar, faça ajustes com o pincel e clique em Aplicar."));
        });
    });

    connect(m_workerThread, &QThread::finished, m_workerThread, &QObject::deleteLater);
    m_workerThread->start();
}

QImage AiBackgroundDialog::finalImage() const
{
    return m_editCanvas ? m_editCanvas->getEditedImage() : m_processedImage;
}

} // namespace cc
