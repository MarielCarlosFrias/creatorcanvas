#pragma once

#include <QString>
#include <optional>
#include "core/image/ImageProcessing.h"

namespace cc {

enum class PresetType {
    Grayscale = 0,
    Sepia = 1,
    Vintage = 2,
    HighContrast = 3,
    VividPop = 4,
    Cyberpunk = 5,
    GoldenHour = 6,
    DramaticNoir = 7,
    Invert = 8,
    Posterize = 9,
    Threshold = 10
};

struct ImageSliderPreset {
    int brightness = 0;
    int exposure = 0;
    int contrast = 0;
    int saturation = 0;
    int temperature = 0;
    int hue = 0;
    int blur = 0;
    int sharpen = 0;
};

class ImagePresets {
public:
    static bool isPixelFilter(int presetIndex);
    static std::optional<ImageProcessing::PresetFilter> getPixelFilter(int presetIndex);
    static ImageSliderPreset getSliderPreset(int presetIndex);
    static QString presetName(int presetIndex);
};

} // namespace cc
