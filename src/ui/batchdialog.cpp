#include "batchdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace cc {

BatchDialog::BatchDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Processamento em Lote"));
    setMinimumSize(500, 400);
    buildUi();
}

void BatchDialog::buildUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    // Seção de arquivos de entrada
    auto* inputGroup = new QGroupBox(tr("Arquivos de Entrada"));
    auto* inputLayout = new QVBoxLayout(inputGroup);

    m_inputList = new QListWidget();
    inputLayout->addWidget(m_inputList);

    auto* inputBtnLayout = new QHBoxLayout();
    auto* addBtn = new QPushButton(tr("Adicionar Arquivos..."));
    auto* removeBtn = new QPushButton(tr("Remover Selecionado"));
    inputBtnLayout->addWidget(addBtn);
    inputBtnLayout->addWidget(removeBtn);
    inputBtnLayout->addStretch();
    inputLayout->addLayout(inputBtnLayout);

    connect(addBtn, &QPushButton::clicked, this, &BatchDialog::selectInputFiles);
    connect(removeBtn, &QPushButton::clicked, this, [](){
        // Remove selected items
    });

    mainLayout->addWidget(inputGroup);

    // Seção de diretório de saída
    auto* outputGroup = new QGroupBox(tr("Diretório de Saída"));
    auto* outputLayout = new QHBoxLayout(outputGroup);

    m_outputEdit = new QLineEdit();
    m_outputEdit->setPlaceholderText(tr("Selecionar diretório de saída..."));
    auto* outputBtn = new QPushButton(tr("Selecionar..."));
    outputLayout->addWidget(m_outputEdit);
    outputLayout->addWidget(outputBtn);

    connect(outputBtn, &QPushButton::clicked, this, &BatchDialog::selectOutputDirectory);

    mainLayout->addWidget(outputGroup);

    // Seção de operações
    auto* opsGroup = new QGroupBox(tr("Operações"));
    auto* opsLayout = new QVBoxLayout(opsGroup);

    m_resizeCheck = new QCheckBox(tr("Redimensionar (largura x altura)"));
    m_resizeEdit = new QLineEdit("1920x1080");
    m_resizeEdit->setEnabled(false);
    connect(m_resizeCheck, &QCheckBox::toggled, m_resizeEdit, &QLineEdit::setEnabled);

    m_formatCheck = new QCheckBox(tr("Converter Formato"));
    m_formatCombo = new QComboBox();
    m_formatCombo->addItems({QStringLiteral("PNG"), QStringLiteral("JPEG"), QStringLiteral("WebP")});
    m_formatCombo->setEnabled(false);
    connect(m_formatCheck, &QCheckBox::toggled, m_formatCombo, &QComboBox::setEnabled);

    m_qualityCheck = new QCheckBox(tr("Qualidade JPEG/WebP (0-100)"));
    m_qualityEdit = new QLineEdit("90");
    m_qualityEdit->setEnabled(false);
    connect(m_qualityCheck, &QCheckBox::toggled, m_qualityEdit, &QLineEdit::setEnabled);

    opsLayout->addWidget(m_resizeCheck);
    opsLayout->addWidget(m_resizeEdit);
    opsLayout->addWidget(m_formatCheck);
    opsLayout->addWidget(m_formatCombo);
    opsLayout->addWidget(m_qualityCheck);
    opsLayout->addWidget(m_qualityEdit);

    mainLayout->addWidget(opsGroup);

    // Opções adicionais
    m_overwriteCheck = new QCheckBox(tr("Sobrescrever arquivos existentes"));
    mainLayout->addWidget(m_overwriteCheck);

    // Botões de ação
    auto* btnLayout = new QHBoxLayout();
    auto* cancelBtn = new QPushButton(tr("Cancelar"));
    auto* startBtn = new QPushButton(tr("Iniciar Processamento"));
    startBtn->setDefault(true);
    btnLayout->addStretch();
    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(startBtn);

    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(startBtn, &QPushButton::clicked, this, &BatchDialog::startBatch);

    mainLayout->addLayout(btnLayout);
}

void BatchDialog::selectInputFiles()
{
    auto files = QFileDialog::getOpenFileNames(
        this,
        tr("Selecionar Imagens"),
        QString(),
        tr("Imagens (*.png *.jpg *.jpeg *.webp *.bmp *.tiff)")
    );
    
    for (const auto& f : files) {
        if (!m_inputFiles.contains(f)) {
            m_inputFiles.append(f);
            m_inputList->addItem(f);
        }
    }
}

void BatchDialog::selectOutputDirectory()
{
    auto dir = QFileDialog::getExistingDirectory(
        this,
        tr("Selecionar Diretório de Saída"),
        m_outputDirectory
    );
    
    if (!dir.isEmpty()) {
        m_outputDirectory = dir;
        m_outputEdit->setText(dir);
    }
}

void BatchDialog::startBatch()
{
    if (m_inputFiles.isEmpty()) {
        return;
    }
    
    if (m_outputDirectory.isEmpty()) {
        return;
    }
    
    // Collect selected operations
    m_operations.clear();
    if (m_resizeCheck->isChecked()) m_operations.append("resize");
    if (m_formatCheck->isChecked()) m_operations.append("convert");
    if (m_qualityCheck->isChecked()) m_operations.append("quality");
    
    m_overwriteExisting = m_overwriteCheck->isChecked();
    
    accept();
}

QStringList BatchDialog::inputFiles() const
{
    return m_inputFiles;
}

QString BatchDialog::outputDirectory() const
{
    return m_outputDirectory;
}

QStringList BatchDialog::operations() const
{
    return m_operations;
}

bool BatchDialog::overwriteExisting() const
{
    return m_overwriteExisting;
}

} // namespace cc
