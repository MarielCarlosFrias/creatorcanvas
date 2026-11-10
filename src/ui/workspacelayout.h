#pragma once

#include <QDialog>
#include <QMap>
#include <QVariant>
#include <QComboBox>
#include <QLineEdit>

namespace cc {

class WorkspaceLayoutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WorkspaceLayoutDialog(QWidget* parent = nullptr);
    ~WorkspaceLayoutDialog() override = default;

private:
    void buildUi();
    void saveLayout();
    void loadLayout();
    void deleteLayout();
    void updateLayoutList();

    QComboBox* m_layoutCombo = nullptr;
    QLineEdit* m_layoutNameEdit = nullptr;
};

} // namespace cc
