#include "core/Document.h"
#include "core/history/CommandStack.h"
#include "core/selection/SelectionEngine.h"
#include "services/SelectionTransform.h"
#include "ui/transformselectiondialog.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QtTest>

#include <limits>

using namespace cc;

namespace {

QImage rectangularSelection(const QSize& size, const QRect& rectangle)
{
    QImage mask(size, QImage::Format_Alpha8);
    mask.fill(0);
    for (int y = rectangle.top(); y <= rectangle.bottom(); ++y) {
        uchar* row = mask.scanLine(y);
        for (int x = rectangle.left(); x <= rectangle.right(); ++x)
            row[x] = 255;
    }
    return mask;
}

bool hasPartialCoverage(const QImage& mask)
{
    if (mask.isNull())
        return false;
    for (int y = 0; y < mask.height(); ++y) {
        const uchar* row = mask.constScanLine(y);
        for (int x = 0; x < mask.width(); ++x)
            if (row[x] != 0 && row[x] != 255)
                return true;
    }
    return false;
}

} // namespace

class TestSelectionTransform final : public QObject
{
    Q_OBJECT

private slots:
    void identityAndInvalidInput()
    {
        const QImage source = rectangularSelection(QSize(12, 10), QRect(2, 3, 4, 2));
        const SelectionTransformParameters identity;
        QVERIFY(SelectionTransform::isValid(identity));
        QVERIFY(SelectionTransform::isIdentity(identity));
        QVERIFY(SelectionTransform::apply(QImage(), identity).isNull());

        const QImage unchanged = SelectionTransform::apply(source, identity);
        QCOMPARE(unchanged.format(), QImage::Format_Alpha8);
        QCOMPARE(unchanged.size(), source.size());
        QVERIFY(unchanged == source);

        SelectionTransformParameters fullTurn;
        fullTurn.rotationDegrees = 360.0;
        QVERIFY(SelectionTransform::isIdentity(fullTurn));
        QVERIFY(SelectionTransform::apply(source, fullTurn) == source);

        SelectionTransformParameters invalid;
        invalid.scaleXPercent = 0.0;
        QVERIFY(!SelectionTransform::isValid(invalid));
        QVERIFY(SelectionTransform::apply(source, invalid).isNull());

        invalid = SelectionTransformParameters();
        invalid.scaleYPercent = std::numeric_limits<double>::quiet_NaN();
        QVERIFY(!SelectionTransform::isValid(invalid));
        QVERIFY(SelectionTransform::apply(source, invalid).isNull());

        invalid = SelectionTransformParameters();
        invalid.translation = QPointF(std::numeric_limits<double>::infinity(), 0.0);
        QVERIFY(!SelectionTransform::isValid(invalid));
    }

    void integerTranslationIsExactAndNonDestructive()
    {
        const QImage source = rectangularSelection(QSize(16, 12), QRect(2, 3, 4, 2));
        const QImage originalCopy = source;
        SelectionTransformParameters parameters;
        parameters.translation = QPointF(3.0, 1.0);

        const QImage result = SelectionTransform::apply(source, parameters);
        const QImage expected = rectangularSelection(QSize(16, 12), QRect(5, 4, 4, 2));
        QVERIFY(result == expected);
        QCOMPARE(SelectionEngine::boundingBox(result), QRect(5, 4, 4, 2));
        QVERIFY(source == originalCopy);
    }

    void fractionalTranslationKeepsAntialiasing()
    {
        const QImage source = rectangularSelection(QSize(16, 12), QRect(2, 3, 4, 2));
        SelectionTransformParameters parameters;
        parameters.translation = QPointF(0.5, 0.0);

        const QImage result = SelectionTransform::apply(source, parameters);
        QVERIFY(!result.isNull());
        QCOMPARE(result.format(), QImage::Format_Alpha8);
        QCOMPARE(result.size(), source.size());
        QVERIFY(hasPartialCoverage(result));
        QVERIFY(result.constScanLine(3)[2] > 0);
        QVERIFY(result.constScanLine(3)[2] < 255);
        QCOMPARE(result.constScanLine(3)[3], uchar(255));
    }

    void scalesAroundSelectionOutlineCenter()
    {
        const QImage source = rectangularSelection(QSize(24, 24), QRect(6, 8, 4, 4));
        QCOMPARE(SelectionTransform::defaultPivot(source), QPointF(8.0, 10.0));

        SelectionTransformParameters parameters;
        parameters.scaleXPercent = 200.0;
        parameters.scaleYPercent = 50.0;
        const QImage result = SelectionTransform::apply(source, parameters);
        QVERIFY(!result.isNull());
        QCOMPARE(result.size(), source.size());
        // O kernel bilinear acrescenta uma franja de 1 px em cada lado da
        // escala horizontal; os pixels interiores continuam totalmente cheios.
        QCOMPARE(SelectionEngine::boundingBox(result), QRect(3, 9, 10, 2));
        QVERIFY(result.constScanLine(10)[8] > 240);
    }

    void rotatesAroundSelectionOutlineCenter()
    {
        const QImage source = rectangularSelection(QSize(20, 20), QRect(3, 7, 4, 2));
        SelectionTransformParameters parameters;
        parameters.rotationDegrees = 90.0;

        const QImage result = SelectionTransform::apply(source, parameters);
        QVERIFY(!result.isNull());
        QCOMPARE(result.size(), source.size());
        QCOMPARE(SelectionEngine::boundingBox(result), QRect(4, 6, 2, 4));
        QVERIFY(result.constScanLine(8)[5] > 240);
    }

    void supportsAnExplicitPivot()
    {
        const QImage source = rectangularSelection(QSize(20, 20), QRect(3, 7, 4, 2));
        SelectionTransformParameters parameters;
        parameters.pivot = QPointF(5.0, 8.0);
        parameters.rotationDegrees = 90.0;
        const QImage centered = SelectionTransform::apply(source, parameters);

        parameters.pivot = QPointF(3.0, 7.0);
        const QImage aroundCorner = SelectionTransform::apply(source, parameters);
        QVERIFY(!aroundCorner.isNull());
        QVERIFY(aroundCorner != centered);
        QVERIFY(SelectionEngine::boundingBox(aroundCorner).x()
                < SelectionEngine::boundingBox(centered).x());
        QVERIFY(SelectionEngine::boundingBox(aroundCorner).y()
                > SelectionEngine::boundingBox(centered).y());
    }

    void clipsWithoutChangingMaskDimensions()
    {
        const QImage source = rectangularSelection(QSize(12, 10), QRect(1, 1, 2, 2));
        SelectionTransformParameters parameters;
        parameters.translation = QPointF(2.0, 0.0);
        const QImage clipped = SelectionTransform::apply(source, parameters);
        QCOMPARE(clipped.size(), source.size());
        QCOMPARE(SelectionEngine::boundingBox(clipped), QRect(3, 1, 2, 2));

        parameters.translation = QPointF(-50.0, 0.0);
        QVERIFY(SelectionTransform::apply(source, parameters).isNull());
        parameters.translation = QPointF(std::numeric_limits<double>::max(), 0.0);
        QVERIFY(SelectionTransform::apply(source, parameters).isNull());

        const QImage empty(12, 10, QImage::Format_Alpha8);
        QVERIFY(SelectionTransform::apply(empty, parameters).isNull());
    }

    void undoRedoPreservesSelectionSpace()
    {
        for (const SelectionSpace space : {SelectionSpace::Document, SelectionSpace::Layer}) {
            Document document(20, 20);
            const QImage source = rectangularSelection(QSize(20, 20), QRect(2, 3, 4, 3));
            document.setPixelSelection(source, space);
            CommandStack history;
            SelectionTransformParameters parameters;
            parameters.translation = QPointF(4.0, 2.0);

            QVERIFY(SelectionTransform::apply(document, &history, parameters));
            QCOMPARE(history.undoCount(), 1);
            QCOMPARE(history.nextUndoName(), QStringLiteral("selection.transform"));
            QCOMPARE(document.pixelSelectionSpace(), space);
            const QImage transformed = document.pixelSelection();
            QVERIFY(transformed != source);

            history.undo();
            QVERIFY(document.pixelSelection() == source);
            QCOMPARE(document.pixelSelectionSpace(), space);
            QCOMPARE(history.redoCount(), 1);

            history.redo();
            QVERIFY(document.pixelSelection() == transformed);
            QCOMPARE(document.pixelSelectionSpace(), space);
        }
    }

    void directApplyAndIdentityHistoryBehaviour()
    {
        Document document(20, 20);
        const QImage source = rectangularSelection(QSize(20, 20), QRect(2, 3, 4, 3));
        document.setPixelSelection(source, SelectionSpace::Layer);

        QVERIFY(SelectionTransform::apply(document, nullptr, SelectionTransformParameters()));
        QVERIFY(document.pixelSelection() == source);
        QCOMPARE(document.pixelSelectionSpace(), SelectionSpace::Layer);

        CommandStack history;
        QVERIFY(SelectionTransform::apply(document, &history, SelectionTransformParameters()));
        QCOMPARE(history.undoCount(), 0);

        SelectionTransformParameters parameters;
        parameters.scaleXPercent = 150.0;
        QVERIFY(SelectionTransform::apply(document, nullptr, parameters));
        QVERIFY(document.pixelSelection() != source);
        QCOMPARE(document.pixelSelectionSpace(), SelectionSpace::Layer);
    }

    void invalidOrMissingSelectionDoesNotCreateHistory()
    {
        Document document(20, 20);
        CommandStack history;
        SelectionTransformParameters parameters;
        parameters.translation = QPointF(1.0, 0.0);
        QVERIFY(!SelectionTransform::apply(document, &history, parameters));
        QCOMPARE(history.undoCount(), 0);

        const QImage source = rectangularSelection(QSize(20, 20), QRect(2, 3, 4, 3));
        document.setPixelSelection(source, SelectionSpace::Layer);
        parameters.scaleXPercent = 0.0;
        QVERIFY(!SelectionTransform::apply(document, &history, parameters));
        QCOMPARE(history.undoCount(), 0);
        QVERIFY(document.pixelSelection() == source);
        QCOMPARE(document.pixelSelectionSpace(), SelectionSpace::Layer);
    }

    void clippingToEmptySelectionIsUndoable()
    {
        Document document(20, 20);
        const QImage source = rectangularSelection(QSize(20, 20), QRect(2, 3, 4, 3));
        document.setPixelSelection(source, SelectionSpace::Layer);
        CommandStack history;
        SelectionTransformParameters parameters;
        parameters.translation = QPointF(100.0, 0.0);

        QVERIFY(SelectionTransform::apply(document, &history, parameters));
        QVERIFY(document.pixelSelection().isNull());
        QCOMPARE(history.undoCount(), 1);

        history.undo();
        QVERIFY(document.pixelSelection() == source);
        QCOMPARE(document.pixelSelectionSpace(), SelectionSpace::Layer);
        history.redo();
        QVERIFY(document.pixelSelection().isNull());
    }

    void dialogUsesTheSameTransformPipeline()
    {
        const QImage source = rectangularSelection(QSize(20, 20), QRect(2, 3, 4, 3));
        TransformSelectionDialog dialog(source);
        QVERIFY(dialog.transformedSelection() == source);

        auto* translateX = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("translateX"));
        auto* translateY = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("translateY"));
        auto* scaleX = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("scaleX"));
        auto* scaleY = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("scaleY"));
        auto* rotation = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("rotation"));
        QVERIFY(translateX && translateY && scaleX && scaleY && rotation);

        translateX->setValue(2.0);
        translateY->setValue(-1.0);
        scaleX->setValue(150.0);
        scaleY->setValue(75.0);
        rotation->setValue(30.0);
        const SelectionTransformParameters parameters = dialog.parameters();
        QCOMPARE(parameters.translation, QPointF(2.0, -1.0));
        QCOMPARE(parameters.scaleXPercent, 150.0);
        QCOMPARE(parameters.scaleYPercent, 75.0);
        QCOMPARE(parameters.rotationDegrees, 30.0);
        QVERIFY(dialog.transformedSelection()
                == SelectionTransform::apply(source, parameters));
        QVERIFY(source == rectangularSelection(QSize(20, 20), QRect(2, 3, 4, 3)));

        auto* buttons = dialog.findChild<QDialogButtonBox*>();
        QVERIFY(buttons);
        buttons->button(QDialogButtonBox::Reset)->click();
        QVERIFY(SelectionTransform::isIdentity(dialog.parameters()));
        QVERIFY(dialog.transformedSelection() == source);
    }
};

QTEST_MAIN(TestSelectionTransform)
#include "test_selection_transform.moc"
