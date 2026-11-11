#include "webslicesdialog.h"

#include "core/Document.h"
#include "core/commands/LayerCompCommands.h"
#include "core/history/CommandStack.h"
#include "core/layers/Layer.h"
#include "core/selection/SelectionEngine.h"
#include "imageio/DocumentExporter.h"
#include "localization/i18nservice.h"

#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace cc {

WebSlicesDialog::WebSlicesDialog(Document& doc, CommandStack* history,
                                 const LayerId& selectedLayer,
                                 I18nService* i18n, QWidget* parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_history(history)
    , m_selectedLayer(selectedLayer)
    , m_i18n(i18n)
{
    setWindowTitle(tr("title", QStringLiteral("Fatias de Exportação Web - CreatorCanvas")));
    setMinimumSize(520, 420);

    auto* layout = new QVBoxLayout(this);

    m_list = new QListWidget(this);
    layout->addWidget(m_list, 1);

    // --- Criação por valores ---
    auto* formRow = new QHBoxLayout;
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("namePlaceholder", QStringLiteral("Nome da fatia")));
    m_xSpin = new QSpinBox(this); m_ySpin = new QSpinBox(this);
    m_wSpin = new QSpinBox(this); m_hSpin = new QSpinBox(this);
    for (QSpinBox* s : {m_xSpin, m_ySpin, m_wSpin, m_hSpin}) {
        s->setRange(0, 100000);
        s->setSuffix(QStringLiteral(" px"));
    }
    m_wSpin->setValue(640);
    m_hSpin->setValue(360);
    m_formatCombo = new QComboBox(this);
    m_formatCombo->addItem(QStringLiteral("PNG"), QStringLiteral("png"));
    m_formatCombo->addItem(QStringLiteral("JPEG"), QStringLiteral("jpeg"));
    m_formatCombo->addItem(QStringLiteral("WebP"), QStringLiteral("webp"));
    formRow->addWidget(m_nameEdit, 1);
    formRow->addWidget(m_xSpin);
    formRow->addWidget(m_ySpin);
    formRow->addWidget(m_wSpin);
    formRow->addWidget(m_hSpin);
    formRow->addWidget(m_formatCombo);
    layout->addLayout(formRow);

    auto* buttons = new QHBoxLayout;
    auto* fromSelectionBtn = new QPushButton(
        tr("fromSelection", QStringLiteral("Nova a partir da selecção")), this);
    auto* addBtn = new QPushButton(tr("add", QStringLiteral("Adicionar fatia")), this);
    m_removeButton = new QPushButton(tr("remove", QStringLiteral("Remover")), this);
    m_exportButton = new QPushButton(tr("exportAll", QStringLiteral("Exportar todas…")), this);
    m_removeButton->setEnabled(false);
    m_exportButton->setEnabled(false);
    auto* closeBtn = new QPushButton(QStringLiteral("Fechar"), this);
    buttons->addWidget(fromSelectionBtn);
    buttons->addWidget(addBtn);
    buttons->addWidget(m_removeButton);
    buttons->addStretch(1);
    buttons->addWidget(m_exportButton);
    buttons->addWidget(closeBtn);
    layout->addLayout(buttons);

    connect(fromSelectionBtn, &QPushButton::clicked, this, &WebSlicesDialog::addFromSelection);
    connect(addBtn, &QPushButton::clicked, this, &WebSlicesDialog::addNumeric);
    connect(m_removeButton, &QPushButton::clicked, this, &WebSlicesDialog::removeSelected);
    connect(m_exportButton, &QPushButton::clicked, this, &WebSlicesDialog::exportAll);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_list, &QListWidget::itemSelectionChanged, this, [this]() {
        const bool has = !selectedSliceId().isEmpty();
        m_removeButton->setEnabled(has);
        m_exportButton->setEnabled(!m_doc.webSlices().empty());
    });

    refreshList();
}

QString WebSlicesDialog::tr(const char* key, const QString& fallback) const
{
    return m_i18n ? m_i18n->t("editor", QStringLiteral("webSlices.") + QLatin1String(key))
                  : fallback;
}

void WebSlicesDialog::refreshList()
{
    m_list->clear();
    for (const auto& slice : m_doc.webSlices()) {
        auto* item = new QListWidgetItem(QStringLiteral("%1 — %2×%3 em (%4,%5) [%6]")
                .arg(slice.name.isEmpty() ? slice.id : slice.name)
                .arg(slice.rect.width()).arg(slice.rect.height())
                .arg(slice.rect.x()).arg(slice.rect.y())
                .arg(slice.exportFormat.toUpper()));
        item->setData(Qt::UserRole, slice.id);
        m_list->addItem(item);
    }
    m_removeButton->setEnabled(false);
    m_exportButton->setEnabled(!m_doc.webSlices().empty());
}

QString WebSlicesDialog::selectedSliceId() const
{
    QListWidgetItem* item = m_list->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

void WebSlicesDialog::addFromSelection()
{
    if (m_selectedLayer.isNull())
        return;
    const Layer* layer = m_doc.findLayer(m_selectedLayer);
    if (!layer || m_doc.pixelSelection().isNull())
        return;

    // A selecção vive no espaço do bitmap da camada; a fatia é em espaço
    // de documento — atravessa a transformação da camada.
    const QImage selection = SelectionEngine::normalize(m_doc.pixelSelection());
    const QRect bbox = SelectionEngine::boundingBox(selection);
    if (bbox.isEmpty())
        return;
    const QRectF docRect = layer->transform.matrix(QRectF(bbox)).mapRect(QRectF(bbox));

    WebSlice slice;
    slice.name = m_nameEdit->text().trimmed().isEmpty()
        ? QStringLiteral("slice_%1").arg(m_doc.webSlices().size() + 1)
        : m_nameEdit->text().trimmed();
    slice.rect = docRect.normalized();
    slice.kind = SliceKind::Image;
    slice.exportFormat = m_formatCombo->currentData().toString();
    executeAddWebSlice(m_doc, m_history, std::move(slice));
    refreshList();
}

void WebSlicesDialog::addNumeric()
{
    WebSlice slice;
    slice.name = m_nameEdit->text().trimmed().isEmpty()
        ? QStringLiteral("slice_%1").arg(m_doc.webSlices().size() + 1)
        : m_nameEdit->text().trimmed();
    slice.rect = QRectF(m_xSpin->value(), m_ySpin->value(),
                        std::max(1, m_wSpin->value()), std::max(1, m_hSpin->value()));
    slice.kind = SliceKind::Image;
    slice.exportFormat = m_formatCombo->currentData().toString();
    executeAddWebSlice(m_doc, m_history, std::move(slice));
    refreshList();
}

void WebSlicesDialog::removeSelected()
{
    const QString id = selectedSliceId();
    if (id.isEmpty())
        return;
    executeRemoveWebSlice(m_doc, m_history, id);
    refreshList();
}

void WebSlicesDialog::exportAll()
{
    const auto slices = m_doc.webSlices();
    if (slices.empty())
        return;

    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("exportTitle", QStringLiteral("Exportar fatias para")));
    if (dir.isEmpty())
        return;

    int exported = 0;
    QStringList failures;
    for (const auto& slice : slices) {
        if (slice.kind != SliceKind::Image)
            continue;
        const QString fileName = QStringLiteral("%1.%2")
                .arg(slice.name.isEmpty() ? slice.id : slice.name, slice.exportFormat);
        const QString path = QDir(dir).filePath(fileName);
        QString error;
        if (cc::exportSliceToImage(m_doc, slice.rect, path, slice.exportFormat,
                                   slice.exportFormat == QLatin1String("jpeg") ? 92 : -1,
                                   1.0, &error)) {
            ++exported;
        } else {
            failures << fileName + (error.isEmpty() ? QString() : QStringLiteral(" (") + error + QStringLiteral(")"));
        }
    }

    QMessageBox box(this);
    box.setWindowTitle(tr("exportDone", QStringLiteral("Exportação de fatias")));
    box.setText(QStringLiteral("%1/%2 fatias exportadas para %3")
                    .arg(exported).arg(slices.size()).arg(dir));
    if (!failures.isEmpty())
        box.setInformativeText(failures.join(QLatin1Char('\n')));
    box.exec();
}

} // namespace cc
