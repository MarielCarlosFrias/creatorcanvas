#pragma once

#include <QDialog>
#include <QMap>
#include <QVariant>
#include <QComboBox>
#include <QLineEdit>

class QMainWindow;

namespace cc {

class I18nService;

class WorkspaceLayoutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WorkspaceLayoutDialog(I18nService* i18n = nullptr, QWidget* parent = nullptr);
    ~WorkspaceLayoutDialog() override = default;

private:
    void buildUi();
    void saveLayout();
    void loadLayout();
    void deleteLayout();
    void updateLayoutList();
    QMainWindow* mainWindow() const;
    bool restoreLayout(const QByteArray& geometry, const QByteArray& state);
    QString tr_(const QString& key, const QString& fallback) const;

    I18nService* m_i18n = nullptr;
    QComboBox* m_layoutCombo = nullptr;
    QLineEdit* m_layoutNameEdit = nullptr;
};

} // namespace cc
