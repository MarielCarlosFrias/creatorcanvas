#pragma once

#include <QWidget>
#include <QPointer>
#include "core/Document.h"

// Forward declarations de classes Qt necessárias para a interface gráfica
class QDoubleSpinBox;
class QPushButton;
class QLabel;
class QGroupBox;
class QFormLayout;

namespace cc {

class I18nService;
class CommandStack;

/// Painel de propriedades para ShapeLayers (retângulos, elipses, linhas, etc.):
/// Permite alterar a cor de preenchimento, cor de contorno, espessura da linha e raio dos cantos.
/// Projetado com suporte a C++20/Qt6 e compatibilidade multiplataforma (Linux/Windows).
class ShapeInspector final : public QWidget
{
    Q_OBJECT
public:
    explicit ShapeInspector(I18nService* i18n, Document* document,
                            CommandStack* history = nullptr,
                            QWidget* parent = nullptr);

    // Define qual camada está selecionada atualmente no canvas
    void setSelectedLayer(const LayerId& id);

    // Atualiza o ponteiro do documento ativo (ex: ao criar ou abrir outro projeto)
    void setDocument(Document* document);

    // Recarrega todos os valores da camada selecionada para a interface
    void refresh();

private:
    // Constrói os layouts e widgets visuais
    void buildUi();

    // Atualiza os textos dos labels com base no idioma atual
    void retranslateUi();

    // Obtém o ponteiro tipado para a ShapeLayer sendo editada
    bool editingLayer(ShapeLayer** out) const;

    // Atualiza o estilo visual do botão de cor (preenchimento ou contorno)
    static void updateColorButton(QPushButton* button, const QColor& color);

    I18nService* m_i18n = nullptr;
    QPointer<Document> m_document;
    CommandStack* m_history = nullptr;
    LayerId m_id;
    bool m_loading = false;

    // Widgets para preenchimento, contorno e cantos
    QGroupBox* m_groupBox = nullptr;
    QFormLayout* m_formLayout = nullptr;
    QPushButton* m_fillColorBtn = nullptr;
    QPushButton* m_strokeColorBtn = nullptr;
    QDoubleSpinBox* m_strokeWidthSpin = nullptr;
    QDoubleSpinBox* m_cornerRadiusSpin = nullptr;

    QLabel* m_fillLabel = nullptr;
    QLabel* m_strokeLabel = nullptr;
    QLabel* m_strokeWidthLabel = nullptr;
    QLabel* m_cornerRadiusLabel = nullptr;
};

} // namespace cc
