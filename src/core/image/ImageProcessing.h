#pragma once

#include <QColor>
#include <QImage>
#include <QPoint>
#include <QPolygonF>
#include <QRect>
#include <QSize>

namespace cc {

/// Resultado retornado pela operacao de corte com tesoura (Scissors Cut).
/// Contem a imagem resultante e o deslocamento top-left caso tenha sido recortada para a caixa delimitadora.
struct ScissorsCutResult
{
    QImage image;
    QPoint offset; // Posicao superior-esquerda (X, Y) relativa a imagem original
};

/// Motor de processamento raster do CreatorCanvas.
/// Encapsula operacoes de manipulacao direta de pixels sem dependencias de interface (UI).
class ImageProcessing
{
public:
    /// Recorta uma imagem para o retangulo especificado (limitado aos limites validos da imagem).
    /// Retorna QImage vazia caso cropRect seja invalido ou nao intercepte a imagem.
    static QImage cropImage(const QImage& source, const QRect& cropRect);

    /// Calcula um retangulo de corte centralizado para uma proporcao de aspecto (aspect ratio = largura / altura).
    /// Exemplos: 1.0 (quadrado 1:1), 16.0/9.0 (widescreen), etc. Se aspectRatio <= 0, retorna o retangulo inteiro.
    static QRect calculateAspectCropRect(const QSize& sourceSize, double aspectRatio);

    /// Remocao de fundo por tolerancia de cor (Varinha Magica / Magic Wand flood fill).
    /// tolerance: 0 a 100 (porcentagem da distancia maxima euclidiana de cor).
    /// contiguous: se true, apenas pixels conectados ao ponto de origem (seedPoint) sao removidos.
    ///             se false, todos os pixels correspondentes em toda a imagem se tornam transparentes.
    static QImage removeBackground(const QImage& source, const QPoint& seedPoint,
                                   int tolerance = 20, bool contiguous = true);

    /// Corte com tesoura ("Corte com Tesoura" / Scissors Cut) ao longo de um poligono arbitrario.
    /// keepInside: se true, mantem o interior do poligono e torna o exterior transparente.
    ///             se false, remove o interior (abre um orificio transparente) e mantem o exterior.
    /// cropToBoundingRect: se true (e keepInside for true), ajusta a imagem final para os limites do corte.
    static ScissorsCutResult scissorsCut(const QImage& source, const QPolygonF& polygon,
                                         bool keepInside = true, bool cropToBoundingRect = false);

    /// Carimbo de Clonagem (Clone Stamp): copia uma area circular de pixels da imagem de origem para o destino.
    /// radius: raio do pincel circular em pixels (> 0).
    /// opacity: opacidade de 0.0 (invisivel) a 1.0 (totalmente opaco).
    /// hardness: dureza da borda de 0.0 (suave com gradiente radial) a 1.0 (borda nitida).
    static QImage cloneStamp(const QImage& target, const QImage& source,
                             const QPoint& srcPoint, const QPoint& dstPoint,
                             int radius, qreal opacity = 1.0, qreal hardness = 0.8);
};

} // namespace cc
