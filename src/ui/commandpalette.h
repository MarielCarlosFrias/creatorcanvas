#pragma once

#include <QAction>
#include <QDialog>
#include <QVector>

class QEvent;
class QKeyEvent;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QShowEvent;

namespace cc {

class ActionManager;
class I18nService;

/// Paleta de comandos (Ctrl+K): pesquisa incremental sobre TODAS as acções
/// registadas no ActionManager -- menus, ferramentas, ajustes, formas e
/// filtros incluidos -- e executa a escolhida.
///
/// Vive como popup (_Qt::Popup_) por cima da janela principal: fecha com Esc
/// ou clique fora; Enter executa; setas e digitação navegam.
class CommandPalette final : public QDialog
{
    Q_OBJECT

public:
    CommandPalette(ActionManager* actions, I18nService* i18n,
                   QWidget* parent = nullptr);
    ~CommandPalette() override = default;

    /// Refaz a lista de entradas (usado quando o popup é reaberto, para
    /// reflectir o idioma e as acções disponíveis de momento).
    void refresh();

protected:
    void showEvent(QShowEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onQueryChanged(const QString& text);
    void onItemActivated(QListWidgetItem* item);

private:
    struct Entry {
        QAction* action = nullptr;
        QString label;     // texto de menu sem '&'
        QString haystack;  // minusculas: rotulo + tooltip + id
        int base = 0;      // ordem de criacao (estabilidade do resultado)
    };

    void buildEntries();
    void rebuildResults();
    void activate(QListWidgetItem* item);
    void moveSelection(int delta);
    void retranslateUi();
    static int matchScore(const QString& pattern, const QString& text);
    static QString cleanLabel(const QString& text);
    QString groupLabel(const QString& id) const;

    ActionManager* m_actions = nullptr;
    I18nService* m_i18n = nullptr;
    QVector<Entry> m_entries;
    QLineEdit* m_query = nullptr;
    QListWidget* m_results = nullptr;
    QLabel* m_hint = nullptr;
};

} // namespace cc
