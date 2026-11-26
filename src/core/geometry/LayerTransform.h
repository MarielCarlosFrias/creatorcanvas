#pragma once

#include "core/Document.h"

#include <QTransform>

namespace cc {

/// Matriz do espaço do pai imediato de `layer` para o documento. A raiz é
/// apenas um contentor e não participa na transformação. QTransform usa a
/// mesma convenção de QPainter: local × pai, do pai imediato até ao externo.
inline QTransform layerParentToDocumentMatrix(const Document& document,
                                              const Layer& layer)
{
    QTransform result;
    GroupLayer* parent = document.parentOf(layer.id());
    if (!parent || parent == document.rootGroup())
        return result;

    result = parent->transform.matrix(parent->contentBounds());
    for (GroupLayer* ancestor = document.parentOf(parent->id());
         ancestor && ancestor != document.rootGroup();
         ancestor = document.parentOf(ancestor->id())) {
        result = result * ancestor->transform.matrix(ancestor->contentBounds());
    }
    return result;
}

/// Matriz completa de uma transformação local para o documento. O overload
/// com `localTransform` permite calcular a prévia de um gesto antes de a
/// gravar na camada, sem alterar a composição dos grupos ancestrais.
inline QTransform layerToDocumentMatrix(const Document& document,
                                        const Layer& layer,
                                        const AffineTransform& localTransform)
{
    return localTransform.matrix(layer.contentBounds())
        * layerParentToDocumentMatrix(document, layer);
}

inline QTransform layerToDocumentMatrix(const Document& document,
                                        const Layer& layer)
{
    return layerToDocumentMatrix(document, layer, layer.transform);
}

} // namespace cc
