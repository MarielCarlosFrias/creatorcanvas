#pragma once

#include <QDialog>
#include <QImage>

class QSlider;
class QLabel;
class QTimer;

namespace cc {

class I18nService;

/// Painel Refine Edge (avançado): suavizar, expandir/contrair e esbatar a
/// selecção activa, com PRÉVIA AO VIVO da máscara resultante. Todo o
/// trabalho é do pipeline puro `SelectionEngine::refine` — o diálogo só
/// passa parâmetros e devolve a máscara final.
class RefineEdgeDialog final : public QDialog
{
    Q_OBJECT

public:
    RefineEdgeDialog(const QImage& currentSelection,
                     I18nService* i18n = nullptr, QWidget* parent = nullptr);
    ~RefineEdgeDialog() override = default;

    /// Máscara refinada (Alpha8, mesmo tamanho da original).
    QImage refinedSelection() const;

private slots:
    void onParamChanged();

private:
    QString tr(const char* key, const QString& fallback) const;
    void updatePreview();

    QImage m_original;
    I18nService* m_i18n = nullptr;

    QSlider* m_smooth = nullptr;
    QSlider* m_expand = nullptr;
    QSlider* m_feather = nullptr;
    QLabel* m_preview = nullptr;
    QTimer* m_previewTimer = nullptr;
};

} // namespace cc
