#pragma once

#include <QDialog>
#include <QString>

class QLabel;
class QPushButton;

namespace cc {

class I18nService;

/// Diálogo moderno de recuperação de sessão após encerramento inesperado
/// Mostra a data, hora, caminho e tamanho do arquivo recuperado,
/// com opções: "Abrir recuperação", "Abrir como cópia" e "Descartar".
class RecoveryDialog final : public QDialog
{
    Q_OBJECT
public:
    enum class Choice {
        OpenRecovery,
        OpenAsCopy,
        Discard
    };

    explicit RecoveryDialog(I18nService* i18n, const QString& recoveryFilePath,
                            const QString& sourcePath = QString(),
                            QWidget* parent = nullptr);

    Choice userChoice() const { return m_choice; }

private:
    void buildUi(const QString& recoveryFilePath, const QString& sourcePath);

    I18nService* m_i18n = nullptr;
    Choice m_choice = Choice::Discard;
};

} // namespace cc
