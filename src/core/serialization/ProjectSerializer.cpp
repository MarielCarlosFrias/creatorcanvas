#include "ProjectSerializer.h"
#include "Document.h"
#include "ZipArchive.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>
#include <QUuid>

namespace cc {

QString ProjectSerializer::m_error;

static const int CURRENT_VERSION = 1;

QJsonObject ProjectSerializer::layerToJson(const Layer& layer)
{
    QJsonObject obj;
    obj["id"] = layer.id().toString();
    obj["type"] = static_cast<int>(layer.type());
    obj["name"] = layer.name;
    obj["visible"] = layer.visible;
    obj["locked"] = layer.locked;
    obj["opacity"] = layer.opacity();
    obj["blendMode"] = static_cast<int>(layer.blendMode);
    obj["transformX"] = layer.transform.position.x();
    obj["transformY"] = layer.transform.position.y();
    obj["rotation"] = layer.transform.rotationDeg;
    obj["scaleX"] = layer.transform.scaleX;
    obj["scaleY"] = layer.transform.scaleY;
    obj["shearX"] = layer.transform.shearX;
    obj["shearY"] = layer.transform.shearY;

    switch (layer.type()) {
    case LayerType::Group: {
        const auto& g = static_cast<const GroupLayer&>(layer);
        QJsonArray children;
        for (const auto& child : g.children)
            children.append(layerToJson(*child));
        obj["children"] = children;
        break;
    }
    case LayerType::Image: {
        const auto& img = static_cast<const ImageLayer&>(layer);
        obj["assetId"] = img.assetId.toString();
        obj["naturalWidth"] = img.naturalWidth;
        obj["naturalHeight"] = img.naturalHeight;
        break;
    }
    case LayerType::Text: {
        const auto& txt = static_cast<const TextLayer&>(layer);
        obj["content"] = txt.content;
        obj["fontFamily"] = txt.fontFamily;
        obj["sizePt"] = txt.sizePt;
        obj["bold"] = txt.bold;
        obj["italic"] = txt.italic;
        obj["underline"] = txt.underline;
        obj["color"] = txt.color.name();
        obj["letterSpacingPx"] = txt.letterSpacingPx;
        obj["lineHeightMult"] = txt.lineHeightMult;
        obj["align"] = static_cast<int>(txt.align);
        obj["boxW"] = txt.box.width();
        obj["boxH"] = txt.box.height();

        QJsonObject effects;
        QJsonObject outline;
        outline["enabled"] = txt.effects.outline.enabled;
        outline["color"] = txt.effects.outline.color.name();
        outline["width"] = txt.effects.outline.width;
        effects["outline"] = outline;

        QJsonObject shadow;
        shadow["enabled"] = txt.effects.shadow.enabled;
        shadow["color"] = txt.effects.shadow.color.name();
        shadow["offsetX"] = txt.effects.shadow.offsetX;
        shadow["offsetY"] = txt.effects.shadow.offsetY;
        shadow["blur"] = txt.effects.shadow.blur;
        effects["shadow"] = shadow;

        QJsonObject gradient;
        gradient["enabled"] = txt.effects.gradient.enabled;
        gradient["type"] = txt.effects.gradient.type;
        gradient["startColor"] = txt.effects.gradient.startColor.name();
        gradient["endColor"] = txt.effects.gradient.endColor.name();
        gradient["angleDeg"] = txt.effects.gradient.angleDeg;
        effects["gradient"] = gradient;

        obj["effects"] = effects;
        break;
    }
    case LayerType::Shape: {
        const auto& sh = static_cast<const ShapeLayer&>(layer);
        obj["kind"] = static_cast<int>(sh.kind);
        obj["fill"] = sh.fill.name();
        obj["stroke"] = sh.stroke.name();
        obj["strokeWidth"] = sh.strokeWidth;
        obj["cornerRadius"] = sh.cornerRadius;
        QJsonArray pts;
        for (const QPointF& p : sh.points) {
            QJsonArray coord;
            coord.append(p.x());
            coord.append(p.y());
            pts.append(coord);
        }
        obj["points"] = pts;
        break;
    }
    case LayerType::Background: {
        const auto& bg = static_cast<const BackgroundLayer&>(layer);
        obj["fill"] = bg.fill.name();
        break;
    }
    }
    return obj;
}

std::unique_ptr<Layer> ProjectSerializer::layerFromJson(const QJsonObject& obj, QString* error)
{
    if (!obj.contains("type")) {
        if (error) *error = "Missing 'type' field";
        return nullptr;
    }
    int typeVal = obj["type"].toInt();
    if (typeVal < 0 || typeVal > 4) { // Group=0, Image=1, Text=2, Shape=3, Background=4
        if (error) *error = QString("Invalid layer type: %1").arg(typeVal);
        return nullptr;
    }
    LayerType type = static_cast<LayerType>(typeVal);
    auto layer = makeLayer(type);
    if (!layer) {
        if (error) *error = QString("Failed to create layer of type %1").arg(typeVal);
        return nullptr;
    }
    // Restaurar ID
    QString idStr = obj["id"].toString();
    if (!idStr.isEmpty()) {
        // Usar um truque: criar um LayerId a partir da string e atribuir via reinterpret_cast?
        // Não é seguro. Em vez disso, vamos usar uma abordagem que não quebra o encapsulamento:
        // O Layer não expõe setter de ID. Para manter a integridade, vamos sobrescrever
        // o m_id usando um ponteiro para a classe? Melhor: adicionar um construtor protegido.
        // Por simplicidade, vamos gerar um novo ID e avisar.
        // Isso significa que a referência de seleção pode se perder, mas é uma limitação aceitável
        // para a primeira versão.
        //qWarning() << "ID restoration not implemented; new ID generated for layer" << layer->name;
    }

    layer->name = obj["name"].toString();
    layer->visible = obj["visible"].toBool(true);
    layer->locked = obj["locked"].toBool(false);
    layer->setOpacity(obj["opacity"].toDouble(1.0));
    layer->blendMode = static_cast<BlendMode>(obj["blendMode"].toInt(0));
    layer->transform.position = QPointF(obj["transformX"].toDouble(0.0),
                                        obj["transformY"].toDouble(0.0));
    layer->transform.rotationDeg = obj["rotation"].toDouble(0.0);
    layer->transform.scaleX = obj["scaleX"].toDouble(1.0);
    layer->transform.scaleY = obj["scaleY"].toDouble(1.0);
    layer->transform.shearX = obj["shearX"].toDouble(0.0);
    layer->transform.shearY = obj["shearY"].toDouble(0.0);

    switch (type) {
    case LayerType::Group: {
        auto* g = static_cast<GroupLayer*>(layer.get());
        QJsonArray children = obj["children"].toArray();
        for (const QJsonValue& v : children) {
            QString childError;
            auto child = layerFromJson(v.toObject(), &childError);
            if (child) {
                g->children.push_back(std::move(child));
            } else if (error) {
                *error = childError;
                return nullptr;
            }
        }
        break;
    }
    case LayerType::Image: {
        auto* img = static_cast<ImageLayer*>(layer.get());
        img->assetId = QUuid(obj["assetId"].toString());
        img->naturalWidth = obj["naturalWidth"].toInt();
        img->naturalHeight = obj["naturalHeight"].toInt();
        break;
    }
    case LayerType::Text: {
        auto* txt = static_cast<TextLayer*>(layer.get());
        txt->content = obj["content"].toString();
        txt->fontFamily = obj["fontFamily"].toString("Sans Serif");
        txt->sizePt = obj["sizePt"].toDouble(48.0);
        txt->bold = obj["bold"].toBool(false);
        txt->italic = obj["italic"].toBool(false);
        txt->underline = obj["underline"].toBool(false);
        txt->color = QColor(obj["color"].toString());
        txt->letterSpacingPx = obj["letterSpacingPx"].toDouble(0.0);
        txt->lineHeightMult = obj["lineHeightMult"].toDouble(1.0);
        txt->align = static_cast<TextAlignment>(obj["align"].toInt(0));
        txt->box = QSizeF(obj["boxW"].toDouble(0.0), obj["boxH"].toDouble(0.0));

        const QJsonObject effects = obj["effects"].toObject();
        const QJsonObject outline = effects["outline"].toObject();
        txt->effects.outline.enabled = outline["enabled"].toBool(false);
        txt->effects.outline.color = QColor(outline["color"].toString("#000000"));
        txt->effects.outline.width = outline["width"].toDouble(4.0);

        const QJsonObject shadow = effects["shadow"].toObject();
        txt->effects.shadow.enabled = shadow["enabled"].toBool(false);
        txt->effects.shadow.color = QColor(shadow["color"].toString("#000000"));
        txt->effects.shadow.offsetX = shadow["offsetX"].toDouble(4.0);
        txt->effects.shadow.offsetY = shadow["offsetY"].toDouble(4.0);
        txt->effects.shadow.blur = shadow["blur"].toDouble(6.0);

        const QJsonObject gradient = effects["gradient"].toObject();
        txt->effects.gradient.enabled = gradient["enabled"].toBool(false);
        txt->effects.gradient.type = gradient["type"].toInt(0);
        txt->effects.gradient.startColor = QColor(gradient["startColor"].toString("#ff6b6b"));
        txt->effects.gradient.endColor = QColor(gradient["endColor"].toString("#4ecdc4"));
        txt->effects.gradient.angleDeg = gradient["angleDeg"].toDouble(0.0);
        break;
    }
    case LayerType::Shape: {
        auto* sh = static_cast<ShapeLayer*>(layer.get());
        sh->kind = static_cast<ShapeKind>(obj["kind"].toInt(0));
        sh->fill = QColor(obj["fill"].toString());
        sh->stroke = QColor(obj["stroke"].toString());
        sh->strokeWidth = obj["strokeWidth"].toDouble(0.0);
        sh->cornerRadius = obj["cornerRadius"].toDouble(0.0);
        QJsonArray pts = obj["points"].toArray();
        QPolygonF poly;
        for (const QJsonValue& v : pts) {
            QJsonArray coord = v.toArray();
            if (coord.size() >= 2)
                poly << QPointF(coord[0].toDouble(), coord[1].toDouble());
        }
        sh->points = poly;
        break;
    }
    case LayerType::Background: {
        auto* bg = static_cast<BackgroundLayer*>(layer.get());
        bg->fill = QColor(obj["fill"].toString());
        break;
    }
    }
    return layer;
}

bool ProjectSerializer::save(const Document& doc, const QString& filePath)
{
    m_error.clear();
    ZipArchive zip;
    if (!zip.openForWriting(filePath)) {
        m_error = QStringLiteral("Cannot create ZIP file");
        return false;
    }

    QJsonObject root;
    root["version"] = CURRENT_VERSION;
    root["width"] = doc.width();
    root["height"] = doc.height();
    root["dpi"] = doc.dpi();
    root["layers"] = layerToJson(*doc.rootGroup());

    QJsonDocument docJson(root);
    QByteArray data = docJson.toJson(QJsonDocument::Indented);
    if (!zip.addFile("project.json", data, true)) {
        m_error = QStringLiteral("Failed to add project.json");
        return false;
    }

    if (!zip.close()) {
        m_error = QStringLiteral("Failed to finalize ZIP");
        return false;
    }
    return true;
}

bool ProjectSerializer::load(Document& doc, const QString& filePath)
{
    m_error.clear();
    ZipArchive zip;
    if (!zip.openForReading(filePath)) {
        m_error = QStringLiteral("Cannot open ZIP file");
        return false;
    }
    if (!zip.hasFile("project.json")) {
        m_error = QStringLiteral("project.json not found");
        return false;
    }
    QByteArray jsonData = zip.readFile("project.json");
    if (jsonData.isEmpty()) {
        m_error = QStringLiteral("project.json is empty");
        return false;
    }
    zip.closeRead();

    QJsonParseError parseErr;
    QJsonDocument docJson = QJsonDocument::fromJson(jsonData, &parseErr);
    if (parseErr.error != QJsonParseError::NoError) {
        m_error = QStringLiteral("JSON parse error: %1").arg(parseErr.errorString());
        return false;
    }
    QJsonObject root = docJson.object();
    int version = root["version"].toInt(0);
    if (version != CURRENT_VERSION) {
        m_error = QStringLiteral("Unsupported version: %1 (current: %2)").arg(version).arg(CURRENT_VERSION);
        return false;
    }

    int width = root["width"].toInt(100);
    int height = root["height"].toInt(100);
    int dpi = root["dpi"].toInt(96);
    // Como o Document é criado com dimensões fixas, não podemos alterá-las facilmente.
    // Para simplificar, vamos criar um novo Document com as dimensões lidas,
    // mas a função load recebe um Document existente. Vamos sobrescrever o root.
    // Para isso, vamos usar um método setRootLayer que adicionamos.
    QJsonObject layersObj = root["layers"].toObject();
    QString err;
    auto newRoot = layerFromJson(layersObj, &err);
    if (!newRoot) {
        m_error = QStringLiteral("Failed to parse layers: %1").arg(err);
        return false;
    }
    if (newRoot->type() != LayerType::Group) {
        m_error = QStringLiteral("Root layer must be a group");
        return false;
    }

    // Infelizmente, não podemos alterar as dimensões do Document facilmente.
    // Para o M3, vamos ignorar as dimensões salvas e manter as atuais.
    // Isso será melhorado no M4.
    // Substituir a árvore:
    doc.setRootLayer(std::unique_ptr<GroupLayer>(static_cast<GroupLayer*>(newRoot.release())));
    return true;
}

QString ProjectSerializer::lastError()
{
    return m_error;
}

} // namespace cc
