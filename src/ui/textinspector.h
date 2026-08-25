#pragma once

#include <QWidget>

#include "core/Document.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFontComboBox;
class QPlainTextEdit;
class QPushButton;
class QLabel;
class QSpinBox;
class QToolButton;

namespace cc {

class I18nService;

/// Property panel for TextLayers: content, font, size, style, color, align.
/// Changes apply directly (canvas + layers panel update live). Undo for
/// inspector edits arrives with the Phase 2 polish.
class TextInspector final : public QWidget
{
    Q_OBJECT
public:
    explicit TextInspector(I18nService* i18n, Document* document,
                           QWidget* parent = nullptr);

    void setSelectedLayer(const LayerId& id);
    void refresh();

private:
    void buildUi();
    void retranslateUi();
    void loadFromLayer();
    bool editingLayer(TextLayer** out) const;
    void touch();

    I18nService* m_i18n = nullptr;
    Document* m_document = nullptr;
    LayerId m_id;
    bool m_loading = false;

    QPlainTextEdit* m_content = nullptr;
    QFontComboBox* m_fontFamily = nullptr;
    QSpinBox* m_size = nullptr;
    QToolButton* m_bold = nullptr;
    QToolButton* m_italic = nullptr;
    QToolButton* m_underline = nullptr;
    QPushButton* m_color = nullptr;
    QComboBox* m_align = nullptr;

    QLabel* m_contentLabel = nullptr;
    QLabel* m_fontLabel = nullptr;
    QLabel* m_sizeLabel = nullptr;
    QLabel* m_styleLabel = nullptr;
    QLabel* m_colorLabel = nullptr;
    QLabel* m_alignLabel = nullptr;

    QLabel* m_effectsLabel = nullptr;
    QCheckBox* m_outlineOn = nullptr;
    QPushButton* m_outlineColor = nullptr;
    QDoubleSpinBox* m_outlineWidth = nullptr;
    QCheckBox* m_shadowOn = nullptr;
    QPushButton* m_shadowColor = nullptr;
    QDoubleSpinBox* m_shadowX = nullptr;
    QDoubleSpinBox* m_shadowY = nullptr;
    QDoubleSpinBox* m_shadowBlur = nullptr;
};

} // namespace cc
