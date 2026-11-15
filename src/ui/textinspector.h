#pragma once

#include <QWidget>

#include "tools/CanvasTool.h"

#include <QPointer>
#include "core/Document.h"
#include "core/DocumentController.h"
#include "services/textstylestore.h"

#include <optional>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QDoubleSpinBox;
class QFontComboBox;
class QPlainTextEdit;
class QMessageBox;
class QPushButton;
class QLabel;
class QSpinBox;
class QSlider;
class QToolButton;

namespace cc {

class I18nService;
class CollapsibleSection;
class CommandStack;

/// Property panel for TextLayers: content, font, size, style, color, align, effects.
/// Edits are executed via CommandStack for full Undo/Redo support.
class TextInspector final : public QWidget, public IDocumentObserver
{
    Q_OBJECT
public:
    explicit TextInspector(I18nService* i18n, Document* document,
                           CommandStack* history = nullptr,
                           QWidget* parent = nullptr);
    ~TextInspector() override;

    void onDocumentChanged(Document* doc) override { setDocument(doc); }
    void setSelectedLayer(const LayerId& id);
    void setDocument(Document* document);
    void setHistory(CommandStack* history) { m_history = history; }

    void refresh();
    void commitPendingTextChange();
    /// Modo avançado: revela as secções profissionais (estilos, texto em
    /// caminho).
    void setAdvancedMode(bool advanced);

    /// Catálogo de estilos (built-ins + utilizador). Sem dono próprio: o
    /// MainWindow partilha a instância que já cria para os presets.
    void setStyleStore(cc::TextStyleStore* store);

    /// Lista os caminhos (VectorLayers) disponíveis para texto em caminho.
    void refreshPathCombo();

private:
    void onPathLinkChanged(int index);
    void onPathOffsetChanged(int value);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void buildUi();
    void applyTextStyle(const TextStyle& style);
    void saveCurrentAsStyle();
    void refreshStyleCombo();
    void retranslateUi();
    bool editingLayer(TextLayer** out) const;
    void touch();

    I18nService* m_i18n = nullptr;
    QPointer<Document> m_document;
    CommandStack* m_history = nullptr;
    LayerId m_id;
    bool m_loading = false;
    class QTimer* m_textDebounceTimer = nullptr;
    QString m_initialTextContent;
    bool m_isTypingText = false;

    QPlainTextEdit* m_content = nullptr;
    QFontComboBox* m_fontFamily = nullptr;

    // Presets de estilo (modo avançado).
    CollapsibleSection* m_styleSection = nullptr;
    QComboBox* m_styleCombo = nullptr;
    QPushButton* m_styleApplyBtn = nullptr;
    QPushButton* m_styleSaveBtn = nullptr;
    QPushButton* m_styleRemoveBtn = nullptr;
    cc::TextStyleStore* m_styleStore = nullptr;

    // Texto em caminho (modo avançado).
    CollapsibleSection* m_pathSection = nullptr;
    QComboBox* m_pathCombo = nullptr;
    QLabel* m_pathHintLabel = nullptr;
    QPushButton* m_createPathButton = nullptr;
    QSlider* m_pathOffset = nullptr;
    QSpinBox* m_size = nullptr;
    QToolButton* m_bold = nullptr;
    QToolButton* m_italic = nullptr;
    QToolButton* m_underline = nullptr;
    QPushButton* m_color = nullptr;
    QComboBox* m_align = nullptr;
    QDoubleSpinBox* m_letterSpacing = nullptr;
    QDoubleSpinBox* m_lineHeight = nullptr;

    QLabel* m_contentLabel = nullptr;
    QLabel* m_fontLabel = nullptr;
    QLabel* m_sizeLabel = nullptr;
    QLabel* m_styleLabel = nullptr;
    QLabel* m_colorLabel = nullptr;
    QLabel* m_alignLabel = nullptr;
    QLabel* m_letterSpacingLabel = nullptr;
    QLabel* m_lineHeightLabel = nullptr;

    QLabel* m_effectsLabel = nullptr;
    QCheckBox* m_outlineOn = nullptr;
    QPushButton* m_outlineColor = nullptr;
    QDoubleSpinBox* m_outlineWidth = nullptr;
    QCheckBox* m_shadowOn = nullptr;
    QPushButton* m_shadowColor = nullptr;
    QDoubleSpinBox* m_shadowX = nullptr;
    QDoubleSpinBox* m_shadowY = nullptr;
    QDoubleSpinBox* m_shadowBlur = nullptr;

    // Interior Transparente / Vazado e Efeito Knockout (Chroma Cut)
    QCheckBox* m_hollowOn = nullptr;
    QCheckBox* m_knockoutOn = nullptr;

    // Gradiente de Texto
    QCheckBox* m_gradientOn = nullptr;
    QComboBox* m_gradientType = nullptr;
    QPushButton* m_gradientStartColor = nullptr;
    QPushButton* m_gradientEndColor = nullptr;
    QDoubleSpinBox* m_gradientAngle = nullptr;

    // Textura de Preenchimento (Texture / Image Fill)
    QCheckBox* m_textureOn = nullptr;
    QPushButton* m_textureChooseBtn = nullptr;
    QPushButton* m_textureClearBtn = nullptr;
    QLabel* m_textureNameLabel = nullptr;
    QDoubleSpinBox* m_textureOpacity = nullptr;
    QLabel* m_textureOpacityLabel = nullptr;

    // Perspectiva 3D / Inclinação (Tilt / Shear)
    QLabel* m_perspectiveLabel = nullptr;
    QDoubleSpinBox* m_tiltX = nullptr;
    QDoubleSpinBox* m_tiltY = nullptr;

    // Seções recolhíveis
    class CollapsibleSection* m_typographySection = nullptr;
    class CollapsibleSection* m_effectsSection = nullptr;
    class CollapsibleSection* m_gradientSection = nullptr;
    class CollapsibleSection* m_textureSection = nullptr;
    class CollapsibleSection* m_perspectiveSection = nullptr;

signals:
    /// Pede à janela para activar uma ferramenta (ex.: a caneta, para
    /// desenhar o caminho do texto). A inspect GUI não mexe em ferramentas
    /// directamente.
    void requestTool(CanvasTool tool);

};

} // namespace cc
