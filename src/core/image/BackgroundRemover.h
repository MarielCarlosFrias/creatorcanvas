#pragma once

#include <QImage>
#include <QString>
#include <atomic>
#include <memory>

namespace cc {

class BackgroundRemoverImpl;

/// Opcoes de refinamento do recorte (matte). Aplicadas sobre a mascara bruta
/// da rede neural para produzir um recorte limpo: sujeito 100% opaco,
/// fundo 100% transparente, borda suavizada e manchas residuais removidas.
struct MatteOptions
{
    float cutLevel = 0.5f;    ///< 0..1 — ponto central do corte (0.5 = padrao)
    float softness = 0.10f;   ///< 0..0.4 — largura da rampa suave na borda
    bool removeSpecks = true; ///< remove manchas/componentes flutuantes
    bool fillHoles = true;    ///< buracos internos do sujeito ficam opacos
    int edgeFeather = 1;      ///< passes de suavizacao 3x3 na borda (0..3)
};

/// Motor de Remocao de Fundo com Inteligencia Artificial (ONNX Runtime / U2-Net).
/// Executa inferencia neural local offline para segmentacao de silhuetas e objetos.
class BackgroundRemover
{
public:
    explicit BackgroundRemover(const QString& modelPath = QString());
    ~BackgroundRemover();

    BackgroundRemover(const BackgroundRemover&) = delete;
    BackgroundRemover& operator=(const BackgroundRemover&) = delete;

    /// Retorna se a sessao do modelo neural foi carregada com sucesso.
    bool isLoaded() const;

    /// Processa uma QImage de entrada e retorna a imagem recortada com canal
    /// alpha transparente. Suporta cancelamento cooperativo. Se rawAlphaOut
    /// nao for nulo, recebe a mascara bruta normalizada (0-255) no tamanho da
    /// entrada — util para re-aplicar o matte sem nova inferencia.
    QImage removeBackground(const QImage& input,
                            const std::atomic<bool>* cancelFlag = nullptr,
                            const MatteOptions& options = {},
                            QImage* rawAlphaOut = nullptr);

    /// Re-aplica o refinamento do matte sobre uma mascara bruta ja existente
    /// (sem inferencia). Estatico e barato — permite ajustes ao vivo na UI.
    /// cancelFlag interrompe os loops pesados (flood fill / componentes
    /// conectados) quando o usuario cancela o recorte.
    static QImage applyMatte(const QImage& input, const QImage& rawAlpha,
                             const MatteOptions& options = {},
                             const std::atomic<bool>* cancelFlag = nullptr);

    /// Localiza o caminho absoluto para o arquivo de modelo .onnx nos caminhos padrao do sistema.
    static QString findModelPath(const QString& preferredName = QStringLiteral("u2netp.onnx"));

    /// Verifica se ha modelos neurais disponiveis na instalacao.
    static bool isAvailable();

private:
    std::unique_ptr<BackgroundRemoverImpl> m_impl;
};

} // namespace cc
