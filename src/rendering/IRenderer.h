#pragma once

#include <QHash>
#include <QImage>
#include <QList>
#include <QPixmap>
#include <QRectF>
#include <QSize>
#include <QTransform>
#include <QUuid>

#include "CanvasRenderer.h"   // RenderOptions

class QPainter;

namespace cc {

class Document;
class Layer;

/// Interface do compositor. Existe para dar um caminho de fuga a backends
/// futuros (GPU, cache agressivo, RHI) sem que os chamadores precisem de
/// mudar: hoje `SoftwareRenderer` (QPainter, caminho rápido + composição de
/// máscaras) e `CachingRenderer` (o anterior + cache por camada).
class IRenderer
{
public:
    virtual ~IRenderer() = default;

    /// Compõe o documento sobre |painter| (já activo no dispositivo de
    /// destino). |docToDevice| mapeia pixels de documento para pixels de
    /// dispositivo.
    virtual void renderDocument(const Document& doc, QPainter* painter,
                                const QTransform& docToDevice,
                                const RenderOptions& options) = 0;

    /// Liberta toda a memória de cache (testes, troca de documento, pressão
    /// de memória). Nunca altera o resultado — só a velocidade.
    virtual void clearCaches() = 0;

    /// Nome legível para logs e diagnósticos.
    virtual const char* name() const = 0;
};

/// Renderer de software padrão (implementação histórica do compositor).
/// O cache por camada é um hook opcional — o caminho de composição
/// (máscaras de recorte/ajustes) nunca é cacheado.
class SoftwareRenderer : public IRenderer
{
public:
    void renderDocument(const Document& doc, QPainter* painter,
                        const QTransform& docToDevice,
                        const RenderOptions& options) override;

    void clearCaches() override;
    const char* name() const override { return "software"; }

    /// Invalida o cache de UMA camada (chamado por quem sabe exactamente o
    /// que mudou). Sem efeito se o renderer não usar cache.
    virtual void invalidateLayer(const QUuid& layerId);
    /// Invalida todo o cache de camadas.
    virtual void invalidateAllLayers();

protected:
    /// Tenta desenhar a camada a partir de cache. Devolve false se não
    /// cacheou — o chamador desenha então pelo caminho normal.
    /// A implementação base nunca cacheia.
    virtual bool drawCachedLayer(QPainter* painter, const Layer& layer,
                                 const Document& doc, const QRectF& canvasRect,
                                 const QTransform& docToDevice,
                                 const QRectF& canvasOnDevice,
                                 const RenderOptions& options);

    /// Caminho de composição: desenha o compósito da cache, se existir.
    /// A implementação base nunca cacheia.
    virtual bool compositeFromCache(QPainter* painter, const Document& doc,
                                    const QTransform& docToDevice,
                                    const QRectF& canvasOnDevice,
                                    const QSize& docSize,
                                    const RenderOptions& options);

    /// Caminho de composição: guarda o compósito acabado de construir.
    /// No-op no renderer base.
    virtual void storeComposite(const QImage& composite, const Document& doc,
                                const QSize& docSize,
                                const QTransform& docToDevice,
                                const RenderOptions& options);
};

/// Renderer com cache por camada no caminho rápido (documentos sem
/// máscaras de recorte nem ajustes visíveis). Só camadas cuja rasterização
/// é cara (texto, forma, vector, preenchimento procedural) entram no cache;
/// imagens têm cache próprio de máscara e ficam de fora.
///
/// Chave: (id da camada, revisão do documento, tamanho do canvas no
/// dispositivo, modo interactivo). A revisão é monotónica, portanto uma
/// edição no documento invalida tudo o que estiver velho — sem risco de
/// pixmap obsoleto. Orçamento: 16 entradas / 64 MB, com despejo LRU.
class CachingRenderer final : public SoftwareRenderer
{
public:
    void clearCaches() override;
    const char* name() const override { return "software+cache"; }

    void invalidateLayer(const QUuid& layerId) override;
    void invalidateAllLayers() override;

    bool compositeFromCache(QPainter* painter, const Document& doc,
                            const QTransform& docToDevice,
                            const QRectF& canvasOnDevice, const QSize& docSize,
                            const RenderOptions& options) override;
    void storeComposite(const QImage& composite, const Document& doc,
                        const QSize& docSize, const QTransform& docToDevice,
                        const RenderOptions& options) override;

    /// Estatísticas para diagnóstico (acertos/Total desde o arranque).
    int cacheHits() const { return m_hits; }
    int cacheMisses() const { return m_misses; }
    int compositeHits() const { return m_compositeHits; }
    int compositeMisses() const { return m_compositeMisses; }

    /// Chave do cache: camada + revisão do documento + tamanho do canvas no
    /// dispositivo + modo interactivo. Pública para o qHash poder usá-la.
    struct Key {
        QUuid layerId;
        quint64 revision = 0;
        quint64 fingerprint = 0;
        QSize device;
        bool interactive = false;
        double scale = 1.0;

        bool operator==(const Key& o) const
        {
            return layerId == o.layerId && revision == o.revision
                && fingerprint == o.fingerprint
                && device == o.device && interactive == o.interactive
                && qFuzzyCompare(scale, o.scale);
        }
    };

protected:
    bool drawCachedLayer(QPainter* painter, const Layer& layer,
                         const Document& doc, const QRectF& canvasRect,
                         const QTransform& docToDevice,
                         const QRectF& canvasOnDevice,
                         const RenderOptions& options) override;

private:
    struct Entry {
        QPixmap pixmap;
        qint64 bytes = 0;
    };

    void evictToFit();
    void dropLayer(const QUuid& layerId);

    QHash<Key, Entry> m_cache;
    QList<Key> m_lru;
    int m_hits = 0;
    int m_misses = 0;
    qint64 m_bytes = 0;

public:
    /// Cache do compósito (caminho de composição): uma imagem por
    /// (revisão, tamanho do documento, escala, modo interactivo).
    struct CompositeKey {
        // A identidade do documento é o id do grupo raiz: sem ela, dois
        // documentos no mesmo processo (testes!) com a mesma revisão e
        // tamanho colidiam na cache.
        QUuid docId;
        quint64 revision = 0;
        quint64 fingerprint = 0;
        QSize docSize;
        double scale = 1.0;
        bool interactive = false;

        bool operator==(const CompositeKey& o) const
        {
            return docId == o.docId && revision == o.revision
                && fingerprint == o.fingerprint && docSize == o.docSize
                && qFuzzyCompare(scale, o.scale) && interactive == o.interactive;
        }
    };

    QHash<CompositeKey, QImage> m_compositeCache;
    qint64 m_compositeBytes = 0;
    int m_compositeHits = 0;
    int m_compositeMisses = 0;

    void evictComposites();
};

/// Renderer usado pelo caminho livre (renderDocument e restantes pontos de
/// entrada). É uma instância única de processo: o cache sobrevive entre
/// chamadas, e `clearRendererCaches()` limpa-a.
IRenderer& defaultRenderer();

} // namespace cc
