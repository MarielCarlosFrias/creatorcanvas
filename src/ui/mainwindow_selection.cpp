// Métodos de seleção e clipboard da MainWindow — extraídos de mainwindow.cpp
// para reduzir o ficheiro (mudança de sítio apenas; a lógica é idêntica).
#include "mainwindow.h"

#include "core/selection/SelectionEngine.h"
#include "core/selection/SelectionOperations.h"
#include "core/image/ImageProcessing.h"
#include "core/history/DocumentCommands.h"
#include "rendering/CanvasRenderer.h"
#include "ui/canvasview.h"
#include "ui/layerspanel.h"
#include "ui/tooloptionsbar.h"
#include "ui/asynctask.h"
#include "localization/i18nservice.h"

#include <QAction>
#include <QClipboard>
#include <QGuiApplication>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPointer>
#include <QStatusBar>

namespace cc {
namespace {

/// Texto traduzido com fallback para o catálogo atual (mantém mensagens de
/// erro fora do código quando existe catálogo carregado).
QString uiText(const I18nService* i18n, const char* key, const QString& fallback)
{
    return i18n ? i18n->t("editor", QString::fromLatin1(key)) : fallback;
}

} // namespace

void MainWindow::reselectSelection()
{
    if (!m_document)
        return;
    const QImage prev = m_document->previousPixelSelection();
    if (prev.isNull()) {
        statusBar()->showMessage(
            m_i18n ? m_i18n->t("editor", "selection.reselectNone")
                   : QStringLiteral("No previous selection to restore."), 3000);
        return;
    }
    const SelectionSpace prevSpace =
        m_document->selectionSnapshot().previousImageSpace();
    executeSetPixelSelection(*m_document, m_history.get(), prev,
                             QStringLiteral("selection.reselect"), prevSpace);
}

void MainWindow::saveCurrentSelection()
{
    if (!m_document || m_document->pixelSelection().isNull())
        return;
    bool ok = false;
    const QString name = QInputDialog::getText(
        this,
        m_i18n ? m_i18n->t("editor", "selection.save.title")
               : QStringLiteral("Save Selection"),
        m_i18n ? m_i18n->t("editor", "selection.save.name")
               : QStringLiteral("Selection name:"),
        QLineEdit::Normal,
        QStringLiteral("Selection %1").arg(m_document->savedSelections().size() + 1),
        &ok);
    if (!ok || name.trimmed().isEmpty())
        return;
    m_document->addSavedSelection(name.trimmed(), m_document->pixelSelection(),
                                  m_document->pixelSelectionSpace());
    statusBar()->showMessage(
        m_i18n ? m_i18n->t("editor", "selection.saved").arg(name.trimmed())
               : QStringLiteral("Saved selection \"%1\".").arg(name.trimmed()), 3000);
}

void MainWindow::rebuildLoadSelectionMenu()
{
    if (!m_loadSelectionMenu || !m_document)
        return;
    m_loadSelectionMenu->clear();
    const auto& saved = m_document->savedSelections();
    if (saved.empty()) {
        QAction* none = m_loadSelectionMenu->addAction(
            m_i18n ? m_i18n->t("editor", "selection.loadEmpty")
                   : QStringLiteral("(no saved selections)"));
        none->setEnabled(false);
        return;
    }
    for (const auto& s : saved) {
        QAction* act = m_loadSelectionMenu->addAction(s.name);
        const QImage mask = s.mask;
        const SelectionSpace space = s.space;
        connect(act, &QAction::triggered, this, [this, mask, space]() {
            if (m_document)
                executeSetPixelSelection(*m_document, m_history.get(), mask,
                                         QStringLiteral("selection.load"), space);
        });
    }
    m_loadSelectionMenu->addSeparator();
    QAction* delAll = m_loadSelectionMenu->addAction(
        m_i18n ? m_i18n->t("editor", "selection.clearSaved")
               : QStringLiteral("Delete All Saved Selections"));
    connect(delAll, &QAction::triggered, this, [this]() {
        if (m_document)
            m_document->clearSavedSelections();
    });
}

bool MainWindow::commitActiveLayerBitmap(const QImage& newImage, const QString& commandName,
                                         const LayerId& layerId)
{
    const LayerId targetId = layerId.isNull() ? m_selectedId : layerId;
    if (!m_document || targetId.isNull() || newImage.isNull())
        return false;
    Layer* layer = m_document->findLayer(targetId);
    if (!layer || layer->type() != LayerType::Image)
        return false;
    auto* img = static_cast<ImageLayer*>(layer);

    const LayerId newAssetId = m_document->assets().addImage(newImage);
    if (newAssetId.isNull())
        return false;

    if (m_history) {
        m_history->execute(std::make_unique<ModifyImageLayerCommand>(
            *m_document, targetId,
            img->assetId, img->naturalWidth, img->naturalHeight, img->transform,
            newAssetId, newImage.width(), newImage.height(), img->transform,
            commandName));
    } else {
        m_document->setImageLayerAsset(targetId, newAssetId,
                                       newImage.width(), newImage.height());
    }
    return true;
}

void MainWindow::applyImageFilterToActiveLayer(const std::function<QImage(const QImage&)>& fn,
                                               const QString& commandName)
{
    if (!m_document || m_selectedId.isNull() || !fn)
        return;
    if (m_filterInFlight) {
        statusBar()->showMessage(uiText(m_i18n, "filters.error.inFlight", tr("A filter is already running.")), 2500);
        return;
    }
    Layer* layer = m_document->findLayer(m_selectedId);
    if (!layer || layer->type() != LayerType::Image) {
        statusBar()->showMessage(uiText(m_i18n, "filters.error.imageLayer", tr("Filters require an image layer.")), 3500);
        return;
    }
    QString whyNot;
    if (!SelectionOperations::canEditPixels(*m_document, m_selectedId, &whyNot)) {
        statusBar()->showMessage(whyNot, 3500);
        return;
    }
    auto* img = static_cast<ImageLayer*>(layer);
    const LayerId layerId = m_selectedId;
    const LayerId originalAssetId = img->assetId;
    const QImage base = m_document->assets().decodedImage(originalAssetId);
    if (base.isNull())
        return;

    // Mapear a seleção ANTES do trabalho assíncrono; nunca deixar que uma
    // seleção de tamanho diferente transforme a operação numa edição global.
    QImage layerSelection;
    const QImage& sel = m_document->pixelSelection();
    if (!sel.isNull()) {
        layerSelection = SelectionOperations::selectionMaskInLayerSpace(
            *m_document, *img, sel, m_document->pixelSelectionSpace());
        if (layerSelection.isNull() || layerSelection.size() != base.size()) {
            statusBar()->showMessage(uiText(m_i18n, "selection.error.sizeMismatch", tr("Selection does not match the layer size.")), 3500);
            return;
        }
    }

    m_filterInFlight = true;
    const quint64 generation = ++m_filterGeneration;
    QPointer<Document> document(m_document);
    AsyncImageTask::start(
        [base, layerSelection, fn]() {
            QImage filtered = fn(base);
            if (!layerSelection.isNull() && !filtered.isNull()
                && filtered.size() == base.size()) {
                filtered = SelectionOperations::applySelectionMask(
                    base, filtered, layerSelection);
            }
            return filtered;
        },
        [self = QPointer<MainWindow>(this), document, layerId, originalAssetId,
         generation, commandName](QImage filtered) {
            if (!self)
                return;
            self->m_filterInFlight = false;
            if (generation != self->m_filterGeneration || !document
                || self->m_document != document)
                return;
            Layer* current = document->findLayer(layerId);
            if (!current || current->type() != LayerType::Image)
                return;
            const auto* image = static_cast<const ImageLayer*>(current);
            // Não sobrescrever uma edição feita enquanto o filtro calculava.
            if (image->assetId != originalAssetId)
                return;
            if (filtered.isNull()
                || filtered.size() != QSize(image->naturalWidth, image->naturalHeight)) {
                self->statusBar()->showMessage(
                    uiText(self->m_i18n, "filters.error.failed", self->tr("Filter failed.")), 3000);
                return;
            }
            if (!self->commitActiveLayerBitmap(filtered, commandName, layerId))
                self->statusBar()->showMessage(
                    uiText(self->m_i18n, "filters.error.failed", self->tr("Filter failed.")), 3000);
        });
}

void MainWindow::fillSelectionWithColor(const QColor& color)
{
    if (!m_document)
        return;
    Layer* layer = m_document->findLayer(m_selectedId);
    if (!layer || layer->type() != LayerType::Image) {
        statusBar()->showMessage(uiText(m_i18n, "edit.fill.imageLayer", tr("Fill requires an image layer.")), 3500);
        return;
    }
    QString whyNot;
    if (!SelectionOperations::canEditPixels(*m_document, m_selectedId, &whyNot)) {
        statusBar()->showMessage(whyNot, 3500);
        return;
    }
    auto* img = static_cast<ImageLayer*>(layer);
    const QImage original = m_document->assets().decodedImage(img->assetId);
    if (original.isNull())
        return;
    const QImage& sel = m_document->pixelSelection();
    if (!sel.isNull() && sel.size() != original.size()) {
        // Seleção noutro espaço (camada transformada): não adivinhar —
        // evita pintar a camada inteira por engano.
        statusBar()->showMessage(uiText(m_i18n, "selection.error.sizeMismatch", tr("Selection does not match the layer size.")), 3500);
        return;
    }
    QImage overlay(original.size(), QImage::Format_ARGB32_Premultiplied);
    overlay.fill(color);
    const QImage edited = sel.isNull()
        ? overlay
        : SelectionOperations::applySelectionMask(original, overlay, sel);
    if (!commitActiveLayerBitmap(edited, QStringLiteral("edit.fill")))
        statusBar()->showMessage(uiText(m_i18n, "edit.fill.failed", tr("Fill failed.")), 3000);
}

void MainWindow::strokeSelectionWithColor(const QColor& color, double widthPx)
{
    if (!m_document || m_document->pixelSelection().isNull())
        return;
    Layer* layer = m_document->findLayer(m_selectedId);
    if (!layer || layer->type() != LayerType::Image) {
        statusBar()->showMessage(uiText(m_i18n, "edit.stroke.imageLayer", tr("Stroke requires an image layer.")), 3500);
        return;
    }
    QString whyNot;
    if (!SelectionOperations::canEditPixels(*m_document, m_selectedId, &whyNot)) {
        statusBar()->showMessage(whyNot, 3500);
        return;
    }
    auto* img = static_cast<ImageLayer*>(layer);
    const QImage original = m_document->assets().decodedImage(img->assetId);
    if (original.isNull())
        return;
    const QImage& sel = m_document->pixelSelection();
    if (sel.size() != original.size()) {
        statusBar()->showMessage(uiText(m_i18n, "selection.error.sizeMismatch", tr("Selection does not match the layer size.")), 3500);
        return;
    }
    // Anel = dilatação menos erosão, já com anti-aliasing do border().
    const QImage ring = SelectionEngine::border(sel, float(widthPx));
    if (SelectionEngine::isEmpty(ring))
        return;
    QImage overlay(original.size(), QImage::Format_ARGB32_Premultiplied);
    overlay.fill(color);
    const QImage edited = SelectionOperations::applySelectionMask(original, overlay, ring);
    if (!commitActiveLayerBitmap(edited, QStringLiteral("edit.stroke")))
        statusBar()->showMessage(uiText(m_i18n, "edit.stroke.failed", tr("Stroke failed.")), 3000);
}

void MainWindow::clearSelectionPixels()
{
    if (!m_document)
        return;
    Layer* layer = m_document->findLayer(m_selectedId);
    if (!layer || layer->type() != LayerType::Image) {
        statusBar()->showMessage(uiText(m_i18n, "edit.clear.imageLayer", tr("Clear requires an image layer.")), 3500);
        return;
    }
    QString whyNot;
    if (!SelectionOperations::canEditPixels(*m_document, m_selectedId, &whyNot)) {
        statusBar()->showMessage(whyNot, 3500);
        return;
    }
    auto* img = static_cast<ImageLayer*>(layer);
    const QImage original = m_document->assets().decodedImage(img->assetId);
    if (original.isNull())
        return;
    const QImage& sel = m_document->pixelSelection();
    if (!sel.isNull() && sel.size() != original.size()) {
        statusBar()->showMessage(uiText(m_i18n, "selection.error.sizeMismatch", tr("Selection does not match the layer size.")), 3500);
        return;
    }
    QImage overlay(original.size(), QImage::Format_ARGB32_Premultiplied);
    overlay.fill(Qt::transparent);
    const QImage edited = sel.isNull()
        ? overlay
        : SelectionOperations::applySelectionMask(original, overlay, sel);
    if (!commitActiveLayerBitmap(edited, QStringLiteral("edit.clear")))
        statusBar()->showMessage(uiText(m_i18n, "edit.clear.failed", tr("Clear failed.")), 3000);
}

void MainWindow::copySelectedLayers()
{
    if (!m_document || m_selectedId.isNull())
        return;
    const QList<LayerId> ids = (m_canvas && !m_canvas->selectedLayers().isEmpty())
                                    ? m_canvas->selectedLayers()
                                    : QList<LayerId>{m_selectedId};
    if (!m_layerClipboard.capture(*m_document, ids))
        return;

    // Clipboard do sistema: bitmap renderizado das camadas copiadas. Guardamos
    // a cacheKey para detetar quando outra aplicação substitui o conteúdo.
    QImage img(m_document->width(), m_document->height(), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    {
        QPainter painter(&img);
        RenderOptions options;
        options.drawCheckerboard = false;
        options.canvasBorder = Qt::transparent;
        renderSiblingLayers(*m_document, ids, &painter, options);
    }
    QGuiApplication::clipboard()->setImage(img);
    m_systemClipboardImageKey = img.cacheKey();

    statusBar()->showMessage(uiText(m_i18n, "clipboard.copied", tr("Copied %1 layer(s).")).arg(ids.size()), 2500);
    updateSelectionMenuEnabled();
}

void MainWindow::cutSelectedLayer()
{
    if (!m_document || !m_history || m_selectedId.isNull())
        return;
    const QList<LayerId> ids = (m_canvas && !m_canvas->selectedLayers().isEmpty())
                                    ? m_canvas->selectedLayers()
                                    : QList<LayerId>{m_selectedId};
    for (const LayerId& id : ids) {
        if (!m_document->findLayer(id))
            return;
        if (m_document->effectiveLocks(id).all) {
            statusBar()->showMessage(uiText(m_i18n, "clipboard.locked", tr("The layer is locked.")), 3000);
            return;
        }
    }

    copySelectedLayers();
    if (m_layerClipboard.isEmpty())
        return;

    auto compound = std::make_unique<CompoundCommand>(QStringLiteral("clipboard.cut"));
    for (const LayerId& id : ids)
        compound->addCommand(std::make_unique<RemoveLayerCommand>(*m_document, id));
    m_history->execute(std::move(compound));
    statusBar()->showMessage(uiText(m_i18n, "clipboard.cut", tr("Cut %1 layer(s).")).arg(ids.size()), 2500);
}

bool MainWindow::pasteFromClipboard()
{
    if (!m_document)
        return false;

    QImage systemImage = QGuiApplication::clipboard()->image();
    // Se outra aplicação substituiu a imagem do sistema, o snapshot interno
    // antigo deixa de ter prioridade (antes colávamos conteúdo obsoleto).
    if (!m_layerClipboard.isEmpty() && !systemImage.isNull()
        && m_systemClipboardImageKey != 0
        && systemImage.cacheKey() != m_systemClipboardImageKey) {
        m_layerClipboard.clear();
        m_systemClipboardImageKey = 0;
    }

    // 1) Clipboard interno: preserva TODAS as camadas, tipos, estilos e assets.
    if (!m_layerClipboard.isEmpty()) {
        const QList<LayerId> ids = m_layerClipboard.paste(*m_document, m_history.get());
        if (ids.isEmpty()) {
            statusBar()->showMessage(uiText(m_i18n, "clipboard.pasteFailed", tr("Paste failed.")), 3000);
            return false;
        }
        if (m_canvas)
            m_canvas->setSelectedLayer(ids.last());
        if (m_layersPanel)
            m_layersPanel->refresh();
        return true;
    }

    // 2) Imagem do clipboard do sistema (ex.: captura de ecrã, outra app).
    if (systemImage.isNull()) {
        statusBar()->showMessage(uiText(m_i18n, "clipboard.empty", tr("Clipboard is empty.")), 2500);
        return false;
    }
    auto layer = std::make_unique<ImageLayer>();
    layer->name = uiText(m_i18n, "clipboard.pastedLayer", tr("Pasted layer"));
    layer->assetId = m_document->assets().addImage(systemImage);
    if (layer->assetId.isNull()) {
        statusBar()->showMessage(uiText(m_i18n, "clipboard.pasteFailed", tr("Paste failed.")), 3000);
        return false;
    }
    layer->naturalWidth = systemImage.width();
    layer->naturalHeight = systemImage.height();
    layer->transform.position = QPointF(m_document->width() / 2.0,
                                        m_document->height() / 2.0);
    const LayerId newId = layer->id();
    if (m_history)
        m_history->execute(std::make_unique<AddLayerCommand>(
            *m_document, std::move(layer), m_document->rootGroup(), -1));
    else
        m_document->addLayer(std::move(layer), m_document->rootGroup(), -1);
    if (m_canvas)
        m_canvas->setSelectedLayer(newId);
    if (m_layersPanel)
        m_layersPanel->refresh();
    return true;
}

void MainWindow::pasteIntoSelection()
{
    if (!m_document || m_document->pixelSelection().isNull()) {
        statusBar()->showMessage(uiText(m_i18n, "clipboard.selectionRequired", tr("Make a selection first.")), 2500);
        return;
    }
    // Se a colagem falhar ou o clipboard estiver vazio, NÃO tocar na camada
    // atualmente selecionada (era uma edição inesperada na versão anterior).
    if (!pasteFromClipboard() || m_selectedId.isNull())
        return;
    Layer* layer = m_document->findLayer(m_selectedId);
    if (!layer || layer->type() != LayerType::Image) {
        statusBar()->showMessage(uiText(m_i18n, "clipboard.maskImageOnly", tr("Pasted — masking is available for image layers.")), 3000);
        return;
    }

    auto* image = static_cast<ImageLayer*>(layer);
    QImage mask = SelectionOperations::selectionMaskInLayerSpace(
        *m_document, *image, m_document->pixelSelection(),
        m_document->pixelSelectionSpace());
    if (mask.isNull()) {
        statusBar()->showMessage(uiText(m_i18n, "clipboard.maskMapFailed", tr("Could not map the selection to the pasted layer.")), 3000);
        return;
    }
    // Combina com uma máscara já existente em vez de a destruir.
    if (!image->alphaMask.isNull() && image->alphaMask.size() == mask.size()) {
        const QImage oldAlpha = image->alphaMask.convertToFormat(QImage::Format_Alpha8);
        for (int y = 0; y < mask.height(); ++y) {
            uchar* dst = mask.scanLine(y);
            const uchar* src = oldAlpha.constScanLine(y);
            for (int x = 0; x < mask.width(); ++x)
                dst[x] = static_cast<uchar>((int(dst[x]) * int(src[x])) / 255);
        }
    }
    const QImage oldMask = image->alphaMask;
    if (m_history) {
        m_history->execute(std::make_unique<LayerPropertyCommand<QImage>>(
            *m_document, m_selectedId, QStringLiteral("layer.mask"),
            &Document::setLayerAlphaMask, oldMask, mask));
    } else {
        m_document->setLayerAlphaMask(m_selectedId, mask);
    }
    executeSetPixelSelection(*m_document, m_history.get(), QImage(),
                             QStringLiteral("selection.deselect"));
}

} // namespace cc
