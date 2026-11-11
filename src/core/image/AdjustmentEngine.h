#pragma once

#include <QImage>
#include <QString>

#include <utility>
#include <vector>

#include "core/layers/Layer.h"
#include "core/layers/ToneAdjustments.h"

namespace cc {

/// Parâmetros de uma camada de ajuste: pares chave→valor serializáveis
/// (formato do manifest .creatorcanvas). O motor de renderização e a UI
/// falam exactamente a mesma linguagem através deste tipo.
using AdjustmentParams = std::vector<std::pair<QString, double>>;

/// Lê um parâmetro com default (ausente = identidade na maioria dos casos).
double adjustmentParam(const AdjustmentParams& params, const QString& key, double def);

/// Lê um canal de níveis dos parâmetros. Prefixos usados:
/// "levels.master", "levels.r", "levels.g", "levels.b".
LevelsChannel levelsFromParams(const AdjustmentParams& params, const QString& prefix);

/// Reconstrói os pontos de uma curva: "curves.<canal>.count" +
/// "curves.<canal>.<i>.in/out". Menos de 2 pontos (ou valores fora de
/// 0..1, ou count > 64) devolve curva vazia = identidade — parâmetro
/// corrompido nunca quebra o render.
std::vector<CurvePoint> curveFromParams(const AdjustmentParams& params, const QString& prefix);

/// Encoders usados pela UI para produzir os parâmetros.
AdjustmentParams encodeLevels(const LevelsChannel& master, const LevelsChannel& red,
                              const LevelsChannel& green, const LevelsChannel& blue);
AdjustmentParams encodeCurves(const std::vector<CurvePoint>& master,
                              const std::vector<CurvePoint>& red = {},
                              const std::vector<CurvePoint>& green = {},
                              const std::vector<CurvePoint>& blue = {});

/// Aplica um ajuste de qualquer tipo sobre uma imagem usando os parâmetros
/// serializados. `strength` (0..1) mistura original→ajustado, como a
/// opacidade de uma camada de ajuste no renderer.
/// Imagem nula ou parâmetros identidade devolvem a própria imagem (sem
/// cópia), para que prévias e renders não paguem por ajustes vazios.
QImage applyAdjustmentParameters(const QImage& source, AdjustmentType kind,
                                 const AdjustmentParams& params, float strength = 1.0f);

} // namespace cc
