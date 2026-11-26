#pragma once

#include <QDialog>

class QComboBox;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QLabel;
class QShowEvent;

namespace cc {

class I18nService;
struct PackItem;

/// Navegador de pacotes de recursos: grelha de miniaturas com filtro por
/// categoria. Ao escolher um item, o sinaliza para quem cria o documento
/// (ou insere a camada no documento actual).
class PackBrowser final : public QDialog
{
    Q_OBJECT

public:
    PackBrowser(I18nService* i18n, QWidget* parent = nullptr);
    ~PackBrowser() override = default;

protected:
    void showEvent(QShowEvent* event) override;

signals:
    /// Item escolhido: caminho do recurso e tamanho natural.
    void itemChosen(const QString& resource, const QSize& naturalSize);

private slots:
    void onCategoryChanged(int index);
    void onQueryChanged(const QString& text);
    void onItemActivated(QListWidgetItem* item);

private:
    void retranslateUi();
    void rebuild();
    QString trKeyPrefix() const;

    I18nService* m_i18n = nullptr;
    QComboBox* m_category = nullptr;
    QLineEdit* m_query = nullptr;
    QListWidget* m_grid = nullptr;
    QLabel* m_emptyLabel = nullptr;
    bool m_activated = false; // evita emissão dupla do itemChosen
};

} // namespace cc
