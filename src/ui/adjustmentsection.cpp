#include "adjustmentsection.h"

#include "core/Document.h"
#include "core/commands/AdjustmentLayerCommands.h"
#include "core/history/CommandStack.h"
#include "core/layers/Layer.h"
#include "localization/i18nservice.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace cc {

AdjustmentsSection::AdjustmentsSection(I18nService* i18n, QWidget* parent)
    : QWidget(parent)
    , m_i18n(i18n)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    m_typeCombo = new QComboBox(this);
    struct Entry { AdjustmentType type; const char* key; const char* fallback; };
    static const Entry entries[] = {
        {AdjustmentType::BrightnessContrast, "adjustments.brightnessContrast", "Brilho / Contraste"},
        {AdjustmentType::HueSaturation, "adjustments.hueSaturation", "Matiz / Saturação"},
        {AdjustmentType::ShadowsHighlights, "adjustments.shadowsHighlights", "Sombras / Realces"},
        {AdjustmentType::Vignette, "adjustments.vignette", "Vinheta"},
        {AdjustmentType::ChannelMixer, "adjustments.channelMixer", "Misturador de Canais"},
    };
    for (const auto& e : entries) {
        m_typeCombo->addItem(m_i18n ? m_i18n->t("editor", QString::fromLatin1(e.key))
                                    : QString::fromUtf8(e.fallback),
                             static_cast<int>(e.type));
    }
    layout->addWidget(m_typeCombo);

    auto makeSlider = [this, layout](QLabel*& label, int lo, int hi, int value) {
        auto* row = new QHBoxLayout;
        label = new QLabel(this);
        label->setMinimumWidth(72);
        auto* slider = new QSlider(Qt::Horizontal, this);
        slider->setRange(lo, hi);
        slider->setValue(value);
        // Não renderiza o documento a cada pixel percorrido durante o
        // arrasto. O valor visual acompanha o rato, mas valueChanged só é
        // emitido ao soltar, produzindo um único ajuste/undo e uma única
        // renderização pesada.
        slider->setTracking(false);
        row->addWidget(label);
        row->addWidget(slider);
        layout->addLayout(row);
        return slider;
    };

    m_sliderA = makeSlider(m_labelA, -100, 100, 0);
    m_sliderB = makeSlider(m_labelB, -100, 100, 0);
    m_sliderC = makeSlider(m_labelC, 0, 200, 100);

    auto* strengthRow = new QHBoxLayout;
    strengthRow->addWidget(new QLabel(tr("strength", QStringLiteral("Força")), this));
    m_strength = new QSlider(Qt::Horizontal, this);
    m_strength->setRange(0, 100);
    m_strength->setValue(100);
    m_strength->setTracking(false);
    strengthRow->addWidget(m_strength);
    layout->addLayout(strengthRow);

    connect(m_typeCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &AdjustmentsSection::onTypeChanged);
    for (QSlider* s : {m_sliderA, m_sliderB, m_sliderC, m_strength}) {
        connect(s, &QSlider::valueChanged, this, &AdjustmentsSection::onSliderChanged);
    }
}

QString AdjustmentsSection::tr(const char* key, const QString& fallback) const
{
    return m_i18n ? m_i18n->t("editor", QStringLiteral("adjustments.") + QLatin1String(key))
                  : fallback;
}

void AdjustmentsSection::setContext(Document* doc, CommandStack* history,
                                    const LayerId& imageLayerId)
{
    m_doc = doc;
    m_history = history;
    m_imageLayer = imageLayerId;
    setEnabled(doc != nullptr && !imageLayerId.isNull());
    loadFromLayer();
}

void AdjustmentsSection::loadFromLayer()
{
    m_loading = true;
    m_sliderC->setVisible(false);
    m_labelC->setVisible(false);

    const AdjustmentLayer* adj = nullptr;
    if (m_doc && !m_imageLayer.isNull()) {
        if (const Layer* layer = m_doc->findLayer(m_imageLayer)) {
            if (layer->type() == LayerType::Image) {
                const auto* img = static_cast<const ImageLayer*>(layer);
                if (img->adjustmentLayerId)
                    adj = m_doc->findAdjustmentLayer(*img->adjustmentLayerId);
            }
        }
    }

    auto param = [adj](const QString& key, double def) {
        if (!adj)
            return def;
        for (const auto& [k, v] : adj->parameters)
            if (k == key)
                return std::isfinite(v) ? v : def;
        return def;
    };

    const auto sliderValue = [](double value, int minimum, int maximum, int fallback) {
        if (!std::isfinite(value))
            return fallback;
        const double bounded = std::clamp(value, static_cast<double>(minimum),
                                          static_cast<double>(maximum));
        return static_cast<int>(bounded);
    };

    AdjustmentType type = AdjustmentType::BrightnessContrast;
    if (adj)
        type = adj->kind;
    for (int i = 0; i < m_typeCombo->count(); ++i) {
        if (static_cast<AdjustmentType>(m_typeCombo->itemData(i).toInt()) == type) {
            m_typeCombo->setCurrentIndex(i);
            break;
        }
    }

    // Rótulos e valores por tipo. Os nomes têm de coincidir com currentParams;
    // caso contrário, reabrir um ajuste e mover um slider podia trocar os
    // parâmetros por brightness/contrast indevidamente.
    QString keyA = QStringLiteral("brightness");
    QString keyB = QStringLiteral("contrast");
    QString labelA = tr("sliderA", "A");
    QString labelB = tr("sliderB", "B");
    switch (type) {
    case AdjustmentType::HueSaturation:
        keyA = QStringLiteral("hue");
        keyB = QStringLiteral("saturation");
        labelA = QStringLiteral("H");
        labelB = QStringLiteral("S");
        break;
    case AdjustmentType::ShadowsHighlights:
        keyA = QStringLiteral("shadows");
        keyB = QStringLiteral("highlights");
        labelA = QStringLiteral("Sh");
        labelB = QStringLiteral("Hi");
        break;
    case AdjustmentType::Vignette:
        keyA = QStringLiteral("vignette");
        labelA = QStringLiteral("V");
        break;
    case AdjustmentType::ChannelMixer:
        keyA = QStringLiteral("redGain");
        keyB = QStringLiteral("greenGain");
        labelA = QStringLiteral("R");
        labelB = QStringLiteral("G");
        break;
    default:
        break;
    }
    const bool channel = type == AdjustmentType::ChannelMixer;
    m_labelA->setText(labelA);
    m_labelB->setText(labelB);
    m_sliderA->setValue(sliderValue(param(keyA, channel ? 1.0 : 0.0)
                                          * (channel ? 100 : 1),
                                    m_sliderA->minimum(), m_sliderA->maximum(), 0));
    m_sliderB->setValue(sliderValue(param(keyB, channel ? 1.0 : 0.0)
                                          * (channel ? 100 : 1),
                                    m_sliderB->minimum(), m_sliderB->maximum(), 0));
    m_sliderC->setVisible(channel);
    m_labelC->setVisible(channel);
    if (channel)
        m_sliderC->setValue(sliderValue(param("blueGain", 1.0) * 100,
                                        m_sliderC->minimum(), m_sliderC->maximum(), 100));
    m_strength->setValue(sliderValue(adj ? adj->opacity() * 100 : 100.0,
                                     m_strength->minimum(), m_strength->maximum(), 100));

    m_loading = false;
}

AdjustmentParams AdjustmentsSection::currentParams() const
{
    const AdjustmentType type = static_cast<AdjustmentType>(
        m_typeCombo->currentData().toInt());
    const double a = m_sliderA->value();
    const double b = m_sliderB->value();

    switch (type) {
    case AdjustmentType::BrightnessContrast:
        return {{QStringLiteral("brightness"), a}, {QStringLiteral("contrast"), b}};
    case AdjustmentType::HueSaturation:
        return {{QStringLiteral("hue"), a}, {QStringLiteral("saturation"), b}};
    case AdjustmentType::ShadowsHighlights:
        return {{QStringLiteral("shadows"), a}, {QStringLiteral("highlights"), b}};
    case AdjustmentType::Vignette:
        return {{QStringLiteral("vignette"), a}};
    case AdjustmentType::ChannelMixer:
        return {{QStringLiteral("redGain"), a / 100.0},
                {QStringLiteral("greenGain"), b / 100.0},
                {QStringLiteral("blueGain"), m_sliderC->value() / 100.0}};
    default:
        return {};
    }
}

void AdjustmentsSection::onTypeChanged(int)
{
    if (m_loading)
        return;
    loadFromLayer();
    onSliderChanged();
}

void AdjustmentsSection::onSliderChanged()
{
    if (m_loading || !m_doc || m_imageLayer.isNull())
        return;

    const AdjustmentType type = static_cast<AdjustmentType>(
        m_typeCombo->currentData().toInt());
    const AdjustmentParams params = currentParams();
    const float strength = std::clamp(m_strength->value() / 100.0f, 0.0f, 1.0f);

    // Camada ligada existente? Actualizar; senão criar.
    const Layer* imageLayer = m_doc->findLayer(m_imageLayer);
    if (!imageLayer || imageLayer->type() != LayerType::Image)
        return;
    const auto* img = static_cast<const ImageLayer*>(imageLayer);
    LayerId adjId = img->adjustmentLayerId.value_or(LayerId{});
    const bool hasLinked = !adjId.isNull() && m_doc->findAdjustmentLayer(adjId);

    if (!m_history) {
        // Sem histórico: aplica directo (nunca perder a edição).
        if (!hasLinked) {
            adjId = m_doc->createAdjustmentLayer(type, m_imageLayer);
            m_doc->setLayerAdjustmentLayerId(m_imageLayer, adjId);
        } else {
            m_doc->setAdjustmentKind(adjId, type);
        }
        m_doc->setAdjustmentParameters(adjId, params);
        m_doc->setLayerOpacity(adjId, strength);
        emit adjustmentChanged();
        return;
    }

    if (hasLinked) {
        const AdjustmentLayer* adj = m_doc->findAdjustmentLayer(adjId);
        const AdjustmentType oldKind = adj->kind;
        const AdjustmentParams oldParams = adj->parameters;
        const float oldStrength = adj->opacity();

        // Força: comando de opacidade separado e fundível.
        if (oldStrength != strength) {
            m_history->execute(std::make_unique<LayerOpacityCommand>(
                *m_doc, adjId, QStringLiteral("adjustment.strength"),
                &Document::setLayerOpacity, oldStrength, strength));
        }
        if (oldKind != type || oldParams != params) {
            m_history->execute(std::make_unique<UpdateAdjustmentLayerCommand>(
                *m_doc, adjId, oldKind, type, oldParams, params,
                QStringLiteral("adjustment.update")));
        }
    } else {
        auto cmd = std::make_unique<CreateAdjustmentLayerCommand>(
            *m_doc, type, params, m_imageLayer, QStringLiteral("adjustment.create"), strength);
        m_history->execute(std::move(cmd));
    }
    emit adjustmentChanged();
}

void AdjustmentsSection::onStrengthChanged()
{
    onSliderChanged();
}

} // namespace cc
