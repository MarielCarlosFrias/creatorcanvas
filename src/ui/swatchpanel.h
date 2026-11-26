#pragma once

#include <QColor>
#include <QVector>
#include <QWidget>

namespace cc {

/// Painel de amostras de cor (swatches). Clique escolhe a cor de pintura;
/// o último espaço ("+") adiciona a cor atual; botão direito numa amostra
/// remove-a. A paleta persiste em QSettings.
class SwatchPanel final : public QWidget
{
    Q_OBJECT
public:
    explicit SwatchPanel(QWidget* parent = nullptr);

    void setCurrentColor(const QColor& c) { m_current = c; }
    QColor currentColor() const { return m_current; }

signals:
    void colorSelected(const QColor& color);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    QSize sizeHint() const override { return {180, 120}; }

private:
    void load();
    void save() const;
    int swatchAt(const QPoint& p) const; // -1 = fora; == m_colors.size() → botão "+"

    QVector<QColor> m_colors;
    QColor m_current{230, 50, 50};
    int m_swatchSize = 24;
    int m_spacing = 4;
    int m_columns = 6;
};

} // namespace cc
