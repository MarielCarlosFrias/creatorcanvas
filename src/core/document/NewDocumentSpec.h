#pragma once

#include <QColor>

#include <memory>

namespace cc {

class Document;

struct NewDocumentSpec
{
    int width = 1280;
    int height = 720;
    int dpi = 96;
    bool transparentBackground = false;
    QColor backgroundColor = Qt::white;
};

std::unique_ptr<Document> createDocument(const NewDocumentSpec& spec);

} // namespace cc
