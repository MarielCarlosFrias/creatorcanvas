#include "refineedgedialog.h"

#include "core/selection/SelectionEngine.h"
#include "localization/i18nservice.h"

#include <QLabel>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPushButton>

#include <algorithm>

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

    // Debounce: o refine percorre a máscara inteira várias vezes; durante o
    // arrasto do slider recalculamos só após uma pausa curta.
    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(80);
    connect(m_previewTimer, &QTimer::timeout, this, &RefineEdgeDialog::updatePreview);

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
    auto* cancelBtn = new QPushButton(tr("cancel", QStringLiteral("Cancel")), this);
    auto* applyBtn = new QPushButton(tr("apply", QStringLiteral("✓ Apply")), this);
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
    if (m_previewTimer)
        m_previewTimer->start();
}

void RefineEdgeDialog::updatePreview()
{
    if (!m_preview || m_original.isNull())
        return;

    // Prévia em escala reduzida: o refine é O(N·raio) sobre a máscara inteira,
    // por isso refinamos uma versão pequena (raios proporcionais) e só o
    // resultado final (aceite) usa a máscara à resolução original.
    const int maxSide = 320;
    const int ow = m_original.width();
    const int oh = m_original.height();
    const double scale = std::min(1.0, static_cast<double>(maxSide)
                                      / std::max(ow, oh));
    QImage small = m_original;
    if (scale < 1.0) {
        small = small.scaled(std::max(1, qRound(ow * scale)),
                             std::max(1, qRound(oh * scale)),
                             Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    const QImage mask = SelectionEngine::refine(
        small,
        static_cast<float>((m_smooth ? m_smooth->value() : 0) * scale),
        static_cast<float>((m_expand ? m_expand->value() : 0) * scale),
        static_cast<float>((m_feather ? m_feather->value() : 0) * scale));

    // Desenha a máscara como véu azul sobre fundo escuro: seleccionado =
    // azul claro, fora = escuro; valores intermédios vêem-se como borda suave.
    QImage img(mask.size(), QImage::Format_ARGB32);
    img.fill(QColor(0x16, 0x18, 0x1d));
    for (int y = 0; y < mask.height(); ++y) {
        const uchar* maskRow = mask.constScanLine(y);
        QRgb* out = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < mask.width(); ++x) {
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
