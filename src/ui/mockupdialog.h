#pragma once

#include <QDialog>
#include <QImage>
#include <QString>
#include "core/Document.h"

class QComboBox;
class QCheckBox;

namespace cc {

struct MockupDialogResult {
    bool accepted = false;
    QImage logo;
    bool freeShape = false;
};

class MockupDialog : public QDialog
{
    Q_OBJECT
public:
    explicit MockupDialog(QWidget* parent, Document* document, const LayerId& currentLayerId);

    MockupDialogResult execute();

    /// Processa o logo com remocao de fundo uniforme dos 4 cantos,
    /// suavizacao de halo anti-aliased e recorte ao bounding box.
    static QImage processLogo(const QImage& logo, bool removeBackground);

private:
    Document* m_document = nullptr;
    LayerId m_currentLayerId;
    QComboBox* m_sourceCombo = nullptr;
    QComboBox* m_modeCombo = nullptr;
    QCheckBox* m_removeBackground = nullptr;
};

} // namespace cc
