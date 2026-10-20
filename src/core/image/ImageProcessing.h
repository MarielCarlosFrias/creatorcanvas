#pragma once

#include <QColor>
#include <QImage>
#include <QPoint>
#include <QPolygonF>
#include <QRect>
#include <QSize>
#include "../layers/ToneAdjustments.h"

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

    // Tipos de Pincel Estilo Paint
    enum class BrushType {
        Brush,        // Pincel redondo suave (antialiased)
        Pencil,       // Lápis / Caneta precisa (pixelado/duro)
        Highlighter,  // Marcador translúcido
        Airbrush,     // Spray / Aerógrafo com dispersão estocástica
        Eraser        // Borracha (limpa pixels para transparente)
    };

    /// Pinta um traço contínuo entre prevPoint e curPoint com o pincel selecionado.
    static QImage paintStroke(const QImage& target, const QPointF& prevPoint, const QPointF& curPoint,
                             BrushType brush, const QColor& color, int size, qreal opacity = 1.0);

    /// Balde de Tinta (Flood Fill): preenche uma área contígua de mesma cor com fillColor.
    static QImage floodFill(const QImage& source, const QPoint& seedPoint,
                            const QColor& fillColor, int tolerance = 20);

    // Ajustes Rápidos de Imagem e Correção de Cor
    /// Aplica ajustes de brilho (-100 a +100), contraste (-100 a +100), saturação (-100 a +100), temperatura (-100 a +100) e matiz (-180 a +180).
    static QImage adjustColors(const QImage& source, double brightness, double contrast,
                              double saturation, double temperature, double hue = 0.0,
                              double shadows = 0.0, double highlights = 0.0,
                              double vignette = 0.0);

    /// Aplica desfoque gaussiano / suavização rápida.
    static QImage applyBlur(const QImage& source, double radius);

    /// Aplica nitidez (Sharpen).
    static QImage applySharpen(const QImage& source, double amount);

    /// Aplica ajuste de Níveis (Levels) por canal ou master.
    static QImage applyLevels(const QImage& source, const LevelsChannel& master,
                             const LevelsChannel& red = {}, const LevelsChannel& green = {},
                             const LevelsChannel& blue = {});

    /// Aplica ajuste de Curvas (Curves) por canal ou master.
    static QImage applyCurves(const QImage& source, const std::vector<CurvePoint>& master,
                             const std::vector<CurvePoint>& red = {},
                             const std::vector<CurvePoint>& green = {},
                             const std::vector<CurvePoint>& blue = {});

    enum class PresetFilter {
        Grayscale,    // P&B / Preto e Branco
        Sepia,        // Sépia clássico
        Vintage,      // Retrô / Vintage quente
        HighContrast, // Alto contraste dinâmico
        Invert,       // Inverter Cores
        Posterize,    // Posterizar (níveis reduzidos de cor)
        Threshold     // Preto e branco puro por limiar
    };

    /// Aplica filtros rápidos em 1 clique.
    static QImage applyPresetFilter(const QImage& source, PresetFilter preset);

    /// Aplica mapa de gradiente (Gradient Map) mapeando a luminância de cada pixel
    /// para uma transição suave entre color1 e color2.
    static QImage applyGradientMap(const QImage& source, const QColor& color1,
                                   const QColor& color2, bool reverse = false);

    /// Remove uma cor da imagem (Chroma Key / Color to Alpha), tornando-a transparente
    /// e desmisturando as bordas perfeitamente sem halos residuais.
    /// transparency: threshold abaixo do qual fica 100% transparente (0.0 padrão)
    /// opacityThreshold: threshold acima do qual permanece opaco (1.0 padrão)
    /// tolerance: tolerância de distância da cor chave (0.0 a 1.0, padrão 0.0)
    static QImage colorToAlpha(const QImage& source, const QColor& keyColor,
                               double transparency = 0.0, double opacityThreshold = 1.0,
                               double tolerance = 0.0);

    // ===== Ferramentas de preenchimento e seleção (Photoshop-like) =====

    /// Preenche com um gradiente entre dois pontos. `mask` (Alpha8, opcional)
    /// limita o preenchimento a uma seleção. radial: gradiente radial.
    /// startColor com alfa 0 produz fade para transparente.
    static QImage paintGradient(const QImage& target, const QPointF& start,
                                const QPointF& end, const QColor& startColor,
                                const QColor& endColor, bool radial = false,
                                qreal opacity = 1.0, const QImage& mask = {});

    /// Cria uma seleção (Alpha8) de retângulo e/ou elipse já normalizada.
    static QImage makeShapeSelection(const QSize& size, const QRectF& rect,
                                     bool ellipse, int feather = 0);

    /// Combina duas seleções Alpha8: add = união (max), sub = diferença.
    static QImage combineSelections(const QImage& base, const QImage& overlay,
                                    bool subtract);

    /// Modos de retoque por pincel.
    enum class RetouchMode {
        Blur,    // Desfocar (Blur)
        Sharpen, // Nitidez (Sharpen)
        Smudge,  // Borrão / arrastar pixels (Smudge)
        Dodge,   // Clarear (Dodge)
        Burn,    // Escurecer (Burn)
        Sponge   // Saturar/desaturar (Sponge)
    };

    /// Aplica um traço de retoque contínuo entre dois pontos.
    /// size: diâmetro do pincel em px; strength: 0..1.
    static QImage retouchStroke(const QImage& target, const QPointF& prevPoint,
                                const QPointF& curPoint, RetouchMode mode,
                                int size, qreal strength = 0.4);

    /// Pincel de reparo: copia uma área da imagem para o ponto alvo com borda
    /// suave (Spot Healing). sourcePoint é a origem amostrada.
    static QImage spotHeal(const QImage& target, const QPoint& targetPoint,
                           const QPoint& sourcePoint, int radius,
                           qreal strength = 0.85);

    /// Remove olho vermelho: dessatura pixels avermelhados num raio.
    static QImage redEyeRemove(const QImage& target, const QPoint& center, int radius);
};

} // namespace cc
