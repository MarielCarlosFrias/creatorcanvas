#pragma once

#include <QObject>
#include <QVector>

class QWidget;
class QLabel;
class QPushButton;

namespace cc {

class I18nService;
class SettingsService;

/// Tutorial guiado de quatro passos: um popup ancorado a cada região da
/// interface (barra de ferramentas, opções, camadas, exportação). Corre uma
/// única vez por instalação (persistido em SettingsService) e pode ser
/// repetido a partir de Ajuda ▸ Tour rápido.
class OnboardingTour final : public QObject
{
    Q_OBJECT

public:
    OnboardingTour(I18nService* i18n, SettingsService* settings,
                   QWidget* host, QObject* parent = nullptr);
    ~OnboardingTour() override;

    /// Regiões realçadas, por ordem: ferramentas, opções, camadas, exportar.
    void setTargets(const QVector<QWidget*>& targets);

    /// Começa (ou recomeça) o tutorial a partir do primeiro passo.
    void start();

    /// Verdadeiro enquanto o popup está visível.
    bool isRunning() const { return m_running; }

private:
    void showStep(int index);
    void buildPopup();
    void positionPopup(QWidget* target);
    void finish(bool completed);

    I18nService* m_i18n = nullptr;
    SettingsService* m_settings = nullptr;
    QWidget* m_host = nullptr;
    QVector<QWidget*> m_targets;
    QWidget* m_popup = nullptr;
    QLabel* m_stepLabel = nullptr;
    QLabel* m_textLabel = nullptr;
    QPushButton* m_nextButton = nullptr;
    QPushButton* m_skipButton = nullptr;
    int m_index = 0;
    bool m_running = false;
};

} // namespace cc
