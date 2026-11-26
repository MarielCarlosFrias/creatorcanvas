#include "batchdialog.h"
#include "localization/i18nservice.h"

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
#include <QRegularExpression>
#include <QVBoxLayout>

namespace cc {

BatchDialog::BatchDialog(I18nService* i18n, QWidget* parent)
    : QDialog(parent)
    , m_i18n(i18n)
{
    setWindowTitle(tr_("batch.title", QStringLiteral("Batch Processing")));
    setMinimumSize(500, 400);
    buildUi();
}

QString BatchDialog::tr_(const QString& key, const QString& fallback) const
{
    return m_i18n ? m_i18n->t("editor", key) : fallback;
}

void BatchDialog::buildUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    // Seção de arquivos de entrada
    auto* inputGroup = new QGroupBox(tr_("batch.inputFiles", QStringLiteral("Input Files")));
    auto* inputLayout = new QVBoxLayout(inputGroup);

    m_inputList = new QListWidget();
    inputLayout->addWidget(m_inputList);

    auto* inputBtnLayout = new QHBoxLayout();
    auto* addBtn = new QPushButton(tr_("batch.addFiles", QStringLiteral("Add Files...")));
    auto* removeBtn = new QPushButton(tr_("batch.removeSelected", QStringLiteral("Remove Selected")));
    inputBtnLayout->addWidget(addBtn);
    inputBtnLayout->addWidget(removeBtn);
    inputBtnLayout->addStretch();
    inputLayout->addLayout(inputBtnLayout);

    connect(addBtn, &QPushButton::clicked, this, &BatchDialog::selectInputFiles);
    connect(removeBtn, &QPushButton::clicked, this, [this](){
        const auto selected = m_inputList->selectedItems();
        for (QListWidgetItem* item : selected) {
            m_inputFiles.removeAll(item->text());
            delete item;
        }
    });

    mainLayout->addWidget(inputGroup);

    // Seção de diretório de saída
    auto* outputGroup = new QGroupBox(tr_("batch.outputDirectory", QStringLiteral("Output Directory")));
    auto* outputLayout = new QHBoxLayout(outputGroup);

    m_outputEdit = new QLineEdit();
    m_outputEdit->setPlaceholderText(tr_("batch.selectOutputPlaceholder", QStringLiteral("Select output directory...")));
    auto* outputBtn = new QPushButton(tr_("batch.select", QStringLiteral("Select...")));
    outputLayout->addWidget(m_outputEdit);
    outputLayout->addWidget(outputBtn);

    connect(outputBtn, &QPushButton::clicked, this, &BatchDialog::selectOutputDirectory);

    mainLayout->addWidget(outputGroup);

    // Seção de operações
    auto* opsGroup = new QGroupBox(tr_("batch.operations", QStringLiteral("Operations")));
    auto* opsLayout = new QVBoxLayout(opsGroup);

    m_resizeCheck = new QCheckBox(tr_("batch.resize", QStringLiteral("Resize (width x height)")));
    m_resizeEdit = new QLineEdit("1920x1080");
    m_resizeEdit->setEnabled(false);
    connect(m_resizeCheck, &QCheckBox::toggled, m_resizeEdit, &QLineEdit::setEnabled);

    m_formatCheck = new QCheckBox(tr_("batch.convert", QStringLiteral("Convert Format")));
    m_formatCombo = new QComboBox();
    m_formatCombo->addItems({QStringLiteral("PNG"), QStringLiteral("JPEG"),
                             QStringLiteral("WebP"), QStringLiteral("PDF"),
                             QStringLiteral("SVG")});
    m_formatCombo->setEnabled(false);
    connect(m_formatCheck, &QCheckBox::toggled, m_formatCombo, &QComboBox::setEnabled);

    m_qualityCheck = new QCheckBox(tr_("batch.quality", QStringLiteral("JPEG/WebP Quality (0-100)")));
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
    m_overwriteCheck = new QCheckBox(tr_("batch.overwrite", QStringLiteral("Overwrite existing files")));
    mainLayout->addWidget(m_overwriteCheck);

    // Botões de ação
    auto* btnLayout = new QHBoxLayout();
    auto* cancelBtn = new QPushButton(tr_("batch.cancel", QStringLiteral("Cancel")));
    auto* startBtn = new QPushButton(tr_("batch.start", QStringLiteral("Start Processing")));
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
        tr_("batch.selectImages", QStringLiteral("Select Images")),
        QString(),
        tr_("batch.imagesFilter", QStringLiteral("Images (*.png *.jpg *.jpeg *.webp *.bmp *.tiff *.psd)"))
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
        tr_("batch.selectOutput", QStringLiteral("Select Output Directory")),
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
    
    // Validate and collect concrete parameters. The main window must never
    // silently replace the user's requested dimensions with a hard-coded one.
    m_operations.clear();
    m_resizeSize = {};
    m_outputFormat = "png";
    m_outputQuality = -1;
    if (m_resizeCheck->isChecked()) {
        const QRegularExpression re(QStringLiteral("^\\s*(\\d+)\\s*[xX×]\\s*(\\d+)\\s*$"));
        const auto match = re.match(m_resizeEdit->text());
        if (!match.hasMatch()) {
            m_resizeEdit->setFocus();
            return;
        }
        const QSize requested(match.captured(1).toInt(), match.captured(2).toInt());
        if (requested.width() < 1 || requested.height() < 1) {
            m_resizeEdit->setFocus();
            return;
        }
        m_resizeSize = requested;
        m_operations.append("resize");
    }
    if (m_formatCheck->isChecked()) {
        m_outputFormat = m_formatCombo->currentText().toLower().toUtf8();
        if (m_outputFormat == "jpeg")
            m_outputFormat = "jpg";
        m_operations.append("convert");
    }
    if (m_qualityCheck->isChecked()) {
        bool valid = false;
        const int quality = m_qualityEdit->text().toInt(&valid);
        if (!valid || quality < 0 || quality > 100) {
            m_qualityEdit->setFocus();
            return;
        }
        m_outputQuality = quality;
        m_operations.append("quality");
    }
    
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

QSize BatchDialog::resizeSize() const { return m_resizeSize; }
QByteArray BatchDialog::outputFormat() const { return m_outputFormat; }
int BatchDialog::outputQuality() const { return m_outputQuality; }

bool BatchDialog::overwriteExisting() const
{
    return m_overwriteExisting;
}

} // namespace cc
