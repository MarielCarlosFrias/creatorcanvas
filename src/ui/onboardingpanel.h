#pragma once

#include <QWidget>
#include <QVector>

class QLabel;
class QPushButton;

namespace cc {

class I18nService;

/// Painel "Primeiros passos": quatro objectivos que se marcam sozinhos à
/// medida que o utilizador trabalha (criar documento, adicionar camada,
/// seleccionar, exportar). Fecha-se quando tudo fica concluído.
class OnboardingPanel final : public QWidget
{
    Q_OBJECT

public:
    static constexpr int kStepCount = 4;


    explicit OnboardingPanel(I18nService* i18n, QWidget* parent = nullptr);
    ~OnboardingPanel() override = default;

    /// Estados dos passos, por ordem: documento, camada, selecção, exportação.
    void setSteps(const QVector<bool>& done);
    bool allDone() const { return m_allDone; }

    void retranslateUi();

signals:
    /// O utilizador fechou o painel (botão ou canto da dock).
    void dismissed();

private:
    I18nService* m_i18n = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_progressLabel = nullptr;
    QVector<QLabel*> m_stepLabels;
    QVector<bool> m_done{false, false, false, false};
    bool m_allDone = false;
    QPushButton* m_dismissButton = nullptr;
};

} // namespace cc
