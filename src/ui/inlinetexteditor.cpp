#include "ui/inlinetexteditor.h"

#include <QLineEdit>
#include <QKeyEvent>
#include <QWidget>
#include <algorithm>

namespace cc {

InlineTextEditor::InlineTextEditor(QWidget* parentCanvas)
    : QObject(parentCanvas)
    , m_parentCanvas(parentCanvas)
{
}

InlineTextEditor::~InlineTextEditor() = default;

void InlineTextEditor::ensureEditor()
{
    if (!m_textEditor && m_parentCanvas) {
        m_textEditor = new QLineEdit(m_parentCanvas);
        m_textEditor->installEventFilter(this);
        connect(m_textEditor, &QLineEdit::returnPressed, this, &InlineTextEditor::commit);
        connect(m_textEditor, &QLineEdit::editingFinished, this, &InlineTextEditor::commit);
    }
}

void InlineTextEditor::startEdit(const LayerId& id, const TextLayer& layer, const QRectF& screenRect, double zoom)
{
    ensureEditor();
    if (!m_textEditor)
        return;

    m_editingTextId = id;
    m_originalContent = layer.content;

    // WYSIWYG: estiliza o editor com os próprios atributos do texto
    const QString hex = layer.color.name(QColor::HexRgb);
    const double px = std::max(6.0, layer.sizePt * (96.0 / 72.0) * zoom);

    // Texto claro sobre fundo claro fica ilegível: escurece o editor quando a cor do texto for clara
    const QColor textColor = layer.color;
    const double luminance = 0.299 * textColor.red()
                           + 0.587 * textColor.green()
                           + 0.114 * textColor.blue();
    const QString editorBg = luminance > 150 ? QStringLiteral("#26272b")
                                             : QStringLiteral("white");

    m_textEditor->setStyleSheet(QStringLiteral(
        "QLineEdit { background: %7; color: %1;"
        " border: 1px solid #2f6fed; padding: 0px;"
        " font-family: \"%2\"; font-size: %3px;"
        " font-weight: %4; font-style: %5; text-decoration: %6; }"
        "QLineEdit { selection-background-color: #2f6fed;"
        " selection-color: %1; }")
        .arg(hex)
        .arg(layer.fontFamily)
        .arg(QString::number(px, 'f', 0))
        .arg(layer.bold ? QStringLiteral("bold") : QStringLiteral("normal"))
        .arg(layer.italic ? QStringLiteral("italic") : QStringLiteral("normal"))
        .arg(layer.underline ? QStringLiteral("underline") : QStringLiteral("none"))
        .arg(editorBg));

    m_textEditor->setAlignment(
        layer.align == TextAlignment::Center
            ? Qt::AlignCenter
            : (layer.align == TextAlignment::Right ? Qt::AlignRight : Qt::AlignLeft));

    m_textEditor->setGeometry(screenRect.toRect().adjusted(-2, -2, 2, 2));
    m_textEditor->setText(layer.content);
    m_textEditor->show();
    m_textEditor->raise();
    m_textEditor->setFocus();
    m_textEditor->selectAll();
}

void InlineTextEditor::commit()
{
    if (!m_textEditor || m_editingTextId.isNull())
        return;

    const QString newContent = m_textEditor->text();
    const QString oldContent = m_originalContent;
    const LayerId id = m_editingTextId;

    m_textEditor->hide();
    m_editingTextId = LayerId();
    m_originalContent.clear();

    if (newContent != oldContent) {
        emit textCommitted(id, oldContent, newContent);
    }
}

void InlineTextEditor::cancel()
{
    if (m_textEditor) {
        m_textEditor->hide();
    }
    m_editingTextId = LayerId();
    m_originalContent.clear();
    emit editCancelled();
}

bool InlineTextEditor::isEditing() const
{
    return !m_editingTextId.isNull() && m_textEditor != nullptr && !m_textEditor->isHidden();
}

bool InlineTextEditor::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_textEditor && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            cancel();
            return true;
        }
    }
    return QObject::eventFilter(watched, event);
}

} // namespace cc
