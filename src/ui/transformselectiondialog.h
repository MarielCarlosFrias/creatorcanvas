#pragma once

#include "services/SelectionTransform.h"

#include <QDialog>
#include <QImage>

class QDialogButtonBox;
class QDoubleSpinBox;
class QLabel;
class QTimer;

namespace cc {

class I18nService;

/// Edita numericamente o contorno da selecção activa, sem tocar no conteúdo
/// das camadas. A máscara original permanece intacta até o chamador aplicar
/// parameters() através de SelectionTransform::apply(Document&, ...).
class TransformSelectionDialog final : public QDialog
{
public:
    explicit TransformSelectionDialog(const QImage& currentSelection,
                                      I18nService* i18n = nullptr,
                                      QWidget* parent = nullptr);
    ~TransformSelectionDialog() override = default;

    SelectionTransformParameters parameters() const;
    QImage transformedSelection() const;

private:
    QString text(const QString& key, const QString& fallback) const;
    void resetParameters();
    void updatePreview();

    QImage m_original;
    I18nService* m_i18n = nullptr;

    QDoubleSpinBox* m_translateX = nullptr;
    QDoubleSpinBox* m_translateY = nullptr;
    QDoubleSpinBox* m_scaleX = nullptr;
    QDoubleSpinBox* m_scaleY = nullptr;
    QDoubleSpinBox* m_rotation = nullptr;
    QLabel* m_preview = nullptr;
    QDialogButtonBox* m_buttons = nullptr;
    QTimer* m_previewTimer = nullptr;
};

} // namespace cc
