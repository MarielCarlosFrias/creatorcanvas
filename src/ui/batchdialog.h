#pragma once

#include <QDialog>
#include <QStringList>
#include <QVector>

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLineEdit>
#include <QListWidget>

namespace cc {

class I18nService;

/// Diálogo para processamento em lote de múltiplos arquivos.
/// Permite aplicar operações em massa a uma seleção de arquivos.
class BatchDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BatchDialog(I18nService* i18n = nullptr, QWidget* parent = nullptr);
    ~BatchDialog() override = default;

    /// Retorna os arquivos selecionados para processamento.
    QStringList inputFiles() const;
    /// Retorna o diretório de saída selecionado.
    QString outputDirectory() const;
    /// Retorna as operações selecionadas.
    QStringList operations() const;
    QSize resizeSize() const;
    QByteArray outputFormat() const;
    int outputQuality() const;
    /// Retorna se deve sobrescrever arquivos existentes.
    bool overwriteExisting() const;

private:
    void buildUi();
    void selectInputFiles();
    void selectOutputDirectory();
    void startBatch();
    QString tr_(const QString& key, const QString& fallback) const;

    I18nService* m_i18n = nullptr;

    QStringList m_inputFiles;
    QString m_outputDirectory;
    QStringList m_operations;
    bool m_overwriteExisting = false;
    QSize m_resizeSize;
    QByteArray m_outputFormat = "png";
    int m_outputQuality = -1;

    // UI widgets
    QListWidget* m_inputList = nullptr;
    QLineEdit* m_outputEdit = nullptr;
    QCheckBox* m_resizeCheck = nullptr;
    QLineEdit* m_resizeEdit = nullptr;
    QCheckBox* m_formatCheck = nullptr;
    QComboBox* m_formatCombo = nullptr;
    QCheckBox* m_qualityCheck = nullptr;
    QLineEdit* m_qualityEdit = nullptr;
    QCheckBox* m_overwriteCheck = nullptr;
};

} // namespace cc
