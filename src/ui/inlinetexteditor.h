#pragma once

#include <QObject>
#include <QRectF>
#include "core/layers/Layer.h"

class QWidget;
class QLineEdit;

namespace cc {

/// Gerencia o editor de texto inline sobreposto diretamente no Canvas.
/// Fornece visualização WYSIWYG, tratamento de foco, cancelamento por Esc e confirmação por Enter.
class InlineTextEditor final : public QObject
{
    Q_OBJECT
public:
    explicit InlineTextEditor(QWidget* parentCanvas);
    ~InlineTextEditor() override;

    void startEdit(const LayerId& id, const TextLayer& layer, const QRectF& screenRect, double zoom);
    void commit();
    void cancel();

    bool isEditing() const;
    LayerId editingLayerId() const { return m_editingTextId; }

signals:
    void textCommitted(const cc::LayerId& id, const QString& oldContent, const QString& newContent);
    void editCancelled();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void ensureEditor();

    QWidget* m_parentCanvas = nullptr;
    QLineEdit* m_textEditor = nullptr;
    LayerId m_editingTextId;
    QString m_originalContent;
};

} // namespace cc
