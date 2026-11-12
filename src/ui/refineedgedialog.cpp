#include "refineedgedialog.h"

#include "core/selection/SelectionEngine.h"
#include "localization/i18nservice.h"

#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPushButton>

namespace cc {

RefineEdgeDialog::RefineEdgeDialog(const QImage& currentSelection,
                                   I18nService* i18n, QWidget* parent)
    : QDialog(parent)
    , m_original(SelectionEngine::normalize(currentSelection))
    , m_i18n(i18n)
{
    setWindowTitle(tr("title", QStringLiteral("Refinar Borda da Selecção - CreatorCanvas")));
    setMinimumSize(360, 300);

    auto* layout = new QVBoxLayout(this);

    auto makeRow = [this, layout](const QString& label, int lo, int hi, int value,
                                  QSlider*& out) {
        auto* row = new QHBoxLayout;
        auto* labelWidget = new QLabel(label, this);
        labelWidget->setMinimumWidth(90);
        auto* slider = new QSlider(Qt::Horizontal, this);
        slider->setRange(lo, hi);
        slider->setValue(value);
        row->addWidget(labelWidget);
        row->addWidget(slider);
        layout->addLayout(row);
        out = slider;
        connect(slider, &QSlider::valueChanged, this, &RefineEdgeDialog::onParamChanged);
    };

    makeRow(tr("smooth", QStringLiteral("Suavizar")), 0, 40, 0, m_smooth);
    makeRow(tr("expand", QStringLiteral("Expandir/Contrair")), -40, 40, 0, m_expand);
    makeRow(tr("feather", QStringLiteral("Esbater")), 0, 40, 0, m_feather);

    m_preview = new QLabel(this);
    m_preview->setMinimumHeight(140);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setStyleSheet(QStringLiteral(
        "background-color: #202227; border: 1px solid #2b303c; border-radius: 6px;"));
    layout->addWidget(m_preview, 1);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    auto* cancelBtn = new QPushButton(QStringLiteral("Cancelar"), this);
    auto* applyBtn = new QPushButton(QStringLiteral("✓ Aplicar"), this);
    applyBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #22c55e; color: white; font-weight: bold; padding: 6px 18px; border-radius: 4px; }"));
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(applyBtn, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addWidget(cancelBtn);
    buttons->addWidget(applyBtn);
    layout->addLayout(buttons);

    updatePreview();
}

QString RefineEdgeDialog::tr(const char* key, const QString& fallback) const
{
    return m_i18n ? m_i18n->t("editor", QStringLiteral("refineEdge.") + QLatin1String(key))
                  : fallback;
}

QImage RefineEdgeDialog::refinedSelection() const
{
    return SelectionEngine::refine(m_original,
                                   static_cast<float>(m_smooth ? m_smooth->value() : 0),
                                   static_cast<float>(m_expand ? m_expand->value() : 0),
                                   static_cast<float>(m_feather ? m_feather->value() : 0));
}

void RefineEdgeDialog::onParamChanged()
{
    updatePreview();
}

void RefineEdgeDialog::updatePreview()
{
    if (!m_preview || m_original.isNull())
        return;

    // Prévia em escala reduzida (a máscara pode ter os pixels do documento).
    const QImage mask = refinedSelection();
    const int maxSide = 240;
    QImage small = mask;
    if (std::max(small.width(), small.height()) > maxSide) {
        const double s = static_cast<double>(maxSide)
                         / std::max(small.width(), small.height());
        small = small.scaled(std::max(1, qRound(small.width() * s)),
                             std::max(1, qRound(small.height() * s)),
                             Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    // Desenha a máscara como véu azul sobre fundo escuro: seleccionado =
    // azul claro, fora = escuro; valores intermédios vêem-se como borda suave.
    QImage img(small.size(), QImage::Format_ARGB32);
    img.fill(QColor(0x16, 0x18, 0x1d));
    for (int y = 0; y < small.height(); ++y) {
        const uchar* maskRow = small.constScanLine(y);
        QRgb* out = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < small.width(); ++x) {
            const int v = maskRow[x];
            if (v == 0)
                continue;
            out[x] = qRgba(0x43 + (0x9e - 0x43) * v / 255,
                           0x78 + (0xd8 - 0x78) * v / 255,
                           0xff, 0xff);
        }
    }
    m_preview->setPixmap(QPixmap::fromImage(img));
}

} // namespace cc
