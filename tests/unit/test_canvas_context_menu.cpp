#include <QtTest>
#include <QMenu>
#include <QAction>

#include "core/Document.h"
#include "core/layers/Layer.h"
#include "ui/canvascontextmenu.h"

using namespace cc;

class TestCanvasContextMenu final : public QObject
{
    Q_OBJECT

private slots:
    void nullAndBackgroundLayerReturnNull()
    {
        Document doc(800, 600);
        auto bg = std::make_unique<BackgroundLayer>();
        const LayerId bgId = bg->id();
        doc.addLayer(std::move(bg));

        // Invalid layer ID returns nullptr
        auto menu1 = CanvasContextMenu::buildMenu(nullptr, doc, LayerId(), nullptr);
        QVERIFY(menu1 == nullptr);

        // BackgroundLayer cannot open context menu
        auto menu2 = CanvasContextMenu::buildMenu(nullptr, doc, bgId, nullptr);
        QVERIFY(menu2 == nullptr);
    }

    void standardShapeLayerActions()
    {
        Document doc(800, 600);
        auto shape = std::make_unique<ShapeLayer>();
        shape->name = QStringLiteral("Box");
        const LayerId shapeId = shape->id();
        doc.addLayer(std::move(shape));

        auto menu = CanvasContextMenu::buildMenu(nullptr, doc, shapeId, nullptr);
        QVERIFY(menu != nullptr);

        const QList<QAction*> actions = menu->actions();
        QStringList texts;
        for (QAction* act : actions) {
            if (!act->isSeparator())
                texts.append(act->text());
        }

        QVERIFY(texts.contains(QStringLiteral("Duplicate")));
        QVERIFY(texts.contains(QStringLiteral("Delete")));
        QVERIFY(texts.contains(QStringLiteral("Bring to Front")));
        QVERIFY(texts.contains(QStringLiteral("Send to Back")));
        QVERIFY(texts.contains(QStringLiteral("Flip Horizontal")));
        QVERIFY(texts.contains(QStringLiteral("Flip Vertical")));
        QVERIFY(texts.contains(QStringLiteral("Center on Canvas")));
        QVERIFY(texts.contains(QStringLiteral("Lock")));

        // Layer locked should show "Unlock"
        doc.setLayerLocked(shapeId, true);
        auto menuLocked = CanvasContextMenu::buildMenu(nullptr, doc, shapeId, nullptr);
        QVERIFY(menuLocked != nullptr);
        QStringList lockedTexts;
        for (QAction* act : menuLocked->actions()) {
            if (!act->isSeparator())
                lockedTexts.append(act->text());
        }
        QVERIFY(lockedTexts.contains(QStringLiteral("Unlock")));
        QVERIFY(!lockedTexts.contains(QStringLiteral("Lock")));
    }

    void imageLayerWithSelectionActions()
    {
        Document doc(800, 600);
        auto img = std::make_unique<ImageLayer>();
        img->name = QStringLiteral("Photo");
        const LayerId imgId = img->id();
        doc.addLayer(std::move(img));

        // Without pixel selection
        {
            auto menu = CanvasContextMenu::buildMenu(nullptr, doc, imgId, nullptr);
            QVERIFY(menu != nullptr);
            QStringList texts;
            for (QAction* act : menu->actions()) {
                if (!act->isSeparator())
                    texts.append(act->text());
            }
            // Has AI background options
            QVERIFY(texts.contains(QStringLiteral("⚡ Quick Background Removal (1-Click)")));
            // Does not have mask options
            QVERIFY(!texts.contains(QStringLiteral("Mask: keep inside selection")));
        }

        // With pixel selection
        {
            QImage mask(800, 600, QImage::Format_Alpha8);
            mask.fill(255);
            doc.setPixelSelection(mask);

            auto menu = CanvasContextMenu::buildMenu(nullptr, doc, imgId, nullptr);
            QVERIFY(menu != nullptr);
            QStringList texts;
            for (QAction* act : menu->actions()) {
                if (!act->isSeparator())
                    texts.append(act->text());
            }
            QVERIFY(texts.contains(QStringLiteral("Mask: keep inside selection")));
            QVERIFY(texts.contains(QStringLiteral("Mask: keep outside selection")));
            QVERIFY(texts.contains(QStringLiteral("Copy selection to new layer")));
            QVERIFY(texts.contains(QStringLiteral("Cut selection to new layer")));
            QVERIFY(texts.contains(QStringLiteral("Invert selection")));
            QVERIFY(texts.contains(QStringLiteral("Cancel selection")));
        }
    }
};

QTEST_MAIN(TestCanvasContextMenu)
#include "test_canvas_context_menu.moc"
