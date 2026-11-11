#pragma once

#include "core/document/WebSlice.h"
#include "core/layers/Layer.h"

#include <QDialog>
#include <QString>

class QListWidget;
class QPushButton;
class QSpinBox;
class QLineEdit;
class QComboBox;

namespace cc {

class Document;
class CommandStack;
class I18nService;

/// Painel de fatias de exportação web: cria rects nomeados no espaço do
/// documento (a partir da selecção activa ou por valores) e exporta cada
/// um para o formato escolhido num directório. O modelo e o exporter já
/// existiam — aqui fica a superfície de utilizador.
class WebSlicesDialog final : public QDialog
{
    Q_OBJECT

public:
    WebSlicesDialog(Document& doc, CommandStack* history,
                    const LayerId& selectedLayer,
                    I18nService* i18n = nullptr, QWidget* parent = nullptr);
    ~WebSlicesDialog() override = default;

private slots:
    void refreshList();
    void addFromSelection();
    void addNumeric();
    void removeSelected();
    void exportAll();

private:
    QString tr(const char* key, const QString& fallback) const;
    QString selectedSliceId() const;

    Document& m_doc;
    CommandStack* m_history = nullptr;
    LayerId m_selectedLayer;
    I18nService* m_i18n = nullptr;

    QListWidget* m_list = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QSpinBox* m_xSpin = nullptr;
    QSpinBox* m_ySpin = nullptr;
    QSpinBox* m_wSpin = nullptr;
    QSpinBox* m_hSpin = nullptr;
    QComboBox* m_formatCombo = nullptr;
    QPushButton* m_removeButton = nullptr;
    QPushButton* m_exportButton = nullptr;
};

} // namespace cc
