#pragma once

#include <QWidget>
#include <QImage>
#include <QColor>
#include <QPoint>

namespace cc {

class MaskEditCanvas : public QWidget {
    Q_OBJECT
public:
    enum Mode { Restore, Erase };

    explicit MaskEditCanvas(QWidget* parent = nullptr);

    void setImage(const QImage& original, const QImage& currentMasked, const QColor& bgColor = QColor());
    void setBackgroundColor(const QColor& bgColor);
    void setBrushRadius(int radius);
    QImage getEditedImage() const;
    void undo();
    void clearEdits();
    bool hasEdits() const;

signals:
    void editingDone();

public slots:
    void setMode(Mode mode);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    QPoint widgetToImage(const QPoint& wgt) const;
    QRect getPixmapRect() const;
    float scaleFactor() const;
    void applyBrush(const QPoint& imgPos);
    static QImage renderWithBackground(const QImage& img, const QColor& bgColor);

    QImage m_originalImage;
    QImage m_editBuffer;
    QColor m_bgColor;
    Mode m_mode = Restore;
    int m_brushRadius = 15;
    bool m_painting = false;
    QPoint m_lastPoint;
    QPoint m_cursorPos{-1, -1};
    QImage m_undoBuffer;
    bool m_hasUndo = false;
};

} // namespace cc
