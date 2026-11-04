#include "ui/canvascontextmenu.h"
#include "ui/canvasview.h"

#include <QMenu>
#include <QAction>

#include "core/Document.h"
#include "core/history/CommandStack.h"
#include "core/layers/Layer.h"
#include "core/commands/LayerStructureCommands.h"
#include "core/commands/LayerPropertyCommands.h"
#include "core/selection/SelectionOperations.h"
#include "localization/i18nservice.h"

namespace cc {
namespace CanvasContextMenu {

struct MenuActions {
    QAction* duplicateAction = nullptr;
    QAction* deleteAction = nullptr;
    QAction* frontAction = nullptr;
    QAction* backAction = nullptr;
    QAction* flipHAction = nullptr;
    QAction* flipVAction = nullptr;
    QAction* centerAction = nullptr;
    QAction* lockAction = nullptr;
    QAction* removeBgAiDialogAction = nullptr;
    QAction* removeBgAiQuickAction = nullptr;
    QAction* maskInsideAction = nullptr;
    QAction* maskOutsideAction = nullptr;
    QAction* copySelectionAction = nullptr;
    QAction* cutSelectionAction = nullptr;
    QAction* invertSelectionAction = nullptr;
    QAction* clearSelectionAction = nullptr;
};

static MenuActions populateMenu(QMenu* menu,
                                const Document& document,
                                const Layer* layer,
                                I18nService* i18n)
{
    MenuActions a;
    if (!layer || !menu)
        return a;

    const QString duplicateText = i18n ? i18n->t("common", "panel.duplicate") : QStringLiteral("Duplicate");
    const QString deleteText = i18n ? i18n->t("common", "panel.delete") : QStringLiteral("Delete");
    const QString frontText = i18n ? i18n->t("common", "panel.front") : QStringLiteral("Bring to Front");
    const QString backText = i18n ? i18n->t("common", "panel.back") : QStringLiteral("Send to Back");
    const QString flipHText = i18n ? i18n->t("common", "menu.layer.flipH") : QStringLiteral("Flip Horizontal");
    const QString flipVText = i18n ? i18n->t("common", "menu.layer.flipV") : QStringLiteral("Flip Vertical");
    const QString centerText = i18n ? i18n->t("common", "menu.layer.align.centerBoth") : QStringLiteral("Center on Canvas");
    const bool isCurrentlyLocked = layer->locked || layer->locks.all;
    const QString lockText = isCurrentlyLocked
        ? (i18n ? i18n->t("common", "panel.unlock") : QStringLiteral("Unlock"))
        : (i18n ? i18n->t("common", "panel.lock") : QStringLiteral("Lock"));

    a.duplicateAction = menu->addAction(duplicateText);
    a.deleteAction = menu->addAction(deleteText);
    menu->addSeparator();
    a.frontAction = menu->addAction(frontText);
    a.backAction = menu->addAction(backText);
    menu->addSeparator();
    a.flipHAction = menu->addAction(flipHText);
    a.flipVAction = menu->addAction(flipVText);
    a.centerAction = menu->addAction(centerText);
    menu->addSeparator();
    a.lockAction = menu->addAction(lockText);

    if (layer->type() == LayerType::Image) {
        if (!document.pixelSelection().isNull()) {
            menu->addSeparator();
            a.maskInsideAction = menu->addAction(QStringLiteral("Mask: keep inside selection"));
            a.maskOutsideAction = menu->addAction(QStringLiteral("Mask: keep outside selection"));
            a.copySelectionAction = menu->addAction(QStringLiteral("Copy selection to new layer"));
            a.cutSelectionAction = menu->addAction(QStringLiteral("Cut selection to new layer"));
            menu->addSeparator();
            a.invertSelectionAction = menu->addAction(QStringLiteral("Invert selection"));
            a.clearSelectionAction = menu->addAction(QStringLiteral("Cancel selection"));
        }
        menu->addSeparator();
        const QString aiDialogText = i18n ? i18n->t("editor", "canvas.context.aiDialog")
                                          : QStringLiteral("✨ Remove Background with AI...");
        const QString aiQuickText = i18n ? i18n->t("editor", "canvas.context.aiQuick")
                                         : QStringLiteral("⚡ Quick Background Removal (1-Click)");
        a.removeBgAiDialogAction = menu->addAction(aiDialogText);
        a.removeBgAiQuickAction = menu->addAction(aiQuickText);
    }

    return a;
}

std::unique_ptr<QMenu> buildMenu(QWidget* parent,
                                 Document& document,
                                 const LayerId& layerId,
                                 I18nService* i18n)
{
    Layer* layer = document.findLayer(layerId);
    if (!layer || layer->type() == LayerType::Background)
        return nullptr;

    auto menu = std::make_unique<QMenu>(parent);
    populateMenu(menu.get(), document, layer, i18n);
    return menu;
}

bool show(QWidget* parent,
          Document& document,
          CommandStack* history,
          I18nService* i18n,
          const LayerId& layerId,
          const QPoint& globalPos,
          CanvasView* canvas)
{
    Layer* layer = document.findLayer(layerId);
    if (!layer || layer->type() == LayerType::Background)
        return false;

    QMenu menu(parent);
    MenuActions a = populateMenu(&menu, document, layer, i18n);

    QAction* chosen = menu.exec(globalPos);
    if (!chosen)
        return false;

    const LayerId id = layer->id();

    if (chosen == a.invertSelectionAction) {
        document.setPixelSelection(
            SelectionOperations::inverted(document.pixelSelection()));
        if (canvas) canvas->update();
    } else if (chosen == a.clearSelectionAction) {
        document.clearPixelSelection();
        if (canvas) canvas->update();
    } else if (chosen == a.copySelectionAction || chosen == a.cutSelectionAction) {
        LayerId createdId;
        if (SelectionOperations::extractToLayer(
                document, history, id, chosen == a.cutSelectionAction, &createdId)) {
            if (chosen == a.cutSelectionAction && canvas) {
                canvas->setSelectedLayer(createdId);
            }
            if (canvas) canvas->update();
        }
    } else if (chosen == a.maskInsideAction || chosen == a.maskOutsideAction) {
        if (SelectionOperations::applyMask(
                document, history, id, chosen == a.maskInsideAction)) {
            if (canvas) canvas->update();
        }
    } else if (chosen == a.removeBgAiDialogAction) {
        if (canvas) canvas->openAiBackgroundRemoval(id);
    } else if (chosen == a.removeBgAiQuickAction) {
        if (canvas) canvas->removeBackgroundAiQuick(id);
    } else if (chosen == a.duplicateAction) {
        if (canvas) emit canvas->duplicateRequested(id);
    } else if (chosen == a.deleteAction) {
        if (canvas) emit canvas->deleteRequested(id);
    } else if (chosen == a.frontAction) {
        if (history) {
            history->execute(std::make_unique<ReorderLayerCommand>(
                document, id, document.parentOf(id), -1));
        } else {
            document.reorderLayer(id, document.parentOf(id), -1);
        }
    } else if (chosen == a.backAction) {
        if (history) {
            history->execute(std::make_unique<ReorderLayerCommand>(
                document, id, document.parentOf(id), 0));
        } else {
            document.reorderLayer(id, document.parentOf(id), 0);
        }
    } else if (chosen == a.flipHAction) {
        const AffineTransform oldT = layer->transform;
        AffineTransform newT = oldT;
        newT.scaleX = -newT.scaleX;
        document.setLayerTransform(id, newT);
        if (canvas) emit canvas->transformCommitted(id, oldT, newT);
    } else if (chosen == a.flipVAction) {
        const AffineTransform oldT = layer->transform;
        AffineTransform newT = oldT;
        newT.scaleY = -newT.scaleY;
        document.setLayerTransform(id, newT);
        if (canvas) emit canvas->transformCommitted(id, oldT, newT);
    } else if (chosen == a.centerAction) {
        const AffineTransform oldT = layer->transform;
        AffineTransform newT = oldT;
        newT.position = QPointF(document.width() / 2.0, document.height() / 2.0);
        document.setLayerTransform(id, newT);
        if (canvas) emit canvas->transformCommitted(id, oldT, newT);
    } else if (chosen == a.lockAction) {
        const bool current = layer->locked || layer->locks.all;
        const bool newLock = !current;
        if (history) {
            history->execute(std::make_unique<LayerLockCommand>(
                document, id, QStringLiteral("layer.lock"),
                &Document::setLayerLocked, current, newLock));
        } else {
            document.setLayerLocked(id, newLock);
        }
    }

    return true;
}

} // namespace CanvasContextMenu
} // namespace cc
