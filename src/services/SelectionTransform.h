#pragma once

#include <QImage>
#include <QPointF>
#include <QString>

#include <optional>

namespace cc {

class CommandStack;
class Document;

/// Parâmetros de Transform Selection. A translação está em pixels do espaço
/// da própria máscara; escala e rotação usam o centro do contorno por omissão.
/// Um pivot explícito permite aos consumidores interactivos usar outro ponto
/// de referência sem converter coordenadas nem alterar o SelectionSpace.
struct SelectionTransformParameters {
    QPointF translation;
    double scaleXPercent = 100.0;
    double scaleYPercent = 100.0;
    double rotationDegrees = 0.0;
    std::optional<QPointF> pivot;
};

/// Transforma apenas o contorno/máscara da selecção, nunca os pixels das
/// camadas. O resultado mantém as dimensões da máscara de entrada e é recortado
/// por esses limites. A sobrecarga com Document aplica um único passo de undo
/// e preserva explicitamente o SelectionSpace activo (inclusive Layer quando
/// a camada e o documento têm o mesmo tamanho).
class SelectionTransform final
{
public:
    SelectionTransform() = delete;

    static bool isValid(const SelectionTransformParameters& parameters);
    static bool isIdentity(const SelectionTransformParameters& parameters);

    /// Centro das arestas do contorno (não o centro da bounding box em
    /// coordenadas de índices). Para uma máscara vazia usa o centro da imagem.
    static QPointF defaultPivot(const QImage& selection);

    /// Amostragem inversa bilinear nos centros dos pixels. Devolve Alpha8 com
    /// o tamanho original; devolve uma QImage nula para entrada/parâmetros
    /// inválidos ou quando todo o contorno sai dos limites da máscara.
    static QImage apply(const QImage& selection,
                        const SelectionTransformParameters& parameters);

    /// Aplica ao Document através de CommandStack quando disponível, ou
    /// directamente sem histórico. Uma operação de identidade não cria um
    /// comando. Devolve false se não existir selecção ou os parâmetros forem
    /// inválidos; true indica estado válido (incluindo um no-op de identidade).
    static bool apply(Document& document, CommandStack* history,
                      const SelectionTransformParameters& parameters,
                      const QString& commandName = QStringLiteral("selection.transform"));
};

} // namespace cc
