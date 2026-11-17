#include "commands/ImageCommand.hpp"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace minipaint::detail
{
namespace
{
std::vector<PixelRun> findChangedRuns(const sf::Image& before, const sf::Image& after)
{
    std::vector<PixelRun> runs;
    if (before.getSize() != after.getSize())
    {
        return runs;
    }

    const sf::Vector2u size = before.getSize();
    const std::uint8_t* beforePixels = before.getPixelsPtr();
    const std::uint8_t* afterPixels = after.getPixelsPtr();

    for (std::size_t y = 0; y < size.y; ++y)
    {
        std::size_t x = 0U;
        while (x < size.x)
        {
            const std::size_t offset = (y * static_cast<std::size_t>(size.x) + x) * 4U;
            const bool changed = beforePixels[offset] != afterPixels[offset] ||
                                 beforePixels[offset + 1U] != afterPixels[offset + 1U] ||
                                 beforePixels[offset + 2U] != afterPixels[offset + 2U] ||
                                 beforePixels[offset + 3U] != afterPixels[offset + 3U];
            if (!changed)
            {
                ++x;
                continue;
            }

            const std::size_t start = x;
            const std::uint32_t beforeColor = packColor(sf::Color(beforePixels[offset],
                                                                   beforePixels[offset + 1U],
                                                                   beforePixels[offset + 2U],
                                                                   beforePixels[offset + 3U]));
            const std::uint32_t afterColor = packColor(sf::Color(afterPixels[offset],
                                                                  afterPixels[offset + 1U],
                                                                  afterPixels[offset + 2U],
                                                                  afterPixels[offset + 3U]));

            ++x;
            while (x < size.x)
            {
                const std::size_t nextOffset = (y * static_cast<std::size_t>(size.x) + x) * 4U;
                const std::uint32_t nextBefore = packColor(sf::Color(beforePixels[nextOffset],
                                                                       beforePixels[nextOffset + 1U],
                                                                       beforePixels[nextOffset + 2U],
                                                                       beforePixels[nextOffset + 3U]));
                const std::uint32_t nextAfter = packColor(sf::Color(afterPixels[nextOffset],
                                                                      afterPixels[nextOffset + 1U],
                                                                      afterPixels[nextOffset + 2U],
                                                                      afterPixels[nextOffset + 3U]));
                const bool nextChanged = beforePixels[nextOffset] != afterPixels[nextOffset] ||
                                         beforePixels[nextOffset + 1U] != afterPixels[nextOffset + 1U] ||
                                         beforePixels[nextOffset + 2U] != afterPixels[nextOffset + 2U] ||
                                         beforePixels[nextOffset + 3U] != afterPixels[nextOffset + 3U];

                if (!nextChanged || nextBefore != beforeColor || nextAfter != afterColor)
                {
                    break;
                }

                ++x;
            }

            runs.push_back({static_cast<std::uint32_t>(y * static_cast<std::size_t>(size.x) + start),
                            static_cast<std::uint32_t>(x - start),
                            beforeColor,
                            afterColor});
        }
    }

    return runs;
}
} // namespace

std::unique_ptr<ImageCommand> ImageCommand::create(std::vector<Layer>& layers,
                                                    const std::size_t layerIndex,
                                                    const sf::Image& before,
                                                    const sf::Image& after)
{
    if (layerIndex >= layers.size())
    {
        return nullptr;
    }

    if (before.getSize() == after.getSize())
    {
        std::vector<PixelRun> runs = findChangedRuns(before, after);
        if (runs.empty())
        {
            return nullptr;
        }

        return std::make_unique<ImageCommand>(layers, layerIndex, std::move(runs),
                                              std::nullopt, std::nullopt);
    }

    // Size-changing edit (e.g. a 90-degree layer rotation): keep full images.
    return std::make_unique<ImageCommand>(layers, layerIndex, std::vector<PixelRun>{},
                                          before, after);
}

ImageCommand::ImageCommand(std::vector<Layer>& layers,
                           const std::size_t layerIndex,
                           std::vector<PixelRun> runs,
                           std::optional<sf::Image> beforeImage,
                           std::optional<sf::Image> afterImage)
    : m_layers(layers)
    , m_layerIndex(layerIndex)
    , m_runs(std::move(runs))
    , m_beforeImage(std::move(beforeImage))
    , m_afterImage(std::move(afterImage))
{
}

void ImageCommand::notifyLayerRemoved(const std::size_t removedIndex)
{
    if (m_disabled)
    {
        return;
    }

    if (m_layerIndex == removedIndex)
    {
        m_disabled = true; // the layer no longer exists
        return;
    }

    if (m_layerIndex > removedIndex)
    {
        --m_layerIndex; // the layer shifted down one slot
    }
}

bool ImageCommand::applyRuns(const std::vector<PixelRun>& runs, const bool useAfterColors)
{
    if (m_layerIndex >= m_layers.size())
    {
        return false; // the layer was removed; nothing to restore
    }

    sf::Image& image = m_layers[m_layerIndex].image;
    const sf::Vector2u size = image.getSize();
    if (size.x == 0U || size.y == 0U)
    {
        return false;
    }

    const std::uint32_t width = size.x;
    const std::uint32_t height = size.y;
    for (const PixelRun& run : runs)
    {
        const sf::Color color = unpackColor(useAfterColors ? run.afterColor : run.beforeColor);
        for (std::uint32_t offset = 0U; offset < run.length; ++offset)
        {
            const std::uint32_t index = run.startIndex + offset;
            const std::uint32_t x = index % width;
            const std::uint32_t y = index / width;
            if (x >= width || y >= height)
            {
                continue; // tolerate edits against a resized image
            }

            image.setPixel({x, y}, color);
        }
    }
    return true;
}

void ImageCommand::execute()
{
    if (m_disabled)
    {
        return;
    }

    if (m_afterImage.has_value())
    {
        if (m_layerIndex < m_layers.size())
        {
            m_layers[m_layerIndex].image = *m_afterImage;
        }
        return;
    }

    (void)applyRuns(m_runs, true);
}

void ImageCommand::undo()
{
    if (m_disabled)
    {
        return;
    }

    if (m_beforeImage.has_value())
    {
        if (m_layerIndex < m_layers.size())
        {
            m_layers[m_layerIndex].image = *m_beforeImage;
        }
        return;
    }

    (void)applyRuns(m_runs, false);
}
} // namespace minipaint::detail
