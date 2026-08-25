#include "NewDocumentSpec.h"

#include "core/Document.h"
#include "core/layers/Layer.h"

namespace cc {

std::unique_ptr<Document> createDocument(const NewDocumentSpec& spec)
{
    auto doc =
        std::make_unique<Document>(spec.width, spec.height, spec.dpi);

    if (!spec.transparentBackground) {
        auto background = std::make_unique<BackgroundLayer>();
        background->name = QStringLiteral("Background");
        background->fill = spec.backgroundColor;
        doc->addLayer(std::move(background));
    }

    return doc;
}

} // namespace cc
