#include "ImagePresets.h"

namespace cc {

bool ImagePresets::isPixelFilter(int presetIndex)
{
    return presetIndex >= 8 && presetIndex <= 10;
}

std::optional<ImageProcessing::PresetFilter> ImagePresets::getPixelFilter(int presetIndex)
{
    switch (presetIndex) {
    case 8: return ImageProcessing::PresetFilter::Invert;
    case 9: return ImageProcessing::PresetFilter::Posterize;
    case 10: return ImageProcessing::PresetFilter::Threshold;
    default: return std::nullopt;
    }
}

ImageSliderPreset ImagePresets::getSliderPreset(int presetIndex)
{
    ImageSliderPreset p;
    switch (presetIndex) {
    case 0: // P&B (Grayscale)
        p.saturation = -100;
        p.contrast = 10;
        break;
    case 1: // Sépia
        p.saturation = -50;
        p.temperature = 40;
        p.contrast = 5;
        break;
    case 2: // Vintage
        p.saturation = -25;
        p.temperature = 20;
        p.contrast = 15;
        break;
    case 3: // Alto Contraste
        p.contrast = 50;
        p.sharpen = 20;
        break;
    case 4: // Vivid Pop
        p.saturation = 45;
        p.contrast = 25;
        p.sharpen = 15;
        break;
    case 5: // Cyberpunk
        p.contrast = 35;
        p.saturation = 30;
        p.temperature = -40;
        p.sharpen = 10;
        break;
    case 6: // Golden Hour
        p.temperature = 45;
        p.saturation = 20;
        p.brightness = 10;
        p.contrast = 10;
        break;
    case 7: // Dramatic Noir
        p.saturation = -100;
        p.contrast = 60;
        p.brightness = -10;
        p.sharpen = 25;
        break;
    default:
        break;
    }
    return p;
}

QString ImagePresets::presetName(int presetIndex)
{
    switch (presetIndex) {
    case 0: return QStringLiteral("P&B");
    case 1: return QStringLiteral("Sépia");
    case 2: return QStringLiteral("Vintage");
    case 3: return QStringLiteral("Alto Contraste");
    case 4: return QStringLiteral("Vivid Pop");
    case 5: return QStringLiteral("Cyberpunk");
    case 6: return QStringLiteral("Golden Hour");
    case 7: return QStringLiteral("Dramatic Noir");
    case 8: return QStringLiteral("Invert");
    case 9: return QStringLiteral("Posterize");
    case 10: return QStringLiteral("Threshold");
    default: return QStringLiteral("Custom");
    }
}

} // namespace cc
