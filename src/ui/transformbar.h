#pragma once

#include "core/geometry/AffineTransform.h"
#include "core/layers/Layer.h"

#include <QPair>
#include <QWidget>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;

namespace cc {

/// Redimensiona uma camada de imagem a partir de valores em píxeis,
/// preservando o sinal de espelho (scaleX/scaleY negativos) que a camada
/// já tinha. Funções puras — testadas sem widget.
AffineTransform numericResize(const AffineTransform& base, int naturalWidth,
                              int naturalHeight, double w, double h);

/// Com a proporção travada, devolve o par (largura, altura) consistente:
/// `fromWidth=true` deriva a altura da largura, e vice-versa.
QPair<double, double> lockAspect(double w, double h, int naturalWidth, int naturalHeight,
                                 bool fromWidth);

class Document;
class I18nService;

/// Barra de transformação numérica (estilo Photoshop): X/Y, W/H e rotação
/// da camada seleccionada. Edições são emitidas uma a uma e o histórico
/// funde as que vêm do mesmo gesto (SetLayerTransformCommand tem merge),
/// pelo que arrastar um spin faz UM passo de undo.
///
/// W/H só fazem sentido para camadas com tamanho intrínseco — hoje imagem
/// (escala = dimensão / natural). Para os restantes tipos os campos ficam
/// desabilitados em vez de mentirem.
class TransformBar final : public QWidget
{
    Q_OBJECT

public:
    explicit TransformBar(QWidget* parent = nullptr);

    void setI18n(I18nService* i18n);

    /// Popula os campos da camada (ou limpa tudo se não houver camada).
    void setLayer(const Document& doc, const LayerId& id);

signals:
    /// Emitido a cada edição de campo; `oldTransform` é o estado antes
    /// desta edição concreta (para o comando de histórico).
    void transformEdited(const LayerId& id, AffineTransform oldTransform,
                         AffineTransform newTransform);

private slots:
    void onFieldEdited();

private:
    void retranslate();
    void emitEdit();

    I18nService* m_i18n = nullptr;
    LayerId m_id;
    AffineTransform m_current;
    int m_naturalWidth = 0;
    int m_naturalHeight = 0;
    bool m_loading = false;

    QDoubleSpinBox* m_xSpin = nullptr;
    QDoubleSpinBox* m_ySpin = nullptr;
    QDoubleSpinBox* m_wSpin = nullptr;
    QDoubleSpinBox* m_hSpin = nullptr;
    QDoubleSpinBox* m_rotSpin = nullptr;
    QCheckBox* m_aspectLock = nullptr;
};

} // namespace cc
