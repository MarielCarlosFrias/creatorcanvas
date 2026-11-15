#pragma once

#include "core/document/WebSlice.h"
#include "core/layers/Layer.h"

#include <QDialog>
#include <QString>

class QLabel;
class QListWidget;
class QPushButton;
class QSpinBox;
class QLineEdit;
class QComboBox;

namespace cc {

class SlicePreviewWidget;

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
                    I18nService* i18n = nullptr, QWidget* parent = nullptr,
                    bool advancedMode = false);
    ~WebSlicesDialog() override = default;

private:
    /// Desenha o documento em miniatura com os rectângulos das fatias por
    /// cima — é a prévia que torna o painel preciso sem adivinhar coordenadas.
    /// Inclui sempre o rectângulo PENDENTE (o que os campos descrevem
    /// agora), para se ver onde a fatia vai ficar antes de a adicionar.
    void updatePreview();

private slots:
    void refreshList();
    void addFromSelection();
    void addNumeric();
    void removeSelected();
    void exportAll();

public:
    /// Adiciona uma fatia por valores (sem depender dos spins — testável e
    /// reutilizável). Devolve o id criado, ou vazio.
    QString addSliceNumeric(int x, int y, int w, int h,
                            const QString& format, const QString& name);

    /// Adiciona uma fatia a partir da selecção de píxeis activa (como a
    /// caneta/pen: a bounding box é mapeada pela matriz da camada).
    QString addSliceFromSelection();

    /// Remove uma fatia pelo id. Devolve true se algo mudou.
    bool removeSliceById(const QString& id);

private:
    QString tr(const char* key, const QString& fallback) const;
    QString selectedSliceId() const;

    Document& m_doc;
    CommandStack* m_history = nullptr;
    LayerId m_selectedLayer;
    I18nService* m_i18n = nullptr;
    bool m_advancedMode = false;

    QListWidget* m_list = nullptr;
    SlicePreviewWidget* m_preview = nullptr;
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
