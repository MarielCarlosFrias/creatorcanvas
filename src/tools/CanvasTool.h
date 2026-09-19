#pragma once

#include <QPointF>
#include <QList>
#include <functional>
#include "core/Document.h"

class QMouseEvent;
class QKeyEvent;
class QPainter;
class QTransform;

namespace cc {

class CanvasView;
class SnapEngine;
class CommandStack;
class I18nService;

enum class CanvasTool {
    Select,
    Crop,
    Scissors,
    MagicWand,
    CloneStamp,
    Paint,
    FloodFill
};

using CanvasToolType = CanvasTool;

/// Context provided to tools on each event or draw cycle
struct ToolContext {
    Document* document = nullptr;
    CommandStack* history = nullptr;
    SnapEngine* snapEngine = nullptr;
    I18nService* i18n = nullptr;
    CanvasView* view = nullptr;

    double zoom = 1.0;
    QPointF panOffset;
    QTransform docToDevice;
    QTransform deviceToDoc;

    bool showGrid = false;
    bool snapToGrid = false;
    int gridSpacing = 20;
    int safeZoneMode = 0;

    LayerId selectedLayerId;
    QList<LayerId> selectedLayerIds;

    std::function<void()> requestUpdate;
    std::function<void(const QString&)> requestStatusMessage;
    std::function<void(const LayerId&)> selectLayer;
    std::function<void(CanvasTool)> switchTool;
};

/// Abstract base class for all canvas interaction tools
class ICanvasTool
{
public:
    virtual ~ICanvasTool() = default;

    virtual CanvasTool type() const = 0;

    virtual void activate(const ToolContext& ctx) { Q_UNUSED(ctx); }
    virtual void deactivate(const ToolContext& ctx) { Q_UNUSED(ctx); }

    virtual void mousePress(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) {
        Q_UNUSED(event); Q_UNUSED(docPos); Q_UNUSED(ctx);
    }
    virtual void mouseMove(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) {
        Q_UNUSED(event); Q_UNUSED(docPos); Q_UNUSED(ctx);
    }
    virtual void mouseRelease(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) {
        Q_UNUSED(event); Q_UNUSED(docPos); Q_UNUSED(ctx);
    }
    virtual void mouseDoubleClick(QMouseEvent* event, const QPointF& docPos, const ToolContext& ctx) {
        Q_UNUSED(event); Q_UNUSED(docPos); Q_UNUSED(ctx);
    }

    virtual void keyPress(QKeyEvent* event, const ToolContext& ctx) {
        Q_UNUSED(event); Q_UNUSED(ctx);
    }
    virtual void keyRelease(QKeyEvent* event, const ToolContext& ctx) {
        Q_UNUSED(event); Q_UNUSED(ctx);
    }

    virtual void drawOverlay(QPainter* painter, const ToolContext& ctx) {
        Q_UNUSED(painter); Q_UNUSED(ctx);
    }

    virtual void cancel(const ToolContext& ctx) { Q_UNUSED(ctx); }
};

} // namespace cc
