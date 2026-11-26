#include "Document.h"

#include "geometry/LayerTransform.h"

#include <algorithm>
#include <functional>
#include <cstddef>
#include <utility>
#include <QUuid>
#include <QVariant>
#include <QDateTime>
#include <QDataStream>
#include <QIODevice>
#include <QTransform>
#include <QtMath>

namespace cc {

namespace {

QImage selectionInLayerSpace(const Document& document, const Layer& layer,
                             const QImage& documentSelection)
{
    if (documentSelection.isNull())
        return {};
    const QSize layerSize = layer.contentBounds().size().toSize();
    if (layerSize.isEmpty())
        return {};
    const QTransform layerToDocument = layerToDocumentMatrix(document, layer);
    if (qFuzzyIsNull(layerToDocument.determinant()))
        return {};
    const QImage source = documentSelection.convertToFormat(QImage::Format_Alpha8);
    QImage result(layerSize, QImage::Format_Alpha8);
    result.fill(0);
    for (int y = 0; y < result.height(); ++y) {
        uchar* dst = result.scanLine(y);
        for (int x = 0; x < result.width(); ++x) {
            const QPointF documentPoint = layerToDocument.map(QPointF(x + 0.5, y + 0.5));
            const QPoint sourcePoint(qRound(documentPoint.x()), qRound(documentPoint.y()));
            if (source.rect().contains(sourcePoint))
                dst[x] = source.constScanLine(sourcePoint.y())[sourcePoint.x()];
        }
    }
    return result;
}

}

Document::Document(int width, int height, int dpi, QObject* parent)
    : QObject(parent)
    , m_width(qMax(1, width))
    , m_height(qMax(1, height))
    , m_dpi(qMax(1, dpi))
    , m_colorSpace(ColorSpace::sRGB)
{
    m_root = std::make_unique<GroupLayer>();
    m_root->name = QStringLiteral("Root");
}

Document::~Document() = default;

void Document::setColorSpace(ColorSpace space)
{
    if (m_colorSpace != space) {
        m_colorSpace = space;
        // A newly selected working space invalidates the profile imported
        // from the previous source; the exporter resolves the new one.
        m_iccProfile.clear();
        bumpRevision();
        emit structureChanged();
    }
}

void Document::setIccProfile(const QByteArray& profile)
{
    if (m_iccProfile == profile)
        return;
    m_iccProfile = profile;
    bumpRevision();
    emit structureChanged();
}

bool Document::setCanvasSize(int width, int height, const QPointF& layerOffset)
{
    const int w = qMax(1, width);
    const int h = qMax(1, height);
    if (w == m_width && h == m_height && layerOffset.isNull())
        return false;

    QTransform oldToNew;
    oldToNew.translate(layerOffset.x(), layerOffset.y());
    m_selection.transformDocumentMasks(QSize(w, h), oldToNew);

    for (Guide& guide : m_guides)
        guide.position += guide.horizontal ? layerOffset.y() : layerOffset.x();

    const QRectF canvasRect(0.0, 0.0, w, h);
    for (WebSlice& slice : m_webSlices) {
        slice.rect.translate(layerOffset);
        slice.rect = slice.rect.intersected(canvasRect);
    }
    m_webSlices.erase(
        std::remove_if(m_webSlices.begin(), m_webSlices.end(),
                       [](const WebSlice& slice) {
                           return slice.rect.isEmpty() || !slice.rect.isValid();
                       }),
        m_webSlices.end());

    m_width = w;
    m_height = h;
    // O offset aplica-se só às camadas do grupo raiz: as transforms dos
    // grupos propagam-se aos filhos, portanto deslocar a raiz desloca tudo.
    if (!layerOffset.isNull()) {
        for (auto& child : m_root->children)
            child->transform.position += layerOffset;
    }
    bumpRevision();
    emit canvasSizeChanged(m_width, m_height);
    emit structureChanged();
    return true;
}

bool Document::rotateCanvas(double degrees)
{
    double d = std::fmod(degrees, 360.0);
    if (d > 180.0)
        d -= 360.0;
    if (d < -180.0)
        d += 360.0;
    if (qFuzzyIsNull(d))
        return false;
    const double absDegrees = std::abs(d);
    if (!qFuzzyCompare(absDegrees, 90.0)
        && !qFuzzyCompare(absDegrees, 180.0))
        return false;

    const bool swapWH = qFuzzyCompare(absDegrees, 90.0);
    const int newW = swapWH ? m_height : m_width;
    const int newH = swapWH ? m_width : m_height;
    const QPointF oldCenter(m_width / 2.0, m_height / 2.0);
    const QPointF newCenter(newW / 2.0, newH / 2.0);
    const QPointF shift = newCenter - oldCenter;

    QTransform rot;
    rot.translate(oldCenter.x(), oldCenter.y());
    rot.rotate(d);
    rot.translate(-oldCenter.x(), -oldCenter.y());

    QTransform oldToNew;
    oldToNew.translate(newCenter.x(), newCenter.y());
    oldToNew.rotate(d);
    oldToNew.translate(-oldCenter.x(), -oldCenter.y());
    m_selection.transformDocumentMasks(QSize(newW, newH), oldToNew);

    if (swapWH) {
        const bool clockwise = d > 0.0;
        for (Guide& guide : m_guides) {
            if (guide.horizontal) {
                guide.horizontal = false;
                guide.position = clockwise ? m_height - guide.position
                                           : guide.position;
            } else {
                guide.horizontal = true;
                guide.position = clockwise ? guide.position
                                           : m_width - guide.position;
            }
        }
    } else {
        for (Guide& guide : m_guides) {
            guide.position = (guide.horizontal ? m_height : m_width)
                - guide.position;
        }
    }

    const QRectF canvasRect(0.0, 0.0, newW, newH);
    for (WebSlice& slice : m_webSlices) {
        slice.rect = oldToNew.mapRect(slice.rect).normalized().intersected(canvasRect);
    }
    m_webSlices.erase(
        std::remove_if(m_webSlices.begin(), m_webSlices.end(),
                       [](const WebSlice& slice) {
                           return slice.rect.isEmpty() || !slice.rect.isValid();
                       }),
        m_webSlices.end());

    // Só as camadas do grupo raiz: a rotação do grupo propaga-se aos filhos
    // (a matrix do pai compõe com a do filho), rodá-las também rodaria 2×.
    for (auto& child : m_root->children) {
        AffineTransform t = child->transform;
        t.position = rot.map(t.position) + shift;
        t.rotationDeg += d;
        child->transform = t;
    }

    m_width = newW;
    m_height = newH;
    bumpRevision();
    emit canvasSizeChanged(m_width, m_height);
    emit structureChanged();
    return true;
}

Layer* Document::findLayer(const LayerId& id) const
{
    if (m_root->id() == id)
        return m_root.get();
    return findRecursive(m_root.get(), id);
}

Layer* Document::findRecursive(GroupLayer* group, const LayerId& id) const
{
    for (const auto& child : group->children) {
        if (child->id() == id)
            return child.get();
        if (child->type() == LayerType::Group) {
            if (Layer* found =
                    findRecursive(static_cast<GroupLayer*>(child.get()), id))
                return found;
        }
    }
    return nullptr;
}

GroupLayer* Document::parentOf(const LayerId& id) const
{
    if (m_root->id() == id)
        return nullptr;
    return findParentRecursive(m_root.get(), id);
}

GroupLayer* Document::findParentRecursive(GroupLayer* group, const LayerId& id) const
{
    for (const auto& child : group->children) {
        if (child->id() == id)
            return group;
        if (child->type() == LayerType::Group) {
            if (GroupLayer* found = findParentRecursive(
                    static_cast<GroupLayer*>(child.get()), id))
                return found;
        }
    }
    return nullptr;
}

int Document::indexOf(const LayerId& id) const
{
    GroupLayer* parent = parentOf(id);
    if (!parent)
        return -1;
    for (std::size_t i = 0; i < parent->children.size(); ++i)
        if (parent->children[i]->id() == id)
            return static_cast<int>(i);
    return -1;
}

bool Document::addLayer(std::unique_ptr<Layer> layer, GroupLayer* parent, int index)
{
    if (!layer)
        return false;
    if (!parent)
        parent = m_root.get();
    if (findLayer(parent->id()) != parent)
        return false;

    auto& children = parent->children;
    if (index < 0)
        index = static_cast<int>(children.size());
    else
        index = qBound(0, index, static_cast<int>(children.size()));

    children.insert(children.begin() + index, std::move(layer));
    bumpRevision();
    emit structureChanged();
    return true;
}

std::unique_ptr<Layer> Document::detachLayer(const LayerId& id)
{
    GroupLayer* parent = parentOf(id);
    if (!parent)
        return nullptr;
    auto& children = parent->children;
    auto it = std::find_if(children.begin(), children.end(),
                           [&](const auto& l) { return l->id() == id; });
    if (it == children.end())
        return nullptr;
    auto layer = std::move(*it);
    children.erase(it);
    return layer;
}

std::unique_ptr<Layer> Document::takeLayer(const LayerId& id)
{
    auto layer = detachLayer(id);
    if (!layer)
        return nullptr;
    bumpRevision();
    emit structureChanged();
    return layer;
}

bool Document::removeLayer(const LayerId& id)
{
    if (effectiveLocks(id).all)
        return false;
    return takeLayer(id) != nullptr;
}

QSet<LayerId> Document::referencedAssetIds(bool includeTransient) const
{
    QSet<LayerId> ids;
    std::function<void(const Layer*)> walk = [&](const Layer* layer) {
        if (!layer)
            return;
        if (!includeTransient && layer->transient)
            return; // prévia de mockup: não persiste
        if (layer->type() == LayerType::Image)
            ids.insert(static_cast<const ImageLayer*>(layer)->assetId);
        if (layer->type() == LayerType::Group)
            for (const auto& child :
                 static_cast<const GroupLayer*>(layer)->children)
                walk(child.get());
    };
    walk(m_root.get());
    return ids;
}

int Document::purgeUnreferencedAssets(const QSet<LayerId>& extraKeepIds)
{
    const QSet<LayerId> referenced = referencedAssetIds(true);
    int removed = 0;
    const auto list = m_assets.assets();
    for (const auto& a : list) {
        if (!referenced.contains(a.id) && !extraKeepIds.contains(a.id)) {
            if (m_assets.remove(a.id))
                ++removed;
        }
    }
    return removed;
}

bool Document::reorderLayer(const LayerId& id, GroupLayer* newParent, int newIndex)
{
    Layer* layer = findLayer(id);
    if (!layer || id == m_root->id())
        return false;
    if (effectiveLocks(id).all)
        return false;
    if (!newParent)
        newParent = m_root.get();
    if (findLayer(newParent->id()) != newParent)
        return false;
    if (effectiveLocks(newParent->id()).all)
        return false;

    for (GroupLayer* p = newParent; p; p = parentOf(p->id())) {
        if (p == layer)
            return false;
    }

    auto moved = detachLayer(id);
    if (!moved)
        return false;

    auto& children = newParent->children;
    if (newIndex < 0)
        newIndex = static_cast<int>(children.size());
    else
        newIndex = qBound(0, newIndex, static_cast<int>(children.size()));
    children.insert(children.begin() + newIndex, std::move(moved));

    bumpRevision();
    emit structureChanged();
    return true;
}

LayerId Document::duplicateLayer(const LayerId& id)
{
    Layer* original = findLayer(id);
    if (!original || id == m_root->id())
        return {};

    GroupLayer* parent = parentOf(id);
    const int insertAt = indexOf(id) + 1;

    auto copy = original->deepCopy();
    copy->name = original->name + QStringLiteral(" copy");
    const LayerId newId = copy->id();

    if (!addLayer(std::move(copy), parent, insertAt))
        return {};
    return newId;
}

bool Document::setLayerName(const LayerId& id, QString name)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->name == name)
        return layer != nullptr;
    if (effectiveLocks(id).all)
        return false;
    layer->name = std::move(name);
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setLayerVisible(const LayerId& id, bool visible)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->visible == visible)
        return layer != nullptr;
    layer->visible = visible;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setLayerLocked(const LayerId& id, bool locked)
{
    Layer* layer = findLayer(id);
    if (!layer || (layer->locked == locked && layer->locks.all == locked))
        return layer != nullptr;
    layer->locked = locked;
    layer->locks.all = locked;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setGroupExpanded(const LayerId& id, bool expanded)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Group)
        return false;
    auto* group = static_cast<GroupLayer*>(layer);
    if (group->isExpanded == expanded)
        return true;
    group->isExpanded = expanded;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setLayerColorTag(const LayerId& id, LayerColorTag tag)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->colorTag == tag)
        return layer != nullptr;
    if (effectiveLocks(id).all)
        return false;
    layer->colorTag = tag;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setLayerOpacity(const LayerId& id, float opacity)
{
    Layer* layer = findLayer(id);
    if (!layer)
        return false;
    if (effectiveLocks(id).all)
        return false;
    const float old = layer->opacity();
    layer->setOpacity(opacity);
    if (layer->opacity() == old)
        return true;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setLayerBlendMode(const LayerId& id, BlendMode mode)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->blendMode == mode)
        return layer != nullptr;
    if (effectiveLocks(id).all)
        return false;
    layer->blendMode = mode;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setLayerBlendIf(const LayerId& id, BlendIf blendIf)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->blendIf == blendIf)
        return layer != nullptr;
    if (effectiveLocks(id).all)
        return false;
    layer->blendIf = std::move(blendIf);
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setLayerFillOpacity(const LayerId& id, float fillOpacity)
{
    Layer* layer = findLayer(id);
    if (!layer)
        return false;
    if (effectiveLocks(id).all)
        return false;
    const float old = layer->fillOpacity();
    layer->setFillOpacity(fillOpacity);
    if (qFuzzyCompare(layer->fillOpacity(), old))
        return true;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setLayerClipped(const LayerId& id, bool clipped)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->clipped == clipped)
        return layer != nullptr;
    if (effectiveLocks(id).all)
        return false;

    // Uma camada de ajuste é recortada pela base no renderizador, mas
    // recortá-la isolada não tem efeito: o ajuste já é aplicado ao
    // acumulado. Mantém a coerência com o painel de camadas.
    if (clipped && layer->type() == LayerType::Adjustment)
        return false;

    // Recortar exige uma camada BASE abaixo: a primeira não-recortada e
    // visível do mesmo grupo. Sem base, o recorte não teria efeito.
    if (clipped) {
        GroupLayer* parent = parentOf(id);
        if (!parent)
            return false;
        const int index = indexOf(id);
        bool hasBase = false;
        for (int i = index - 1; i >= 0; --i) {
            const Layer* candidate = parent->children[static_cast<std::size_t>(i)].get();
            if (!candidate->visible || candidate->clipped)
                continue;
            hasBase = true;
            break;
        }
        if (!hasBase)
            return false;
    }

    layer->clipped = clipped;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setLayerLinkGroup(const LayerId& id, std::optional<qint64> linkGroup)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->linkGroup == linkGroup)
        return layer != nullptr;
    if (effectiveLocks(id).all)
        return false;
    layer->linkGroup = linkGroup;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setVectorLayerNodes(const LayerId& id, std::vector<VectorNode> nodes)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Vector)
        return false;
    if (effectiveLocks(id).all)
        return false;
    auto* vec = static_cast<VectorLayer*>(layer);
    if (vec->nodes == nodes)
        return true;
    vec->nodes = std::move(nodes);
    // O renderer desenha `path`: sincronizar aqui faz a edição aparecer.
    vec->rebuildPath();
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setTextPathLayerId(const LayerId& textId, std::optional<LayerId> pathLayerId)
{
    Layer* textLayer = findLayer(textId);
    if (!textLayer || textLayer->type() != LayerType::Text)
        return false;
    if (effectiveLocks(textId).all)
        return false;
    // O caminho tem de ser uma camada vectorial existente.
    if (pathLayerId) {
        const Layer* pathLayer = findLayer(*pathLayerId);
        if (!pathLayer || pathLayer->type() != LayerType::Vector || *pathLayerId == textId)
            return false;
    }
    auto* text = static_cast<TextLayer*>(textLayer);
    if (text->pathLayerId == pathLayerId)
        return true;
    text->pathLayerId = pathLayerId;
    bumpRevision();
    emit layerPropertyChanged(textId);
    return true;
}

bool Document::setTextPathOffset(const LayerId& textId, double offsetPct)
{
    Layer* layer = findLayer(textId);
    if (!layer || layer->type() != LayerType::Text)
        return false;
    if (effectiveLocks(textId).all)
        return false;
    auto* text = static_cast<TextLayer*>(layer);
    const double clamped = std::clamp(offsetPct, 0.0, 1.0);
    if (qFuzzyCompare(text->pathOffsetPct, clamped))
        return true;
    text->pathOffsetPct = clamped;
    bumpRevision();
    emit layerPropertyChanged(textId);
    return true;
}

bool Document::setAdjustmentKind(const LayerId& id, AdjustmentType kind)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Adjustment)
        return false;
    if (effectiveLocks(id).all)
        return false;
    auto* adj = static_cast<AdjustmentLayer*>(layer);
    if (adj->kind == kind)
        return true;
    adj->kind = kind;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setAdjustmentParameters(
    const LayerId& id, std::vector<std::pair<QString, double>> parameters)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Adjustment)
        return false;
    if (effectiveLocks(id).all)
        return false;
    auto* adj = static_cast<AdjustmentLayer*>(layer);
    if (adj->parameters == parameters)
        return true;
    adj->parameters = std::move(parameters);
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setLayerVectorMask(const LayerId& id, QImage mask)
{
    Layer* layer = findLayer(id);
    if (!layer)
        return false;
    if (effectiveLocks(id).all)
        return false;
    static_cast<void>(layer); // mask stored on specific layer types; ImageLayer/TextLayer/ShapeLayer handle it
    // For now, vector mask is stored on the base Layer class directly.
    layer->vectorMask = std::move(mask);
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setLayerAdjustmentLayerId(const LayerId& imageLayerId,
                                         const std::optional<LayerId>& adjustmentLayerId)
{
    Layer* layer = findLayer(imageLayerId);
    if (!layer || layer->type() != LayerType::Image)
        return false;
    if (effectiveLocks(imageLayerId).all)
        return false;
    auto* image = static_cast<ImageLayer*>(layer);
    if (image->adjustmentLayerId == adjustmentLayerId)
        return true;
    image->adjustmentLayerId = adjustmentLayerId;
    bumpRevision();
    emit layerPropertyChanged(imageLayerId);
    return true;
}

LayerId Document::createAdjustmentLayer(AdjustmentType type,
                                        const std::optional<LayerId>& sourceLayerId)
{
    auto adj = std::make_unique<AdjustmentLayer>();
    adj->kind = type;
    adj->sourceLayerId = sourceLayerId;
    // A seleção ativa torna-se uma máscara persistente do ajuste. Assim,
    // criar Brilho/Contraste, Curvas etc. não "esquece" a área que o
    // utilizador tinha selecionado, e o efeito continua não destrutivo.
    if (!m_selection.image().isNull()) {
        if (sourceLayerId) {
            if (const Layer* source = findLayer(*sourceLayerId))
                adj->vectorMask = selectionInLayerSpace(*this, *source, m_selection.image());
        } else {
            adj->vectorMask = m_selection.image().convertToFormat(QImage::Format_Alpha8);
        }
    }
    const LayerId newId = adj->id();
    if (!addLayer(std::move(adj), nullptr, -1))
        return {};
    bumpRevision();
    emit structureChanged();
    return newId;
}

AdjustmentLayer* Document::findAdjustmentLayer(const LayerId& id) const
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Adjustment)
        return nullptr;
    return static_cast<AdjustmentLayer*>(layer);
}

LayerLocks Document::effectiveLocks(const LayerId& id) const
{
    LayerLocks result;
    for (Layer* layer = findLayer(id); layer; ) {
        if (layer->locked)
            result.all = true; // legacy single lock actives everything
        const LayerLocks& l = layer->locks;
        result.transparency = result.transparency || l.transparency;
        result.pixels = result.pixels || l.pixels;
        result.position = result.position || l.position;
        result.all = result.all || l.all;
        if (layer->id() == m_root->id())
            break;
        layer = parentOf(layer->id());
        // parentOf pode retornar nullptr (raiz): findLayer é estável.
        if (!layer)
            break;
    }
    return result;
}

bool Document::setLayerLocks(const LayerId& id, LayerLocks locks)
{
    Layer* layer = findLayer(id);
    if (!layer || (layer->locks == locks && layer->locked == locks.all))
        return layer != nullptr;
    layer->locks = locks;
    layer->locked = locks.all;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setLayerTransform(const LayerId& id, const AffineTransform& transform)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->transform == transform)
        return layer != nullptr;
    const auto locks = effectiveLocks(id);
    if (locks.position || locks.all)
        return false;

    // Guardar os bounds dos grupos pais ANTES da mudança. O centro desses
    // bounds entra na matriz do grupo; se o filho se deslocar e o centro se
    // mover, a matriz do grupo não pode mudar com ele ou o movimento seria
    // cancelado (a imagem ficava parada enquanto o handle se deslocava).
    std::vector<std::pair<GroupLayer*, QRectF>> ancestors;
    for (GroupLayer* parent = parentOf(id);
         parent && parent != m_root.get();
         parent = parentOf(parent->id()))
        ancestors.emplace_back(parent, parent->contentBounds());

    layer->transform = transform;

    // Mantém a matriz de cada grupo constante: compensa apenas a variação do
    // centro pela parte linear do transform do grupo (rotação/escala/shear).
    std::vector<LayerId> changedAncestors;
    for (const auto& [parent, oldBounds] : ancestors) {
        const AffineTransform oldParentTransform = parent->transform;
        const QRectF newBounds = parent->contentBounds();
        if (oldBounds.isEmpty() && !newBounds.isEmpty()) {
            parent->transform.position = newBounds.center();
        } else if (!oldBounds.isEmpty() && !newBounds.isEmpty()) {
            QTransform linear;
            linear.rotate(parent->transform.rotationDeg);
            linear.shear(parent->transform.shearX, parent->transform.shearY);
            linear.scale(parent->transform.scaleX, parent->transform.scaleY);
            parent->transform.position += linear.map(newBounds.center() - oldBounds.center());
        }
        if (parent->id() != id && parent->transform != oldParentTransform)
            changedAncestors.push_back(parent->id());
    }

    bumpRevision();
    emit layerPropertyChanged(id);
    for (const LayerId& ancestorId : changedAncestors)
        emit layerPropertyChanged(ancestorId);
    return true;
}

bool Document::setLayerTextBox(const LayerId& id, const QSizeF& box)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Text)
        return false;
    const auto locks = effectiveLocks(id);
    if (locks.position || locks.all)
        return false;
    auto* text = static_cast<TextLayer*>(layer);
    if (text->box == box)
        return true;
    text->box = box;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setLayerTextEffects(const LayerId& id, const TextEffects& effects)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Text)
        return false;
    const auto locks = effectiveLocks(id);
    if (locks.pixels || locks.all)
        return false;
    auto* text = static_cast<TextLayer*>(layer);
    if (text->effects == effects)
        return true;
    text->effects = effects;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setLayerImageEffects(const LayerId& id, const ImageEffects& effects)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Image)
        return false;
    const auto locks = effectiveLocks(id);
    if (locks.pixels || locks.all)
        return false;
    auto* img = static_cast<ImageLayer*>(layer);
    if (img->effects == effects)
        return true;
    img->effects = effects;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setLayerTextContent(const LayerId& id, QString content)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Text)
        return false;
    const auto locks = effectiveLocks(id);
    if (locks.pixels || locks.all)
        return false;
    auto* text = static_cast<TextLayer*>(layer);
    if (text->content == content)
        return true;
    text->content = std::move(content);
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setLayerFontFamily(const LayerId& id, QString family)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Text)
        return false;
    const auto locks = effectiveLocks(id);
    if (locks.pixels || locks.all)
        return false;
    auto* text = static_cast<TextLayer*>(layer);
    if (text->fontFamily == family)
        return true;
    text->fontFamily = std::move(family);
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setLayerTextSize(const LayerId& id, double sizePt)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Text)
        return false;
    const auto locks = effectiveLocks(id);
    if (locks.pixels || locks.all)
        return false;
    auto* text = static_cast<TextLayer*>(layer);
    if (qFuzzyCompare(text->sizePt, sizePt))
        return true;
    text->sizePt = sizePt;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setLayerTextBold(const LayerId& id, bool bold)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Text)
        return false;
    const auto locks = effectiveLocks(id);
    if (locks.pixels || locks.all)
        return false;
    auto* text = static_cast<TextLayer*>(layer);
    if (text->bold == bold)
        return true;
    text->bold = bold;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setLayerTextItalic(const LayerId& id, bool italic)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Text)
        return false;
    const auto locks = effectiveLocks(id);
    if (locks.pixels || locks.all)
        return false;
    auto* text = static_cast<TextLayer*>(layer);
    if (text->italic == italic)
        return true;
    text->italic = italic;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setLayerTextUnderline(const LayerId& id, bool underline)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Text)
        return false;
    const auto locks = effectiveLocks(id);
    if (locks.pixels || locks.all)
        return false;
    auto* text = static_cast<TextLayer*>(layer);
    if (text->underline == underline)
        return true;
    text->underline = underline;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setLayerTextColor(const LayerId& id, QColor color)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Text)
        return false;
    const auto locks = effectiveLocks(id);
    if (locks.pixels || locks.all)
        return false;
    auto* text = static_cast<TextLayer*>(layer);
    if (text->color == color)
        return true;
    text->color = std::move(color);
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setLayerTextAlign(const LayerId& id, TextAlignment align)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Text)
        return false;
    const auto locks = effectiveLocks(id);
    if (locks.pixels || locks.all)
        return false;
    auto* text = static_cast<TextLayer*>(layer);
    if (text->align == align)
        return true;
    text->align = align;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setLayerLetterSpacing(const LayerId& id, double letterSpacingPx)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Text)
        return false;
    const auto locks = effectiveLocks(id);
    if (locks.pixels || locks.all)
        return false;
    auto* text = static_cast<TextLayer*>(layer);
    if (qFuzzyCompare(text->letterSpacingPx, letterSpacingPx))
        return true;
    text->letterSpacingPx = letterSpacingPx;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setLayerLineHeight(const LayerId& id, double lineHeightMult)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Text)
        return false;
    const auto locks = effectiveLocks(id);
    if (locks.pixels || locks.all)
        return false;
    auto* text = static_cast<TextLayer*>(layer);
    if (qFuzzyCompare(text->lineHeightMult, lineHeightMult))
        return true;
    text->lineHeightMult = lineHeightMult;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

void Document::touchLayer(const LayerId& id)
{
    if (findLayer(id)) {
        bumpRevision();
        emit layerPropertyChanged(id);
    }
}

// Altera a cor de preenchimento da forma geométrica
bool Document::setShapeFill(const LayerId& id, QColor fill)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Shape)
        return false;
    const auto locks = effectiveLocks(id);
    if (locks.pixels || locks.all)
        return false;
    auto* shape = static_cast<ShapeLayer*>(layer);
    if (shape->fill == fill)
        return true; // Sem alteração, evita notificações redundantes
    shape->fill = std::move(fill);
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}
bool Document::setShapeGradient(const LayerId& id, Gradient gradient)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Shape)
        return false;
    const auto locks = effectiveLocks(id);
    if (locks.pixels || locks.all)
        return false;
    static_cast<ShapeLayer*>(layer)->gradient = std::move(gradient);
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setShapeStarPoints(const LayerId& id, int points)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Shape)
        return false;
    const auto locks = effectiveLocks(id);
    if (locks.pixels || locks.all)
        return false;
    auto* shape = static_cast<ShapeLayer*>(layer);
    const int next = qBound(3, points, 16);
    if (shape->starPoints == next)
        return true;
    shape->starPoints = next;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setShapeStarInnerRatio(const LayerId& id, double ratio)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Shape)
        return false;
    const auto locks = effectiveLocks(id);
    if (locks.pixels || locks.all)
        return false;
    auto* shape = static_cast<ShapeLayer*>(layer);
    const double next = qBound(0.1, ratio, 0.9);
    if (shape->starInnerRatio == next)
        return true;
    shape->starInnerRatio = next;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setShapeStrokeDashRatio(const LayerId& id, double ratio)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Shape)
        return false;
    const auto locks = effectiveLocks(id);
    if (locks.pixels || locks.all)
        return false;
    auto* shape = static_cast<ShapeLayer*>(layer);
    const double next = qBound(0.0, ratio, 10.0);
    if (shape->strokeDashRatio == next)
        return true;
    shape->strokeDashRatio = next;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setShapeShadow(const LayerId& id, ShapeShadow shadow)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Shape)
        return false;
    const auto locks = effectiveLocks(id);
    if (locks.pixels || locks.all)
        return false;
    auto* shape = static_cast<ShapeLayer*>(layer);
    if (shape->shadow == shadow)
        return true;
    shape->shadow = std::move(shadow);
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setBackgroundGradient(const LayerId& id, Gradient gradient)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Background)
        return false;
    const auto locks = effectiveLocks(id);
    if (locks.pixels || locks.all)
        return false;
    static_cast<BackgroundLayer*>(layer)->gradient = std::move(gradient);
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

// Altera a cor do contorno (stroke) da forma geométrica
bool Document::setShapeStroke(const LayerId& id, QColor stroke)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Shape)
        return false;
    const auto locks = effectiveLocks(id);
    if (locks.pixels || locks.all)
        return false;
    auto* shape = static_cast<ShapeLayer*>(layer);
    if (shape->stroke == stroke)
        return true;
    shape->stroke = std::move(stroke);
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

// Altera a espessura do contorno da forma geométrica (clamped em >= 0.0)
bool Document::setShapeStrokeWidth(const LayerId& id, double width)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Shape)
        return false;
    const auto locks = effectiveLocks(id);
    if (locks.pixels || locks.all)
        return false;
    auto* shape = static_cast<ShapeLayer*>(layer);
    const double clamped = std::max(0.0, width);
    if (qFuzzyCompare(shape->strokeWidth, clamped))
        return true;
    shape->strokeWidth = clamped;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

// Altera o raio de arredondamento dos cantos (para RoundedRect, clamped em >= 0.0)
bool Document::setShapeCornerRadius(const LayerId& id, double radius)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Shape)
        return false;
    const auto locks = effectiveLocks(id);
    if (locks.pixels || locks.all)
        return false;
    auto* shape = static_cast<ShapeLayer*>(layer);
    const double clamped = std::max(0.0, radius);
    if (qFuzzyCompare(shape->cornerRadius, clamped))
        return true;
    shape->cornerRadius = clamped;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setShapeKind(const LayerId& id, ShapeKind kind)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Shape)
        return false;
    const auto locks = effectiveLocks(id);
    if (locks.pixels || locks.all)
        return false;
    auto* shape = static_cast<ShapeLayer*>(layer);
    if (shape->kind == kind)
        return true;
    shape->kind = kind;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

// Atualiza o asset associado a uma camada de imagem (ImageLayer), com dimensões naturais
bool Document::setImageLayerAsset(const LayerId& id, const LayerId& newAssetId, int naturalWidth, int naturalHeight)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Image)
        return false;
    const auto locks = effectiveLocks(id);
    if (locks.pixels || locks.all)
        return false;
    auto* img = static_cast<ImageLayer*>(layer);
    img->assetId = newAssetId;
    img->naturalWidth = naturalWidth;
    img->naturalHeight = naturalHeight;
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::setLayerAlphaMask(const LayerId& id, QImage mask)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Image)
        return false;
    const auto locks = effectiveLocks(id);
    if (locks.pixels || locks.all)
        return false;
    static_cast<ImageLayer*>(layer)->alphaMask = std::move(mask);
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::invertLayerAlphaMask(const LayerId& id)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Image)
        return false;
    const auto locks = effectiveLocks(id);
    if (locks.pixels || locks.all)
        return false;
    auto* img = static_cast<ImageLayer*>(layer);
    if (img->alphaMask.isNull())
        return false;

    QImage inverted = img->alphaMask.convertToFormat(QImage::Format_Alpha8);
    inverted.invertPixels();
    img->alphaMask = std::move(inverted);
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

bool Document::removeLayerAlphaMask(const LayerId& id)
{
    Layer* layer = findLayer(id);
    if (!layer || layer->type() != LayerType::Image)
        return false;
    const auto locks = effectiveLocks(id);
    if (locks.pixels || locks.all)
        return false;
    auto* img = static_cast<ImageLayer*>(layer);
    if (img->alphaMask.isNull())
        return false;

    img->alphaMask = QImage();
    bumpRevision();
    emit layerPropertyChanged(id);
    return true;
}

SelectionSpace Document::inferredSelectionSpace(const QImage& mask) const
{
    return !mask.isNull() && mask.size() != QSize(m_width, m_height)
        ? SelectionSpace::Layer
        : SelectionSpace::Document;
}

void Document::setPixelSelection(QImage selection)
{
    const SelectionSpace space = inferredSelectionSpace(selection);
    setPixelSelection(std::move(selection), space);
}

void Document::setPixelSelection(QImage selection, SelectionSpace space)
{
    m_selection.setImage(std::move(selection), space);
    bumpRevision();
    emit structureChanged();
}

void Document::clearPixelSelection()
{
    if (m_selection.image().isNull())
        return;
    m_selection.clear();
    bumpRevision();
    emit structureChanged();
}

QUuid Document::addSavedSelection(const QString& name, const QImage& mask)
{
    return addSavedSelection(name, mask, inferredSelectionSpace(mask));
}

QUuid Document::addSavedSelection(const QString& name, const QImage& mask,
                                  SelectionSpace space)
{
    const QUuid id = m_selection.addSavedSelection(name, mask, space);
    bumpRevision();
    return id;
}

QUuid Document::restoreSavedSelection(const QUuid& id, const QString& name,
                                      const QImage& mask)
{
    return restoreSavedSelection(id, name, mask, inferredSelectionSpace(mask));
}

QUuid Document::restoreSavedSelection(const QUuid& id, const QString& name,
                                      const QImage& mask, SelectionSpace space)
{
    const QUuid newId = m_selection.addSavedSelection(id, name, mask, space);
    bumpRevision();
    return newId;
}

bool Document::removeSavedSelection(const QUuid& id)
{
    if (!m_selection.removeSavedSelection(id))
        return false;
    bumpRevision();
    emit structureChanged();
    return true;
}

void Document::clearSavedSelections()
{
    if (m_selection.savedSelections().empty())
        return;
    m_selection.clearSavedSelections();
    bumpRevision();
    emit structureChanged();
}

void Document::restoreSelectionSnapshot(const SelectionState& snapshot)
{
    m_selection = snapshot;
    bumpRevision();
    emit structureChanged();
}

void Document::addLayerComp(LayerComp comp)
{
    m_layerComps.push_back(std::move(comp));
    bumpRevision();
}

bool Document::applyLayerComp(const QString& compId)
{
    auto it = std::find_if(m_layerComps.begin(), m_layerComps.end(),
                           [&](const LayerComp& c) { return c.id == compId; });
    if (it == m_layerComps.end())
        return false;

    const LayerComp& comp = *it;
    for (const auto& state : comp.states) {
        Layer* layer = findLayer(state.layerId);
        if (!layer)
            continue;

        if (comp.applyVisibility && state.visible.has_value())
            layer->visible = *state.visible;

        if (comp.applyPosition && state.position.has_value())
            layer->transform.position = *state.position;

        if (comp.applyAppearance) {
            if (state.opacity.has_value())
                layer->setOpacity(*state.opacity);
            if (state.blendMode.has_value())
                layer->blendMode = *state.blendMode;
        }

        emit layerPropertyChanged(state.layerId);
    }

    bumpRevision();
    emit structureChanged();
    return true;
}

bool Document::removeLayerComp(const QString& compId)
{
    auto it = std::find_if(m_layerComps.begin(), m_layerComps.end(),
                           [&](const LayerComp& c) { return c.id == compId; });
    if (it == m_layerComps.end())
        return false;

    m_layerComps.erase(it);
    bumpRevision();
    return true;
}

void Document::addWebSlice(WebSlice slice)
{
    m_webSlices.push_back(std::move(slice));
    bumpRevision();
    emit structureChanged();
}

bool Document::removeWebSlice(const QString& sliceId)
{
    auto it = std::find_if(m_webSlices.begin(), m_webSlices.end(),
                           [&](const WebSlice& s) { return s.id == sliceId; });
    if (it == m_webSlices.end())
        return false;

    m_webSlices.erase(it);
    bumpRevision();
    emit structureChanged();
    return true;
}

void Document::clearWebSlices()
{
    if (m_webSlices.empty())
        return;
    m_webSlices.clear();
    bumpRevision();
    emit structureChanged();
}

void Document::setWebSlices(std::vector<WebSlice> slices)
{
    m_webSlices = std::move(slices);
    bumpRevision();
    emit structureChanged();
}

void Document::addGuide(Guide guide)
{
    m_guides.push_back(guide);
    bumpRevision();
    emit structureChanged();
}

void Document::clearGuides()
{
    if (m_guides.empty())
        return;
    m_guides.clear();
    bumpRevision();
    emit structureChanged();
}

void Document::setGuides(std::vector<Guide> guides)
{
    m_guides = std::move(guides);
    bumpRevision();
    emit structureChanged();
}

QUuid Document::createDocumentVersion(const QString& name, const QString& description)
{
    DocumentVersion version;
    version.id = QUuid::createUuid();
    version.name = name.isEmpty() ? QStringLiteral("Version %1").arg(m_documentVersions.size() + 1) : name;
    version.description = description;
    version.createdAt = QDateTime::currentDateTime();
    version.revision = m_revision;

    // Serialize document state to QByteArray snapshot
    QByteArray snapshotData;
    QDataStream stream(&snapshotData, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);

    // Serialize document metadata
    stream << m_width << m_height << m_dpi << static_cast<int>(m_colorSpace);

    // Serialize layer tree recursively

    // Serialize pixel selection
    stream << m_selection.image();

    // Serialize layer comps

    // Serialize web slices

    version.snapshot = snapshotData;
    m_documentVersions.add(std::move(version));
    return m_documentVersions.back().id;
}

std::vector<Document::DocumentVersion> Document::documentVersions() const
{
    return m_documentVersions.all();
}

bool Document::restoreDocumentVersion(const QUuid& versionId)
{
    DocumentVersion* version = m_documentVersions.find(versionId);
    if (!version)
        return false;

    // Restore document metadata from snapshot
    QDataStream stream(&version->snapshot, QIODevice::ReadOnly);
    stream.setVersion(QDataStream::Qt_6_0);

    int w, h, dpi, csInt;
    stream >> w >> h >> dpi >> csInt;

    QImage pixelSelection;
    stream >> pixelSelection;

    std::vector<LayerComp> layerComps;
    
    std::vector<WebSlice> webSlices;
    
    // Apply metadata changes
    if (m_width != w || m_height != h || m_dpi != dpi || static_cast<int>(m_colorSpace) != csInt) {
        m_width = w;
        m_height = h;
        m_dpi = dpi;
        m_colorSpace = static_cast<ColorSpace>(csInt);
        bumpRevision();
        emit structureChanged();
    }

    const SelectionSpace selectionSpace = inferredSelectionSpace(pixelSelection);
    m_selection.setImage(std::move(pixelSelection), selectionSpace);
    m_layerComps = layerComps;
    m_webSlices = webSlices;

    bumpRevision();
    emit structureChanged();
    return true;
}

bool Document::removeDocumentVersion(const QUuid& versionId)
{
    return m_documentVersions.remove(versionId);
}

void Document::clearDocumentVersions()
{
    m_documentVersions.clear();
}

} // namespace cc
