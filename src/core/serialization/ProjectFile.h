#pragma once

#include <QString>
#include <memory>

namespace cc {

class Document;

inline constexpr int kProjectFormatVersion = 1;

bool saveDocument(const Document& doc, const QString& filePath,
                  QString* errorMessage = nullptr);

std::unique_ptr<Document> loadDocument(const QString& filePath,
                                       QString* errorMessage = nullptr);

} // namespace cc
