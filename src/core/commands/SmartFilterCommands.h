#pragma once

#include "AdjustmentLayerCommands.h"

namespace cc {

/// Aplica um Smart Filter a uma camada de imagem num único passo de undo.
///
/// O caminho não destrutivo (default) cria ou actualiza uma AdjustmentLayer
/// ligada; o caminho destrutivo coze o resultado num novo asset. A política e
/// os comandos concretos são partilhados com executeApplyAdjustment para que
/// undo/redo, locks e retenção de assets tenham exactamente a mesma semântica.
/// Devolve false para tipos que não sejam Smart Filters ou destinos inválidos.
inline bool executeApplySmartFilter(
    Document& doc, CommandStack* history, const LayerId& imageLayerId,
    AdjustmentType type, const AdjustmentParams& params,
    bool nonDestructive = true,
    const QString& name = QStringLiteral("smartFilter.apply"))
{
    if (!isSmartFilterType(type))
        return false;
    return executeApplyAdjustment(doc, history, imageLayerId, type, params,
                                  nonDestructive, name);
}

} // namespace cc
