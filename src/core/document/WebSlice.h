#pragma once

#include <QRectF>
#include <QString>
#include <QUuid>

namespace cc {

enum class SliceKind {
    Image,
    NoImage
};

/// Ported from PhotoCraft's WebSlice (crates/doc/src/slices.rs)
struct WebSlice
{
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString name;
    QRectF rect;
    SliceKind kind = SliceKind::Image;
    QString exportFormat = QStringLiteral("png"); // "png", "jpg", "webp"

    bool operator==(const WebSlice& o) const
    {
        return id == o.id && name == o.name && rect == o.rect && kind == o.kind && exportFormat == o.exportFormat;
    }
};

} // namespace cc
