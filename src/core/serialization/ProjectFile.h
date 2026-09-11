#pragma once

#include <QString>
#include <QImage>
#include <memory>

namespace cc {

class Document;

inline constexpr int kProjectFormatVersion = 1;

bool saveDocument(const Document& doc, const QString& filePath,
                  QString* errorMessage = nullptr,
                  const QImage* thumbnail = nullptr);

std::unique_ptr<Document> loadDocument(const QString& filePath,
                                       QString* errorMessage = nullptr);

QImage loadProjectThumbnail(const QString& filePath);

} // namespace cc
