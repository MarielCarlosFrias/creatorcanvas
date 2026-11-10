#pragma once

#include <QDialog>
#include <QKeySequence>
#include <QMap>
#include <QPair>
#include <QTableWidget>

namespace cc {

class ActionManager;

/// Diálogo para personalização de atalhos de teclado.
/// Permite ao usuário visualizar e alterar os atalhos de teclado
/// associados a cada ação do aplicativo.
class ShortcutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ShortcutDialog(ActionManager* actionManager, QWidget* parent = nullptr);
    ~ShortcutDialog() override = default;

    /// Retorna o mapa de atalhos alterados (ação -> nova sequência de teclas).
    QMap<QString, QKeySequence> changedShortcuts() const;

private:
    void buildUi();
    void populateActions();
    void editShortcut(int row);
    void resetShortcut(int row);
    void resetAllShortcuts();
    void applyChanges();

    ActionManager* m_actionManager;
    QMap<QString, QKeySequence> m_originalShortcuts;
    QMap<QString, QKeySequence> m_changedShortcuts;
    QTableWidget* m_table = nullptr;
};

} // namespace cc
