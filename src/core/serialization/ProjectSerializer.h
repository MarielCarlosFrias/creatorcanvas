#pragma once

#include <QString>
#include <QByteArray>
#include <memory>
#include <QJsonObject>

namespace cc {

class Document;
class Layer;

class ProjectSerializer
{
public:
    static bool save(const Document& doc, const QString& filePath);
    static bool load(Document& doc, const QString& filePath);
    static QString lastError();

private:
    static QString m_error;
    static QJsonObject layerToJson(const Layer& layer);
    static std::unique_ptr<Layer> layerFromJson(const QJsonObject& obj, QString* error = nullptr);
};

} // namespace cc
