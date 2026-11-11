#pragma once

#include "core/document/LayerComps.h"

#include <QDialog>
#include <QString>

class QListWidget;
class QCheckBox;
class QPushButton;
class QLineEdit;

namespace cc {

class Document;
class CommandStack;
class I18nService;

/// Painel de Composições de Camadas (estilo Photoshop): guarda o estado
/// actual como comp, aplica (um passo de undo) e remove. O painel mostra
/// que grupos de propriedades cada comp captura.
class LayerCompsDialog final : public QDialog
{
    Q_OBJECT

public:
    LayerCompsDialog(Document& doc, CommandStack* history,
                     I18nService* i18n = nullptr, QWidget* parent = nullptr);
    ~LayerCompsDialog() override = default;

private slots:
    void refreshList();
    void saveCurrentState();
    void applySelected();
    void removeSelected();

private:
    QString tr(const char* key, const QString& fallback) const;
    QString selectedCompId() const;

    Document& m_doc;
    CommandStack* m_history = nullptr;
    I18nService* m_i18n = nullptr;

    QListWidget* m_list = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QCheckBox* m_visibilityCheck = nullptr;
    QCheckBox* m_positionCheck = nullptr;
    QCheckBox* m_appearanceCheck = nullptr;
    QPushButton* m_applyButton = nullptr;
    QPushButton* m_removeButton = nullptr;
};

} // namespace cc
