#pragma once

#include <QIcon>
#include <QPixmap>
#include <QColor>
#include <QSize>

namespace cc {

/// Produces clean, crisp vector-drawn icons for CreatorCanvas UI:
/// - Main toolbar actions
/// - Layers panel (visibility, lock, type badges, actions)
/// - Start screen brand badges and action buttons
class ThemeIcons
{
public:
    // --- Tools ---
    static QIcon toolSelect(const QColor& color = QColor(220, 220, 220));
    static QIcon toolCrop(const QColor& color = QColor(220, 220, 220));
    static QIcon toolScissors(const QColor& color = QColor(220, 220, 220));
    static QIcon toolWand(const QColor& color = QColor(220, 220, 220));
    static QIcon toolClone(const QColor& color = QColor(220, 220, 220));
    static QIcon toolPaint(const QColor& color = QColor(220, 220, 220));
    static QIcon toolFlood(const QColor& color = QColor(220, 220, 220));
    static QIcon toolText(const QColor& color = QColor(220, 220, 220));
    static QIcon toolShape(const QColor& color = QColor(220, 220, 220));

    // --- Layer Controls ---
    static QIcon layerVisibility(bool visible);
    static QIcon layerLock(bool locked);
    static QIcon layerTypeText();
    static QIcon layerTypeImage();
    static QIcon layerTypeShape();
    static QIcon layerTypePaint();
    static QIcon layerTypeGroup();

    // --- Actions ---
    static QIcon actionAdd();
    static QIcon actionDuplicate();
    static QIcon actionDelete();
    static QIcon actionMoveUp();
    static QIcon actionMoveDown();

    // --- Start Screen & Brands ---
    static QIcon actionNewDocument();
    static QIcon actionOpenFolder();
    static QPixmap brandYoutube(int size = 28);
    static QPixmap brandInstagram(int size = 28);
    static QPixmap brandTiktok(int size = 28);
    static QPixmap brandBanner(int size = 28);
    static QPixmap brandCustom(int size = 28);

    // --- App Icon ---
    static QIcon appIcon();

    // --- File Actions ---
    static QIcon actionSave(const QColor& color = QColor(220, 220, 220));
    static QIcon actionSaveAs(const QColor& color = QColor(220, 220, 220));
    static QIcon actionImport(const QColor& color = QColor(220, 220, 220));
    static QIcon actionExport(const QColor& color = QColor(220, 220, 220));
    static QIcon actionQuit(const QColor& color = QColor(220, 220, 220));

    // --- Edit & History Actions ---
    static QIcon actionUndo(const QColor& color = QColor(220, 220, 220));
    static QIcon actionRedo(const QColor& color = QColor(220, 220, 220));
    static QIcon actionHistory(const QColor& color = QColor(220, 220, 220));

    // --- View & Zoom Actions ---
    static QIcon actionZoomIn(const QColor& color = QColor(220, 220, 220));
    static QIcon actionZoomOut(const QColor& color = QColor(220, 220, 220));
    static QIcon actionZoomFit(const QColor& color = QColor(220, 220, 220));
    static QIcon actionGrid(const QColor& color = QColor(220, 220, 220));
    static QIcon actionSnap(const QColor& color = QColor(220, 220, 220));

    // --- Transform & Alignment Actions ---
    static QIcon actionFlipH(const QColor& color = QColor(220, 220, 220));
    static QIcon actionFlipV(const QColor& color = QColor(220, 220, 220));
    static QIcon actionAlignLeft(const QColor& color = QColor(220, 220, 220));
    static QIcon actionAlignCenter(const QColor& color = QColor(220, 220, 220));
    static QIcon actionAlignRight(const QColor& color = QColor(220, 220, 220));
    static QIcon actionAlignTop(const QColor& color = QColor(220, 220, 220));
    static QIcon actionAlignMiddle(const QColor& color = QColor(220, 220, 220));
    static QIcon actionAlignBottom(const QColor& color = QColor(220, 220, 220));
    static QIcon actionDistributeH(const QColor& color = QColor(220, 220, 220));
    static QIcon actionDistributeV(const QColor& color = QColor(220, 220, 220));

    // --- Dialog & UI Controls ---
    static QIcon actionSettings(const QColor& color = QColor(220, 220, 220));
    static QIcon actionAbout(const QColor& color = QColor(220, 220, 220));
    static QIcon actionCheck(const QColor& color = QColor(255, 255, 255));
    static QIcon actionCancel(const QColor& color = QColor(220, 220, 220));

    // --- Empty state illustration ---
    static QPixmap emptyProjectsPlaceholder(int width = 72, int height = 72);
};

} // namespace cc
