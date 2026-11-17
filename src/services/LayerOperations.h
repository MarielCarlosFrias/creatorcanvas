#pragma once

#include <QList>
#include <QString>
#include <memory>

#include "core/layers/Layer.h"

namespace cc {

class Document;
class CommandStack;
class ImageLayer;

namespace LayerOperations {

/// Verifica se as camadas especificadas pertencem ao mesmo grupo e podem ser mescladas.
bool areMergeableSiblings(const Document& document, const QList<LayerId>& ids);

/// Cria uma camada rasterizada temporária a partir da renderização de camadas irmãs.
std::unique_ptr<ImageLayer> rasterLayerFor(Document& document,
                                           const QList<LayerId>& layerIds,
                                           const QString& name);

/// Mescla camadas selecionadas. Se houver apenas 1 camada, faz Merge Down com a camada imediatamente abaixo.
/// Retorna o LayerId da camada mesclada ou LayerId() se a operação falhou.
LayerId mergeSelected(Document& document,
                      CommandStack* history,
                      QList<LayerId> selectedIds,
                      const QString& defaultName = QStringLiteral("Merged layers"));

/// Mescla todas as camadas visíveis e desbloqueadas (exceto Background).
/// Retorna o LayerId da camada mesclada ou LayerId() se houver menos de 2 camadas válidas.
LayerId mergeVisible(Document& document,
                     CommandStack* history,
                     const QString& defaultName = QStringLiteral("Visible merge"));

/// Achata a imagem (Flatten), preservando camadas bloqueadas intactas por cima.
/// Retorna o LayerId da camada achatada ou LayerId() se todas as camadas estiverem bloqueadas.
LayerId flatten(Document& document,
                CommandStack* history,
                const QString& defaultName = QStringLiteral("Flattened image"));

/// Aplica o efeito "Texto Atrás do Personagem" (Text Behind Subject) criando uma duplicata da imagem
/// recortada acima do texto.
bool textBehindSubject(Document& document,
                       CommandStack* history,
                       const LayerId& textId,
                       const LayerId& subjectId);

/// Agrupa as camadas especificadas.
/// Retorna o LayerId do novo grupo criado.
LayerId groupSelected(Document& document,
                      CommandStack* history,
                      const QList<LayerId>& ids,
                      const QString& groupName = QStringLiteral("Group"));

/// Desagrupa o grupo especificado.
/// Retorna o LayerId do último elemento filho para seleção seguinte.
LayerId ungroup(Document& document,
                CommandStack* history,
                const LayerId& groupId);

/// Inverte a escala horizontal ou vertical da camada (Flip H/V).
bool flip(Document& document,
          CommandStack* history,
          const LayerId& id,
          bool horizontal);

/// Rotaciona a camada em 'degrees' graus relativos.
bool rotate(Document& document,
            CommandStack* history,
            const LayerId& id,
            double degrees);

/// Remove a camada especificada através da pilha de histórico.
bool deleteLayer(Document& document,
                 CommandStack* history,
                 const LayerId& id);

} // namespace LayerOperations
} // namespace cc
