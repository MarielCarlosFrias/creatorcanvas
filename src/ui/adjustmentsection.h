#pragma once

#include "core/image/AdjustmentEngine.h"
#include "core/layers/Layer.h"

#include <QWidget>

class QComboBox;
class QSlider;
class QLabel;

namespace cc {

class Document;
class CommandStack;
class I18nService;

/// Secção viva de ajustes para o inspector de imagem (modo avançado):
/// escolhe o tipo de ajuste e mexe nos parâmetros, escrevendo sempre na
/// camada de ajuste LIGADA à camada de imagem — nunca cozendo pixéis.
///
/// Sem camada ligada, o primeiro ajuste cria uma (`CreateAdjustmentLayerCommand`);
/// a partir daí tudo é `UpdateAdjustmentLayerCommand` (fundível: arrastar um
/// slider = um passo de undo).
class AdjustmentsSection final : public QWidget
{
    Q_OBJECT

public:
    AdjustmentsSection(I18nService* i18n, QWidget* parent = nullptr);

    void setContext(Document* doc, CommandStack* history, const LayerId& imageLayerId);

private slots:
    void onTypeChanged(int index);
    void onSliderChanged();
    void onStrengthChanged();

private:
    void loadFromLayer();
    QString tr(const char* key, const QString& fallback) const;
    AdjustmentParams currentParams() const;

    I18nService* m_i18n = nullptr;
    Document* m_doc = nullptr;
    CommandStack* m_history = nullptr;
    LayerId m_imageLayer;
    bool m_loading = false;

    QComboBox* m_typeCombo = nullptr;
    QSlider* m_sliderA = nullptr;
    QSlider* m_sliderB = nullptr;
    QSlider* m_sliderC = nullptr;
    QLabel* m_labelA = nullptr;
    QLabel* m_labelB = nullptr;
    QLabel* m_labelC = nullptr;
    QSlider* m_strength = nullptr;
};

} // namespace cc
