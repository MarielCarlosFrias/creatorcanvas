#pragma once

#include <QDialog>
#include <QString>

class QComboBox;
class QLabel;

namespace cc {

class I18nService;
class SettingsService;

/// Diálogo de primeira utilização: escolha de idioma + three linhas sobre o
/// que o programa faz. Aparece uma única vez (a confirmação fica gravada em
/// SettingsService) e só no arranque normal -- os modos automatizados
/// (--smoke-test, --ui-snapshot) passam sem ele.
class WelcomeDialog final : public QDialog
{
    Q_OBJECT

public:
    WelcomeDialog(I18nService* i18n, SettingsService* settings,
                  QWidget* parent = nullptr);
    ~WelcomeDialog() override = default;

private:
    void accept() override;
    void retranslateUi();

    I18nService* m_i18n = nullptr;
    SettingsService* m_settings = nullptr;
    QComboBox* m_language = nullptr;
    QLabel* m_introLabel = nullptr;
    QLabel* m_bulletsLabel = nullptr;
    QLabel* m_titleLabel = nullptr;
};

} // namespace cc
