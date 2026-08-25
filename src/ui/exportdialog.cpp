#include "exportdialog.h"

#include "localization/i18nservice.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

namespace cc {

ExportDialog::ExportDialog(I18nService* i18n, QWidget* parent)
    : QDialog(parent)
    , m_i18n(i18n)
{
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;

    m_format = new QComboBox(this);
    m_format->addItem(QStringLiteral("PNG"), QStringLiteral("png"));
    m_format->addItem(QStringLiteral("JPEG"), QStringLiteral("jpeg"));
    m_format->addItem(QStringLiteral("WebP"), QStringLiteral("webp"));

    m_quality = new QSpinBox(this);
    m_quality->setRange(1, 100);
    m_quality->setValue(95);

    m_scale = new QComboBox(this);
    m_scale->addItem(QStringLiteral("50%"), 0.5);
    m_scale->addItem(QStringLiteral("100%"), 1.0);
    m_scale->addItem(QStringLiteral("200%"), 2.0);
    m_scale->setCurrentIndex(1);

    m_formatLabel = new QLabel(this);
    m_qualityLabel = new QLabel(this);
    m_scaleLabel = new QLabel(this);
    form->addRow(m_formatLabel, m_format);
    form->addRow(m_qualityLabel, m_quality);
    form->addRow(m_scaleLabel, m_scale);
    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(m_format, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) { updateQualityEnabled(); });
    updateQualityEnabled();

    if (m_i18n)
        connect(m_i18n, &I18nService::languageChanged,
                this, &ExportDialog::retranslateUi);
    retranslateUi();
}

void ExportDialog::updateQualityEnabled()
{
    // PNG is lossless: quality does not apply.
    const QString fmt = m_format->currentData().toString();
    m_quality->setEnabled(fmt != QLatin1String("png"));
}

ExportSettings ExportDialog::settings() const
{
    ExportSettings s;
    s.format = m_format->currentData().toString();
    s.quality = m_quality->value();
    s.scale = m_scale->currentData().toDouble();
    return s;
}

void ExportDialog::retranslateUi()
{
    if (!m_i18n)
        return;
    setWindowTitle(m_i18n->t("editor", "export.dialog.title"));
    m_formatLabel->setText(m_i18n->t("editor", "export.format"));
    m_qualityLabel->setText(m_i18n->t("editor", "export.quality"));
    m_scaleLabel->setText(m_i18n->t("editor", "export.scale"));
}

} // namespace cc
