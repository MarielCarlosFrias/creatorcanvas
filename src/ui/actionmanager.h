#pragma once

#include <QAction>
#include <QActionGroup>
#include <QList>
#include <QObject>
#include <QString>
#include <QMap>
#include <QKeySequence>
#include <memory>

#include "core/layers/Layer.h"

namespace cc {

class I18nService;

struct NewShapeDef {
    ShapeKind kind;
    const char* key;
    const char* fallback;
};

extern const NewShapeDef kNewShapeDefs[8];
constexpr int kNewShapeCount = 8;

class ActionManager final : public QObject {
    Q_OBJECT

public:
    explicit ActionManager(QObject* parent = nullptr);
    ~ActionManager() override = default;

    void createActions(I18nService* i18n);
    void retranslate(I18nService* i18n);
    void selectToolByName(const QString& name);

    QAction* newAction = nullptr;
    QAction* openAction = nullptr;
    QAction* saveAction = nullptr;
    QAction* saveAsAction = nullptr;
    QAction* importAction = nullptr;
    QAction* exportAction = nullptr;
    QAction* batchAction = nullptr;
    QAction* workspaceLayoutAction = nullptr;
    QAction* quitAction = nullptr;

    QAction* undoAction = nullptr;
    QAction* redoAction = nullptr;

    QAction* zoomInAction = nullptr;
    QAction* zoomOutAction = nullptr;
    QAction* zoomFitAction = nullptr;

    QAction* addTextAction = nullptr;
    QAction* addFillLayerAction = nullptr;
    QAction* addAdjustmentLayerAction = nullptr;
    QAction* addRectAction = nullptr;
    QAction* addRoundedRectAction = nullptr;
    QAction* addEllipseAction = nullptr;
    QAction* addLineAction = nullptr;
    QAction* addArrowRightAction = nullptr;
    QAction* addArrowCurvedAction = nullptr;
    QAction* addStarAction = nullptr;
    QAction* addBadgeAction = nullptr;
    QAction* addSpeechBubbleAction = nullptr;
    QAction* addHeartAction = nullptr;

    QAction* alignLeftAction = nullptr;
    QAction* alignCenterXAction = nullptr;
    QAction* alignRightAction = nullptr;
    QAction* alignTopAction = nullptr;
    QAction* alignCenterYAction = nullptr;
    QAction* alignBottomAction = nullptr;
    QAction* alignCenterBothAction = nullptr;
    QAction* distributeHAction = nullptr;
    QAction* distributeVAction = nullptr;

    QAction* groupAction = nullptr;
    QAction* ungroupAction = nullptr;
    QAction* mergeSelectedAction = nullptr;
    QAction* mergeVisibleAction = nullptr;
    QAction* flattenAction = nullptr;

    QAction* textBehindSubjectAction = nullptr;
    QAction* flipHAction = nullptr;
    QAction* flipVAction = nullptr;
    QAction* rotateLeftAction = nullptr;
    QAction* rotateRightAction = nullptr;

    QAction* deleteAction = nullptr;
    QAction* lockLayerAction = nullptr;
    QAction* createClippingMaskAction = nullptr;
    QAction* releaseClippingMaskAction = nullptr;
    QAction* selectAllAction = nullptr;
    QAction* deselectAction = nullptr;
    QAction* invertSelectionAction = nullptr;
    QAction* selectionOpsAction = nullptr;
    QAction* colorRangeAction = nullptr;

    QAction* toolSelectAction = nullptr;
    QAction* toolCropAction = nullptr;
    QAction* toolScissorsAction = nullptr;
    QAction* toolWandAction = nullptr;
    QAction* toolCloneAction = nullptr;
    QAction* toolPaintAction = nullptr;
    QAction* toolFloodAction = nullptr;
    QAction* toolEyedropperAction = nullptr;
    QAction* toolRetouchAction = nullptr;
    QAction* toolHealingAction = nullptr;

    QAction* removeBgAiAction = nullptr;
    QAction* removeBgAiQuickAction = nullptr;
    QAction* mockupAction = nullptr;

    QList<QAction*> addNewShapeActions;
    QList<QAction*> modifyActions;
    QActionGroup* toolGroup = nullptr;
    QAction* toolMarqueeAction = nullptr;
    QAction* toolGradientAction = nullptr;

    QList<QAction*> filterActions;

    QAction* settingsAction = nullptr;
    QAction* startScreenAction = nullptr;
    QAction* aboutAction = nullptr;
    QAction* aboutQtAction = nullptr;

    QAction* toggleGridAction = nullptr;
    QAction* toggleSnapAction = nullptr;
    QAction* toggleHistoryAction = nullptr;
    QAction* toggleToolsBarAction = nullptr;
    QAction* toggleRightPanelAction = nullptr;
    QAction* toggleLayersDockAction = nullptr;
    QAction* toggleTextDockAction = nullptr;
    QAction* toggleShapeDockAction = nullptr;
    QAction* toggleImageDockAction = nullptr;

    QAction* safeNoneAction = nullptr;
    QAction* safeYouTubeAction = nullptr;
    QAction* safeInstagramAction = nullptr;
    QAction* safeTikTokAction = nullptr;

    QMap<QString, QKeySequence> getAllShortcuts() const;
    bool setShortcut(const QString& actionName, const QKeySequence& key);
    QKeySequence getShortcut(const QString& actionName) const;
    void resetAllShortcuts();
};

} // namespace cc
