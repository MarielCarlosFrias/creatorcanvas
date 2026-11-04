#include "mockupdialog.h"
#include "core/image/ImageProcessing.h"
#include "localization/i18nservice.h"

#include <QComboBox>
#include <QCheckBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QPixmap>
#include <QFileInfo>

namespace cc {

MockupDialog::MockupDialog(QWidget* parent, Document* document, const LayerId& currentLayerId,
                           I18nService* i18n)
    : QDialog(parent)
    , m_document(document)
    , m_currentLayerId(currentLayerId)
    , m_i18n(i18n)
{
    setWindowTitle(trText(QStringLiteral("dialog.mockup.title"),
                          QStringLiteral("Escolher imagem para o mockup")));
    setMinimumWidth(380);

    auto* mainLayout = new QVBoxLayout(this);
    auto* form = new QFormLayout;

    // Linha de seleção de fonte + botão Procurar
    auto* sourceLayout = new QHBoxLayout;
    m_sourceCombo = new QComboBox(this);
    m_sourceCombo->addItem(trText(QStringLiteral("dialog.mockup.importNew"),
                                 QStringLiteral("Importar uma imagem...")), QString());
    if (m_document && m_document->rootGroup()) {
        for (const auto& child : m_document->rootGroup()->children) {
            if (!child || child->type() != LayerType::Image || child->id() == m_currentLayerId)
                continue;
            const auto* imageLayer = static_cast<const ImageLayer*>(child.get());
            const QString defaultName = trText(QStringLiteral("dialog.mockup.existing"),
                                               QStringLiteral("Imagem existente"));
            m_sourceCombo->addItem(imageLayer->name.isEmpty() ? defaultName : imageLayer->name,
                                  imageLayer->id().toString(QUuid::WithoutBraces));
        }
    }
    m_browseBtn = new QPushButton(trText(QStringLiteral("dialog.mockup.browse"),
                                         QStringLiteral("Procurar...")), this);

    sourceLayout->addWidget(m_sourceCombo, 1);
    sourceLayout->addWidget(m_browseBtn);
    form->addRow(trText(QStringLiteral("dialog.mockup.source"),
                        QStringLiteral("Imagem fonte")), sourceLayout);

    // Modo do warp
    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(trText(QStringLiteral("dialog.mockup.modePerspective"),
                                QStringLiteral("Perspectiva (4 cantos) — como Photoshop Distort")), false);
    m_modeCombo->addItem(trText(QStringLiteral("dialog.mockup.modeFreeShape"),
                                QStringLiteral("Forma livre (qualquer formato — contorno inteiro)")), true);
    m_modeCombo->setToolTip(
        QStringLiteral("Perspectiva: marque os 4 cantos da área; a imagem é encaixada em "
                       "perspectiva exata (ideal para estradas/placas).\n"
                       "Forma livre: marque o contorno inteiro (5+ pontos, qualquer "
                       "formato); a imagem preenche a forma, seguindo as arestas."));
    form->addRow(trText(QStringLiteral("dialog.mockup.mode"),
                        QStringLiteral("Modo")), m_modeCombo);

    // Remoção de fundo (Varinha Mágica)
    m_removeBackground = new QCheckBox(trText(QStringLiteral("dialog.mockup.removeBg"),
                                             QStringLiteral("Remover fundo uniforme (Varinha Mágica)")), this);
    m_removeBackground->setToolTip(QStringLiteral("Útil para logos com fundo sólido (branco, magenta, etc.). Removido antes do warp para que só o conteúdo siga os pontos. O ficheiro original não é alterado."));
    m_removeBackground->setChecked(true);
    form->addRow(m_removeBackground);

    // Slider de Tolerância
    auto* tolLayout = new QHBoxLayout;
    m_toleranceSlider = new QSlider(Qt::Horizontal, this);
    m_toleranceSlider->setRange(5, 60);
    m_toleranceSlider->setValue(18);
    m_toleranceValueLabel = new QLabel(QStringLiteral("18%"), this);
    m_toleranceValueLabel->setFixedWidth(35);
    tolLayout->addWidget(m_toleranceSlider, 1);
    tolLayout->addWidget(m_toleranceValueLabel);

    form->addRow(trText(QStringLiteral("dialog.mockup.tolerance"),
                        QStringLiteral("Tolerância")), tolLayout);

    mainLayout->addLayout(form);

    // Miniatura de Prévia com Checkerboard
    auto* previewBox = new QHBoxLayout;
    previewBox->setAlignment(Qt::AlignCenter);
    m_previewLabel = new QLabel(this);
    m_previewLabel->setFixedSize(120, 120);
    m_previewLabel->setStyleSheet(QStringLiteral("border: 1px solid #444; border-radius: 4px; background: #222;"));
    m_previewLabel->setAlignment(Qt::AlignCenter);
    previewBox->addWidget(m_previewLabel);
    mainLayout->addLayout(previewBox);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttons);

    // Conexões
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_browseBtn, &QPushButton::clicked, this, &MockupDialog::onBrowseClicked);
    connect(m_sourceCombo, &QComboBox::currentIndexChanged, this, &MockupDialog::onSourceChanged);
    connect(m_removeBackground, &QCheckBox::toggled, this, [this](bool checked) {
        m_toleranceSlider->setEnabled(checked);
        updatePreview();
    });
    connect(m_toleranceSlider, &QSlider::valueChanged, this, [this](int val) {
        m_toleranceValueLabel->setText(QStringLiteral("%1%").arg(val));
        updatePreview();
    });

    loadCurrentSourceLogo();
    updatePreview();
}

QString MockupDialog::trText(const QString& key, const QString& fallback) const
{
    if (m_i18n) {
        const QString translated = m_i18n->t(QStringLiteral("editor"), key);
        if (!translated.isEmpty() && translated != key)
            return translated;
    }
    return fallback;
}

void MockupDialog::loadCurrentSourceLogo()
{
    const QString existingId = m_sourceCombo->currentData().toString();
    if (!existingId.isEmpty() && m_document) {
        const LayerId sourceId(existingId);
        if (auto* sourceLayer = m_document->findLayer(sourceId); sourceLayer && sourceLayer->type() == LayerType::Image) {
            m_rawLogo = m_document->assets().decodedImage(static_cast<ImageLayer*>(sourceLayer)->assetId);
            return;
        }
    }

    if (!m_importedFilePath.isEmpty()) {
        m_rawLogo = QImage(m_importedFilePath);
    } else {
        m_rawLogo = QImage();
    }
}

void MockupDialog::onBrowseClicked()
{
    const QString path = QFileDialog::getOpenFileName(this,
        trText(QStringLiteral("dialog.mockup.title"), QStringLiteral("Escolher logo")),
        QString(),
        QStringLiteral("Imagens (*.png *.jpg *.jpeg *.webp *.bmp)"));
    if (path.isEmpty())
        return;

    m_importedFilePath = path;
    const QFileInfo fi(path);
    const QString displayName = QStringLiteral("📁 %1").arg(fi.fileName());

    // Se já estiver no combo, atualiza o texto, senão define no item 0
    m_sourceCombo->setItemText(0, displayName);
    m_sourceCombo->setCurrentIndex(0);

    loadCurrentSourceLogo();
    updatePreview();
}

void MockupDialog::onSourceChanged()
{
    loadCurrentSourceLogo();
    updatePreview();
}

void MockupDialog::updatePreview()
{
    if (m_rawLogo.isNull()) {
        m_processedLogo = QImage();
        m_previewLabel->setPixmap(QPixmap());
        m_previewLabel->setText(trText(QStringLiteral("dialog.mockup.preview"),
                                       QStringLiteral("Sem imagem")));
        return;
    }

    const bool removeBg = m_removeBackground->isChecked();
    const int tolerance = m_toleranceSlider->value();
    m_processedLogo = processLogo(m_rawLogo, removeBg, tolerance);

    // Desenha miniatura com checkerboard de transparência
    const int w = m_previewLabel->width();
    const int h = m_previewLabel->height();
    QPixmap pm(w, h);
    pm.fill(QColor(0x2d, 0x2e, 0x33));

    {
        QPainter p(&pm);
        // Checkerboard
        const int chSize = 8;
        for (int y = 0; y < h; y += chSize) {
            for (int x = 0; x < w; x += chSize) {
                const bool dark = ((x / chSize) ^ (y / chSize)) & 1;
                p.fillRect(x, y, chSize, chSize, dark ? QColor(0x35, 0x36, 0x3b) : QColor(0x40, 0x41, 0x47));
            }
        }

        // Desenha a imagem centralizada e escalada proporcionalmente
        if (!m_processedLogo.isNull()) {
            const QImage scaled = m_processedLogo.scaled(w - 12, h - 12, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            const int px = (w - scaled.width()) / 2;
            const int py = (h - scaled.height()) / 2;
            p.drawImage(px, py, scaled);
        }
    }

    m_previewLabel->setText(QString());
    m_previewLabel->setPixmap(pm);
}

MockupDialogResult MockupDialog::execute()
{
    MockupDialogResult res;

    // Se não tiver logo carregado ainda e clicar em OK, tenta navegar se for novo arquivo
    if (m_rawLogo.isNull() && m_sourceCombo->currentIndex() == 0) {
        onBrowseClicked();
    }

    if (exec() != QDialog::Accepted)
        return res;

    res.accepted = true;
    res.freeShape = m_modeCombo->currentData().toBool();
    res.logo = m_processedLogo.isNull() ? m_rawLogo : m_processedLogo;

    return res;
}

QImage MockupDialog::processLogo(const QImage& logo, bool removeBackground, int tolerance)
{
    return ImageProcessing::prepareMockupLogo(logo, removeBackground, tolerance);
}

} // namespace cc
