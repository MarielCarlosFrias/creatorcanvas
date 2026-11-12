#include "webslicesdialog.h"
#include "slicepreviewwidget.h"

#include "core/Document.h"
#include "core/commands/LayerCompCommands.h"
#include "core/history/CommandStack.h"
#include "core/layers/Layer.h"
#include "core/selection/SelectionEngine.h"
#include "rendering/CanvasRenderer.h"
#include "imageio/DocumentExporter.h"
#include "localization/i18nservice.h"

#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace cc {

WebSlicesDialog::WebSlicesDialog(Document& doc, CommandStack* history,
                                 const LayerId& selectedLayer,
                                 I18nService* i18n, QWidget* parent,
                                 bool advancedMode)
    : QDialog(parent)
    , m_doc(doc)
    , m_history(history)
    , m_selectedLayer(selectedLayer)
    , m_i18n(i18n)
    , m_advancedMode(advancedMode)
{
    setWindowTitle(tr("title", QStringLiteral("Fatias de Exportação Web - CreatorCanvas")));
    setMinimumSize(540, 430);

    auto* layout = new QVBoxLayout(this);

    m_preview = new SlicePreviewWidget(this);
    m_preview->setMinimumHeight(150);
    // Arrastar/redimensionar o pendente é ferramenta profissional.
    m_preview->setInteractive(m_advancedMode);
    layout->addWidget(m_preview, 1);

    m_list = new QListWidget(this);
    m_list->setMaximumHeight(110);
    layout->addWidget(m_list, 1);

    // --- Criação por valores ---
    // Rótulos + larguras mínimas: sem eles o QHBoxLayout espreme os spins
    // para ~4px e o utilizador vê "..." em vez de conseguir escrever.
    auto* formRow = new QHBoxLayout;
    formRow->setSpacing(4);
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("namePlaceholder", QStringLiteral("Nome da fatia")));
    m_nameEdit->setMinimumWidth(120);
    m_xSpin = new QSpinBox(this); m_ySpin = new QSpinBox(this);
    m_wSpin = new QSpinBox(this); m_hSpin = new QSpinBox(this);
    for (QSpinBox* s : {m_xSpin, m_ySpin, m_wSpin, m_hSpin}) {
        s->setRange(0, 100000);
        s->setSuffix(QStringLiteral(" px"));
        s->setMinimumWidth(72);
        s->setMaximumWidth(96);
    }
    m_wSpin->setValue(640);
    m_hSpin->setValue(360);
    m_formatCombo = new QComboBox(this);
    m_formatCombo->setMinimumWidth(78);
    m_formatCombo->addItem(QStringLiteral("PNG"), QStringLiteral("png"));
    m_formatCombo->addItem(QStringLiteral("JPEG"), QStringLiteral("jpeg"));
    m_formatCombo->addItem(QStringLiteral("WebP"), QStringLiteral("webp"));

    auto fieldLabel = [this](const QString& text) {
        auto* label = new QLabel(text, this);
        label->setStyleSheet(QStringLiteral("color: #9aa3b2;"));
        return label;
    };

    formRow->addWidget(m_nameEdit, 1);
    formRow->addWidget(m_formatCombo);
    layout->addLayout(formRow);

    // Segunda linha só com a geometria: numa linha única os campos e o
    // nome disputavam espaço e o texto dos botões ficava cortado.
    auto* geometryRow = new QHBoxLayout;
    geometryRow->setSpacing(4);
    geometryRow->addWidget(fieldLabel(QStringLiteral("X")));
    geometryRow->addWidget(m_xSpin);
    geometryRow->addWidget(fieldLabel(QStringLiteral("Y")));
    geometryRow->addWidget(m_ySpin);
    geometryRow->addWidget(fieldLabel(QStringLiteral("L")));
    geometryRow->addWidget(m_wSpin);
    geometryRow->addWidget(fieldLabel(QStringLiteral("A")));
    geometryRow->addWidget(m_hSpin);
    geometryRow->addStretch(1);
    layout->addLayout(geometryRow);

    auto* buttons = new QHBoxLayout;
    auto* fromSelectionBtn = new QPushButton(
        tr("fromSelection", QStringLiteral("Da selecção")), this);
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

    // Prévia em tempo real: escrever coordenadas desenha o pendente sem
    // precisar de adicionar a fatia primeiro.
    for (QSpinBox* s : {m_xSpin, m_ySpin, m_wSpin, m_hSpin}) {
        connect(s, &QSpinBox::valueChanged, this, [this]() { updatePreview(); });
    }
    connect(m_nameEdit, &QLineEdit::textChanged, this, [this]() { updatePreview(); });
    // Arrastar/redimensionar o pendente na prévia (modo avançado) escreve
    // as coordenadas de volta nos campos.
    connect(m_preview, &SlicePreviewWidget::pendingEdited, this,
            [this](const QRectF& docRect) {
                if (!m_xSpin || !m_ySpin || !m_wSpin || !m_hSpin)
                    return;
                const QSignalBlocker b1(m_xSpin), b2(m_ySpin), b3(m_wSpin), b4(m_hSpin);
                m_xSpin->setValue(qRound(docRect.x()));
                m_ySpin->setValue(qRound(docRect.y()));
                m_wSpin->setValue(qRound(docRect.width()));
                m_hSpin->setValue(qRound(docRect.height()));
            });
    connect(m_formatCombo, &QComboBox::currentIndexChanged, this, [this]() { updatePreview(); });

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
    updatePreview();
}

QString WebSlicesDialog::selectedSliceId() const
{
    QListWidgetItem* item = m_list->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

void WebSlicesDialog::updatePreview()
{
    if (!m_preview || m_doc.width() <= 0 || m_doc.height() <= 0)
        return;

    // Documento em miniatura: 320px do lado maior chega para julgar.
    const double scale = std::min(1.0, 320.0 / std::max(m_doc.width(), m_doc.height()));
    const QSize previewSize(std::max(1, qRound(m_doc.width() * scale)),
                            std::max(1, qRound(m_doc.height() * scale)));

    QImage img(previewSize, QImage::Format_ARGB32);
    img.fill(QColor(0x1a, 0x1c, 0x20));
    {
        QPainter painter(&img);
        QTransform toPreview;
        toPreview.scale(scale, scale);
        RenderOptions options;
        options.drawCheckerboard = false;
        options.canvasBorder = Qt::transparent;
        renderDocument(m_doc, &painter, toPreview, options);
    }

    // Pendente = o que os campos descrevem agora.
    QRectF pending;
    if (m_xSpin && m_ySpin && m_wSpin && m_hSpin) {
        pending = QRectF(m_xSpin->value(), m_ySpin->value(),
                         std::max(1, m_wSpin->value()), std::max(1, m_hSpin->value()));
    }

    m_preview->setDocumentImage(img);
    m_preview->setSlices(m_doc.webSlices());
    m_preview->setPending(pending);
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
