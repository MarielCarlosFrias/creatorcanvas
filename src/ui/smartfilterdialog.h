#pragma once

#include "core/image/AdjustmentEngine.h"

#include <QDialog>
#include <QImage>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QResizeEvent;
class QTimer;

namespace cc {

class I18nService;

/// Pedido produzido pelo diálogo. O chamador decide o Document/CommandStack
/// de destino e executa-o com executeApplySmartFilter().
struct SmartFilterRequest
{
    AdjustmentType type = AdjustmentType::Blur;
    AdjustmentParams params;
    bool nonDestructive = true;
    bool valid = false;
};

/// Editor de Blur/MotionBlur/Noise/Twirl/Pinch/Ripple com prévia ao vivo.
/// Não destrutivo é a opção default: cria uma AdjustmentLayer ligada e mantém
/// tipo e parâmetros editáveis. Desmarcar a opção selecciona o caminho
/// destrutivo, que cria um novo asset ao ser executado pelo chamador.
class SmartFilterDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit SmartFilterDialog(const QImage& source,
                               AdjustmentType initial = AdjustmentType::Blur,
                               I18nService* i18n = nullptr,
                               QWidget* parent = nullptr);

    SmartFilterRequest request() const;
    QImage previewImage() const { return m_preview; }

    void accept() override;

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void buildUi();
    void onFilterChanged(int index);
    void schedulePreviewUpdate();
    void updatePreview();
    void showPreviewPixmap();
    AdjustmentParams currentParams(AdjustmentType type) const;
    QString text(const char* key, const QString& fallback) const;

    QImage m_source;
    QImage m_preview;
    I18nService* m_i18n = nullptr;
    bool m_buildingUi = false;
    QTimer* m_previewTimer = nullptr;

    QComboBox* m_filterCombo = nullptr;
    QLabel* m_previewLabel = nullptr;
    QLabel* m_wavelengthLabel = nullptr;
    QCheckBox* m_nonDestructiveCheck = nullptr;
    QCheckBox* m_monochromeCheck = nullptr;

    QDoubleSpinBox* m_radius = nullptr;
    QDoubleSpinBox* m_angle = nullptr;
    QDoubleSpinBox* m_length = nullptr;
    QDoubleSpinBox* m_noiseAmount = nullptr;
    QDoubleSpinBox* m_twirlAmount = nullptr;
    QDoubleSpinBox* m_pinchAmount = nullptr;
    QDoubleSpinBox* m_rippleAmount = nullptr;
    QDoubleSpinBox* m_wavelength = nullptr;
};

} // namespace cc
