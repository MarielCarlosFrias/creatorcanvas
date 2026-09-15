#pragma once

#include <QImage>
#include <QString>
#include <atomic>
#include <memory>

namespace cc {

class BackgroundRemoverImpl;

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

    /// Processa uma QImage de entrada e retorna a imagem recortada com canal alpha transparente.
    /// Suporta cancelamento cooperativo atraves de cancelFlag.
    QImage removeBackground(const QImage& input, const std::atomic<bool>* cancelFlag = nullptr);

    /// Localiza o caminho absoluto para o arquivo de modelo .onnx nos caminhos padrao do sistema.
    static QString findModelPath(const QString& preferredName = QStringLiteral("u2netp.onnx"));

    /// Verifica se ha modelos neurais disponiveis na instalacao.
    static bool isAvailable();

private:
    std::unique_ptr<BackgroundRemoverImpl> m_impl;
};

} // namespace cc
