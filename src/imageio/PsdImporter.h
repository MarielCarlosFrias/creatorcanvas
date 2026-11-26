#pragma once

#include <QByteArray>
#include <QImage>
#include <QString>

namespace cc {

/// Decodes the flattened composite image of a PSD file (8/16-bit, grayscale
/// or RGB/RGBA, raw or PackBits RLE). 16-bit samples are reduced to their
/// high byte. Layer data, CMYK, 32-bit and PSB are rejected — this is a safe
/// minimal reader, not a full PSD parser.
/// All reads are bounds-checked; |maxPixels| caps width*height.
/// Returns false with a human-readable |error| on any malformed input.
bool decodePsdComposite(const QByteArray& bytes, QImage* out, QString* error,
                        qint64 maxPixels);

} // namespace cc
