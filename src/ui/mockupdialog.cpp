#include "mockupdialog.h"
#include "core/image/ImageProcessing.h"

#include <QComboBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QtMath>

namespace cc {

MockupDialog::MockupDialog(QWidget* parent, Document* document, const LayerId& currentLayerId)
    : QDialog(parent)
    , m_document(document)
    , m_currentLayerId(currentLayerId)
{
    setWindowTitle(QStringLiteral("Escolher imagem para o mockup"));
    auto* form = new QFormLayout(this);

    m_sourceCombo = new QComboBox(this);
    m_sourceCombo->addItem(QStringLiteral("Importar uma imagem..."), QString());
    if (m_document && m_document->rootGroup()) {
        for (const auto& child : m_document->rootGroup()->children) {
            if (!child || child->type() != LayerType::Image || child->id() == m_currentLayerId)
                continue;
            const auto* imageLayer = static_cast<const ImageLayer*>(child.get());
            m_sourceCombo->addItem(imageLayer->name.isEmpty() ? QStringLiteral("Imagem existente") : imageLayer->name,
                                  imageLayer->id().toString(QUuid::WithoutBraces));
        }
    }
    form->addRow(QStringLiteral("Imagem fonte"), m_sourceCombo);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(QStringLiteral("Perspectiva (4 cantos) — como Photoshop Distort"), false);
    m_modeCombo->addItem(QStringLiteral("Forma livre (qualquer formato — contorno inteiro)"), true);
    m_modeCombo->setToolTip(
        QStringLiteral("Perspectiva: marque os 4 cantos da área; a imagem é encaixada em "
                       "perspectiva exata (ideal para estradas/placas).\n"
                       "Forma livre: marque o contorno inteiro (5+ pontos, qualquer "
                       "formato); a imagem preenche a forma, seguindo as arestas."));
    form->addRow(QStringLiteral("Modo"), m_modeCombo);

    m_removeBackground = new QCheckBox(QStringLiteral("Remover fundo uniforme (Varinha Mágica)"), this);
    m_removeBackground->setToolTip(QStringLiteral("Útil para logos com fundo sólido (branco, magenta, etc.). Removido antes do warp para que só o conteúdo siga os pontos. O ficheiro original não é alterado."));
    m_removeBackground->setChecked(true);
    form->addRow(m_removeBackground);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

MockupDialogResult MockupDialog::execute()
{
    MockupDialogResult res;
    if (exec() != QDialog::Accepted)
        return res;

    res.accepted = true;
    res.freeShape = m_modeCombo->currentData().toBool();

    QImage logo;
    const QString existingId = m_sourceCombo->currentData().toString();
    if (!existingId.isEmpty() && m_document) {
        const LayerId sourceId(existingId);
        if (auto* sourceLayer = m_document->findLayer(sourceId); sourceLayer && sourceLayer->type() == LayerType::Image)
            logo = m_document->assets().decodedImage(static_cast<ImageLayer*>(sourceLayer)->assetId);
    } else {
        const QString logoPath = QFileDialog::getOpenFileName(this, QStringLiteral("Escolher logo"), QString(),
            QStringLiteral("Imagens (*.png *.jpg *.jpeg *.webp *.bmp)"));
        if (!logoPath.isEmpty())
            logo = QImage(logoPath);
    }

    if (!logo.isNull()) {
        res.logo = processLogo(logo, m_removeBackground->isChecked());
    }

    return res;
}

QImage MockupDialog::processLogo(const QImage& logo, bool removeBackground)
{
    if (logo.isNull() || !removeBackground)
        return logo;

    const QColor bgColor = logo.pixelColor(QPoint(0, 0));
    QImage cleaned = logo;
    const QPoint corners[4] = {
        QPoint(0, 0),
        QPoint(cleaned.width() - 1, 0),
        QPoint(0, cleaned.height() - 1),
        QPoint(cleaned.width() - 1, cleaned.height() - 1)
    };
    for (const QPoint& corner : corners)
        cleaned = ImageProcessing::removeBackground(cleaned, corner, 18, true);

    if (bgColor.isValid()) {
        if (cleaned.format() != QImage::Format_ARGB32)
            cleaned = cleaned.convertToFormat(QImage::Format_ARGB32);
        const double threshold = (18 / 100.0) * 441.67295593;
        for (int pass = 0; pass < 2; ++pass) {
            QImage haloFree = cleaned;
            for (int y = 0; y < haloFree.height(); ++y) {
                QRgb* line = reinterpret_cast<QRgb*>(haloFree.scanLine(y));
                for (int x = 0; x < haloFree.width(); ++x) {
                    const QRgb px = line[x];
                    if (qAlpha(px) == 0)
                        continue;
                    bool touchesTransparent = false;
                    if (x > 0 && qAlpha(line[x - 1]) == 0) touchesTransparent = true;
                    if (x + 1 < haloFree.width() && qAlpha(line[x + 1]) == 0) touchesTransparent = true;
                    if (y > 0 && qAlpha(reinterpret_cast<const QRgb*>(haloFree.constScanLine(y - 1))[x]) == 0) touchesTransparent = true;
                    if (y + 1 < haloFree.height() && qAlpha(reinterpret_cast<const QRgb*>(haloFree.constScanLine(y + 1))[x]) == 0) touchesTransparent = true;
                    if (!touchesTransparent)
                        continue;
                    const double dr = qRed(px) - bgColor.red();
                    const double dg = qGreen(px) - bgColor.green();
                    const double db = qBlue(px) - bgColor.blue();
                    const double dist = qSqrt(dr * dr + dg * dg + db * db);
                    if (dist < threshold)
                        line[x] = qRgba(qRed(px), qGreen(px), qBlue(px), 0);
                    else if (dist < 2.0 * threshold)
                        line[x] = qRgba(qRed(px), qGreen(px), qBlue(px),
                                        qRound(qAlpha(px) * ((dist - threshold) / threshold)));
                }
            }
            cleaned = haloFree;
        }
    }

    QRect content(0, 0, -1, -1);
    for (int y = 0; y < cleaned.height(); ++y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(cleaned.constScanLine(y));
        for (int x = 0; x < cleaned.width(); ++x) {
            if (qAlpha(line[x]) > 8) {
                if (!content.isValid()) content = QRect(x, y, 1, 1);
                else content = content.united(QRect(x, y, 1, 1));
            }
        }
    }
    if (content.isValid() && content != cleaned.rect()) {
        const QImage cropped = ImageProcessing::cropImage(cleaned, content);
        if (!cropped.isNull())
            cleaned = cropped;
    }

    return cleaned;
}

} // namespace cc
