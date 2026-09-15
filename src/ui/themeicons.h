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

    // --- Empty state illustration ---
    static QPixmap emptyProjectsPlaceholder(int width = 72, int height = 72);
};

} // namespace cc
