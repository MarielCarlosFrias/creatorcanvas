#include "MiniPaint.hpp"

#include "Canvas.hpp"
#include "Layer.hpp"
#include "commands/ImageCommand.hpp"
#include "tools/Drawing.hpp"
#include "tools/ToolContext.hpp"

#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Window/Mouse.hpp>

#include <algorithm>
#include <cmath>
#include <deque>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

namespace minipaint
{
namespace
{
std::vector<sf::Color> defaultPalette()
{
    return {
        sf::Color::Black,
        sf::Color::White,
        sf::Color(128, 128, 128),
        sf::Color::Red,
        sf::Color(128, 0, 0),
        sf::Color(255, 165, 0),
        sf::Color::Yellow,
        sf::Color(128, 128, 0),
        sf::Color::Green,
        sf::Color(0, 255, 255),
        sf::Color::Blue,
        sf::Color(0, 0, 128),
        sf::Color::Magenta,
        sf::Color(128, 0, 128),
        sf::Color(139, 69, 19),
        sf::Color(64, 64, 64)
    };
}

sf::Vector2f clampToCanvas(const sf::Vector2f& point, const sf::Vector2u& size)
{
    return {
        std::clamp(point.x, 0.0F, static_cast<float>(size.x)),
        std::clamp(point.y, 0.0F, static_cast<float>(size.y))
    };
}

// Composite all visible layers and upload to the display canvas.
void compositeToCanvas(detail::Canvas& compositeCanvas,
                       const std::vector<Layer>& layers)
{
    const sf::Image composite = compositeLayers(layers);
    if (composite.getSize().x == 0U || composite.getSize().y == 0U)
    {
        return;
    }

    compositeCanvas.replaceImage(composite);
}

// Hot-path sync: push only the region the tool just painted instead of
// recompositing every layer and re-uploading the whole canvas on every mouse
// event. Blending is pixel-local, so recompositing just this rectangle yields
// exactly the pixels a full recomposite would produce. Falls back to the full
// path when the tool did not track a region (or its region escapes the canvas).
void syncDirtyRegion(detail::Canvas& compositeCanvas,
                     const std::vector<Layer>& layers,
                     ToolContext& context)
{
    if (!context.dirtyValid)
    {
        compositeToCanvas(compositeCanvas, layers);
        return;
    }

    const sf::Vector2u canvasSize = compositeCanvas.size();
    const int minX = std::max(0, context.dirtyMinX);
    const int minY = std::max(0, context.dirtyMinY);
    const int maxX = std::min(static_cast<int>(canvasSize.x) - 1, context.dirtyMaxX);
    const int maxY = std::min(static_cast<int>(canvasSize.y) - 1, context.dirtyMaxY);
    context.resetDirty();

    if (minX > maxX || minY > maxY)
    {
        return; // Painted entirely outside the canvas.
    }

    const sf::Vector2u origin{static_cast<unsigned int>(minX),
                              static_cast<unsigned int>(minY)};
    const sf::Vector2u regionSize{static_cast<unsigned int>(maxX - minX + 1),
                                  static_cast<unsigned int>(maxY - minY + 1)};

    sf::Image region;
    compositeLayersIntoRegion(region, layers, regionSize, origin);
    compositeCanvas.updateRegion(region, origin);
}
} // namespace

struct MiniPaint::Impl
{
    Impl(const unsigned int width, const unsigned int height)
        : compositeCanvas({width, height})
        , tool(makeTool(ToolType::Pencil))
        , palette(defaultPalette())
        , selectedPaletteIndex(0U)
        , undoLimit(DefaultUndoLimit)
        , viewport({0.0F, 0.0F},
                   {static_cast<float>(width), static_cast<float>(height)})
    {
        settings.primaryColor = palette.front();
        settings.secondaryColor = sf::Color::White;
        settings.backgroundColor = sf::Color::White;

        // Create the default background layer.
        layers.emplace_back("Background", sf::Vector2u{width, height},
                            sf::Color::White);
        activeLayerIndex = 0U;

        // Composite the initial state.
        compositeToCanvas(compositeCanvas, layers);
    }

    ToolContext makeContext(const sf::Mouse::Button button = sf::Mouse::Button::Left)
    {
        ToolContext context;

        if (activeLayerIndex < layers.size() && !layers[activeLayerIndex].locked)
        {
            context.targetImage = &layers[activeLayerIndex].image;
        }
        else
        {
            context.targetImage = nullptr;
        }

        // Tools read the composited canvas for color sampling; the active
        // layer image remains the write target for painting.
        context.image = &compositeCanvas.image();

        context.target = &compositeCanvas.target();
        context.settings = settings;

        if (button == sf::Mouse::Button::Right)
        {
            context.settings.primaryColor = settings.secondaryColor;
        }

        context.colorPicked = [this, button](const sf::Color color) {
            if (button == sf::Mouse::Button::Right)
            {
                settings.secondaryColor = color;
                return;
            }

            settings.primaryColor = color;
            if (!palette.empty())
            {
                palette[selectedPaletteIndex] = color;
            }
        };

        context.imageReplaced = [this](const sf::Image& image) {
            if (activeLayerIndex < layers.size())
            {
                layers[activeLayerIndex].image = image;
            }
        };

        return context;
    }

    void cancelActiveStroke()
    {
        if (!drawing)
        {
            return;
        }

        tool->cancel();
        if (activeLayerIndex < layers.size())
        {
            layers[activeLayerIndex].image = strokeBefore;
            compositeToCanvas(compositeCanvas, layers);
        }
        drawing = false;
        strokeBefore = sf::Image();
    }

    /// Resets the viewport to the full canvas unless the user customized it.
    void syncViewportToCanvas()
    {
        if (viewportCustomized)
        {
            return;
        }

        const sf::Vector2u size = compositeCanvas.size();
        viewport = {{0.0F, 0.0F},
                    {static_cast<float>(size.x),
                     static_cast<float>(size.y)}};
    }

    void pushCommand(const sf::Image& before, const sf::Image& after)
    {
        if (activeLayerIndex >= layers.size())
        {
            return;
        }

        auto command = detail::ImageCommand::create(
            layers, activeLayerIndex, before, after);
        if (!command)
        {
            return;
        }

        redoStack.clear();
        if (undoLimit == 0U)
        {
            return;
        }

        undoStack.push_back(std::move(command));
        while (undoStack.size() > undoLimit)
        {
            undoStack.pop_front();
        }
    }

    std::optional<sf::Vector2f> mapToCanvas(const sf::Vector2i& screenPosition,
                                            const sf::FloatRect& mappingViewport,
                                            const bool clampOutside) const
    {
        if (mappingViewport.size.x <= 0.0F || mappingViewport.size.y <= 0.0F)
        {
            return std::nullopt;
        }

        const sf::Vector2f screen(static_cast<float>(screenPosition.x),
                                  static_cast<float>(screenPosition.y));
        const sf::Vector2f local = {
            (screen.x - mappingViewport.position.x) *
                static_cast<float>(compositeCanvas.size().x) / mappingViewport.size.x,
            (screen.y - mappingViewport.position.y) *
                static_cast<float>(compositeCanvas.size().y) / mappingViewport.size.y
        };

        if (!clampOutside && !mappingViewport.contains(screen))
        {
            return std::nullopt;
        }

        return clampToCanvas(local, compositeCanvas.size());
    }

    void handleMousePressed(const sf::Vector2i& screenPosition,
                            const sf::FloatRect& mappingViewport,
                            const sf::Mouse::Button button)
    {
        const auto position = mapToCanvas(screenPosition, mappingViewport, false);
        if (!position)
        {
            return;
        }

        if (activeLayerIndex >= layers.size() || layers[activeLayerIndex].locked)
        {
            return;
        }

        cancelActiveStroke();

        strokeBefore = layers[activeLayerIndex].image;
        drawing = true;
        activeButton = button;

        // Draw on the layer image (fast pixel manipulation), then sync to display.
        ToolContext context = makeContext(button);
        context.targetImage = &layers[activeLayerIndex].image;
        context.image = &compositeCanvas.image();

        tool->onMousePressed(*position, context);

        // Sync layer to display canvas so the stroke is visible immediately.
        syncDirtyRegion(compositeCanvas, layers, context);
    }

    void handleMouseMoved(const sf::Vector2i& screenPosition,
                          const sf::FloatRect& mappingViewport)
    {
        if (!drawing)
        {
            return;
        }

        const auto position = mapToCanvas(screenPosition, mappingViewport, true);
        if (!position)
        {
            return;
        }

        ToolContext context = makeContext(activeButton);
        context.targetImage = &layers[activeLayerIndex].image;
        context.image = &compositeCanvas.image();

        // Feed the new position to the tool for EVERY tool type (shape tools
        // need it to update their anchor before we can render the preview).
        tool->onMouseMoved(*position, context);

        // Restore the base image before drawing new preview so it doesn't accumulate.
        if (tool->supportsPreview())
        {
            // The committed composite is already on the GPU. Restore it and
            // draw only the lightweight shape preview on top; uploading the
            // complete image for every mouse move caused visible lag.
            compositeCanvas.restoreCommittedTexture();
            tool->renderPreview(compositeCanvas.target(), context);
            compositeCanvas.target().display();
        }
        else
        {
            // Brush tools (pencil/eraser/airbrush): sync only the painted
            // region instead of recompositing every layer per event.
            syncDirtyRegion(compositeCanvas, layers, context);
        }
    }

    void handleMouseReleased(const sf::Vector2i& screenPosition,
                             const sf::FloatRect& mappingViewport,
                             const sf::Mouse::Button button)
    {
        if (!drawing || button != activeButton)
        {
            return;
        }

        const auto position = mapToCanvas(screenPosition, mappingViewport, true);
        if (!position)
        {
            cancelActiveStroke();
            return;
        }

        ToolContext context = makeContext(activeButton);
        context.targetImage = &layers[activeLayerIndex].image;
        context.image = &compositeCanvas.image();

        tool->onMouseMoved(*position, context);

        const bool toolReportedChange = tool->onMouseReleased(*position, context);
        tool->cancel();

        // Sync the layer to the display canvas. Shape tools may leave preview
        // pixels outside the final shape, so they take the full composite once
        // per stroke; brush tools sync only the painted region.
        if (tool->supportsPreview())
        {
            compositeToCanvas(compositeCanvas, layers);
            compositeCanvas.target().display();
        }
        else
        {
            syncDirtyRegion(compositeCanvas, layers, context);
        }

        const sf::Image after =
            (activeLayerIndex < layers.size()) ? layers[activeLayerIndex].image
                                               : sf::Image();
        if (toolReportedChange && after.getSize().x != 0U &&
            after.getSize().y != 0U)
        {
            pushCommand(strokeBefore, after);
        }

        drawing = false;
        strokeBefore = sf::Image();
    }

    detail::Canvas compositeCanvas;
    std::vector<Layer> layers;
    std::size_t activeLayerIndex{0U};
    std::unique_ptr<Tool> tool;
    ToolType toolType{ToolType::Pencil};
    ToolSettings settings;
    std::vector<sf::Color> palette;
    std::size_t selectedPaletteIndex{0U};
    std::deque<std::unique_ptr<Command>> undoStack;
    std::deque<std::unique_ptr<Command>> redoStack;
    std::size_t undoLimit{DefaultUndoLimit};
    sf::FloatRect viewport{};
    bool viewportCustomized{false};
    bool drawing{false};
    sf::Mouse::Button activeButton{sf::Mouse::Button::Left};
    sf::Image strokeBefore;
};
} // namespace minipaint

MiniPaint::MiniPaint(const unsigned int width, const unsigned int height)
    : m_impl(std::make_unique<Impl>(width, height))
{
}

MiniPaint::~MiniPaint() = default;

// --------------------------------------------------------------- Layers --

std::size_t MiniPaint::addLayer(std::string name, const sf::Color fillColor)
{
    if (m_impl->layers.empty())
    {
        m_impl->layers.emplace_back(std::move(name),
                                    m_impl->compositeCanvas.size(),
                                    fillColor);
        m_impl->activeLayerIndex = 0U;
    }
    else
    {
        const sf::Vector2u size = m_impl->compositeCanvas.size();
        m_impl->layers.emplace_back(std::move(name), size, fillColor);
        m_impl->activeLayerIndex = m_impl->layers.size() - 1U;
    }

    compositeToCanvas(m_impl->compositeCanvas, m_impl->layers);
    return m_impl->activeLayerIndex;
}

bool MiniPaint::removeLayer(std::size_t index)
{
    if (index >= m_impl->layers.size())
    {
        return false;
    }

    m_impl->cancelActiveStroke();
    m_impl->layers.erase(m_impl->layers.begin() +
                         static_cast<std::ptrdiff_t>(index));

    // Remap the stored layer index in every command so undo/redo keep
    // targeting the layer they were created for (or become no-ops).
    for (const auto& stack : {&m_impl->undoStack, &m_impl->redoStack})
    {
        for (const auto& command : *stack)
        {
            if (auto* imageCommand = dynamic_cast<detail::ImageCommand*>(command.get()))
            {
                imageCommand->notifyLayerRemoved(index);
            }
        }
    }

    if (m_impl->layers.empty())
    {
        m_impl->activeLayerIndex = 0U;
    }
    else if (m_impl->activeLayerIndex >= m_impl->layers.size())
    {
        m_impl->activeLayerIndex = m_impl->layers.size() - 1U;
    }

    compositeToCanvas(m_impl->compositeCanvas, m_impl->layers);
    return true;
}

std::size_t MiniPaint::getLayerCount() const noexcept
{
    return m_impl->layers.size();
}

std::size_t MiniPaint::getActiveLayerIndex() const noexcept
{
    return m_impl->activeLayerIndex;
}

bool MiniPaint::setActiveLayer(std::size_t index)
{
    if (index >= m_impl->layers.size())
    {
        return false;
    }

    m_impl->cancelActiveStroke();
    m_impl->activeLayerIndex = index;
    compositeToCanvas(m_impl->compositeCanvas, m_impl->layers);
    return true;
}

const std::string& MiniPaint::getLayerName(std::size_t index) const
{
    if (index >= m_impl->layers.size())
    {
        static const std::string empty;
        return empty;
    }

    return m_impl->layers[index].name;
}

bool MiniPaint::setLayerName(std::size_t index, std::string name)
{
    if (index >= m_impl->layers.size())
    {
        return false;
    }

    m_impl->layers[index].name = std::move(name);
    return true;
}

float MiniPaint::getLayerOpacity(std::size_t index) const
{
    if (index >= m_impl->layers.size())
    {
        return 0.0F;
    }

    return m_impl->layers[index].opacity;
}

void MiniPaint::setLayerOpacity(std::size_t index, const float opacity)
{
    if (index >= m_impl->layers.size())
    {
        return;
    }

    m_impl->layers[index].opacity = std::clamp(opacity, 0.0F, 1.0F);
    compositeToCanvas(m_impl->compositeCanvas, m_impl->layers);
}

bool MiniPaint::getLayerVisibility(std::size_t index) const
{
    if (index >= m_impl->layers.size())
    {
        return false;
    }

    return m_impl->layers[index].visible;
}

void MiniPaint::setLayerVisibility(std::size_t index, const bool visible)
{
    if (index >= m_impl->layers.size())
    {
        return;
    }

    m_impl->layers[index].visible = visible;
    compositeToCanvas(m_impl->compositeCanvas, m_impl->layers);
}

bool MiniPaint::getLayerLocked(std::size_t index) const
{
    if (index >= m_impl->layers.size())
    {
        return false;
    }

    return m_impl->layers[index].locked;
}

void MiniPaint::setLayerLocked(std::size_t index, const bool locked)
{
    if (index >= m_impl->layers.size())
    {
        return;
    }

    m_impl->layers[index].locked = locked;
}

const sf::Image& MiniPaint::getActiveLayerImage() const
{
    if (m_impl->activeLayerIndex < m_impl->layers.size())
    {
        return m_impl->layers[m_impl->activeLayerIndex].image;
    }

    // Return a reference to an empty static image - not ideal but avoids
    // dangling references. Callers should check getLayerCount() first.
    static const sf::Image empty;
    return empty;
}

sf::Vector2u MiniPaint::getSize() const noexcept
{
    return m_impl->compositeCanvas.size();
}

// ------------------------------------------------------------ Events -----

void MiniPaint::handleEvent(const sf::Event& event)
{
    handleEvent(event, m_impl->viewport);
}

void MiniPaint::handleEvent(const sf::Event& event, const sf::FloatRect& viewport)
{
    if (event.is<sf::Event::FocusLost>())
    {
        m_impl->cancelActiveStroke();
        return;
    }

    if (const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>())
    {
        if (pressed->button == sf::Mouse::Button::Left ||
            pressed->button == sf::Mouse::Button::Right)
        {
            m_impl->handleMousePressed(pressed->position, viewport,
                                       pressed->button);
        }
        return;
    }

    if (const auto* moved = event.getIf<sf::Event::MouseMoved>())
    {
        m_impl->handleMouseMoved(moved->position, viewport);
        return;
    }

    if (const auto* released = event.getIf<sf::Event::MouseButtonReleased>())
    {
        if (released->button == sf::Mouse::Button::Left ||
            released->button == sf::Mouse::Button::Right)
        {
            m_impl->handleMouseReleased(released->position, viewport,
                                        released->button);
        }
    }
}

// ------------------------------------------------------------ Display ---

void MiniPaint::draw(sf::RenderTarget& target) const
{
    const sf::Vector2u size = m_impl->compositeCanvas.size();
    draw(target, sf::FloatRect({0.0F, 0.0F},
                               {static_cast<float>(size.x),
                                static_cast<float>(size.y)}));
}

void MiniPaint::draw(sf::RenderTarget& target,
                     const sf::FloatRect& destination) const
{
    sf::Sprite sprite(m_impl->compositeCanvas.target().getTexture());
    const sf::Vector2u textureSize = sprite.getTexture().getSize();

    if (textureSize.x == 0U || textureSize.y == 0U)
    {
        return;
    }

    sprite.setPosition(destination.position);
    sprite.setScale({destination.size.x / static_cast<float>(textureSize.x),
                     destination.size.y / static_cast<float>(textureSize.y)});
    target.draw(sprite);
}

void MiniPaint::applyToTexture(sf::RenderTexture& target) const
{
    sf::Sprite sprite(m_impl->compositeCanvas.target().getTexture());
    target.draw(sprite);
}

void MiniPaint::applyToTexture(sf::RenderTarget& target) const
{
    sf::Sprite sprite(m_impl->compositeCanvas.target().getTexture());
    target.draw(sprite);
}

const sf::Texture& MiniPaint::getCanvasTexture() const
{
    return m_impl->compositeCanvas.target().getTexture();
}

sf::Image MiniPaint::getResult() const
{
    return compositeLayers(m_impl->layers);
}

// --------------------------------------------------------- Load / Save ---

bool MiniPaint::loadFromImage(const sf::Image& image)
{
    if (image.getSize().x == 0U || image.getSize().y == 0U)
    {
        return false;
    }

    m_impl->cancelActiveStroke();
    m_impl->layers.clear();

    const sf::Vector2u size = image.getSize();
    m_impl->layers.emplace_back("Background", size, sf::Color::White);
    m_impl->layers[0].image = image;
    m_impl->activeLayerIndex = 0U;

    m_impl->compositeCanvas.replaceImage(image);
    m_impl->undoStack.clear();
    m_impl->redoStack.clear();
    m_impl->syncViewportToCanvas();
    return true;
}

bool MiniPaint::loadFromFile(const std::filesystem::path& path)
{
    sf::Image image;
    if (!image.loadFromFile(path))
    {
        return false;
    }

    return loadFromImage(image);
}

bool MiniPaint::saveToFile(const std::filesystem::path& path) const
{
    return getResult().saveToFile(path);
}

// ------------------------------------------------------------ Edit -----

void MiniPaint::clear(const sf::Color backgroundColor)
{
    m_impl->cancelActiveStroke();

    if (m_impl->activeLayerIndex >= m_impl->layers.size() ||
        m_impl->layers[m_impl->activeLayerIndex].locked)
    {
        return;
    }

    const sf::Image before = m_impl->layers[m_impl->activeLayerIndex].image;
    sf::Image after(before.getSize(), backgroundColor);
    m_impl->layers[m_impl->activeLayerIndex].image = after;
    compositeToCanvas(m_impl->compositeCanvas, m_impl->layers);
    m_impl->pushCommand(before, after);
}

bool MiniPaint::flipActiveLayerHorizontal()
{
    m_impl->cancelActiveStroke();
    if (m_impl->activeLayerIndex >= m_impl->layers.size() ||
        m_impl->layers[m_impl->activeLayerIndex].locked)
    {
        return false;
    }

    const sf::Image before = m_impl->layers[m_impl->activeLayerIndex].image;
    const sf::Vector2u size = before.getSize();
    if (size.x == 0U || size.y == 0U)
    {
        return false;
    }
    sf::Image after;
    after.resize(size, sf::Color::Transparent);
    for (unsigned int y = 0; y < size.y; ++y)
    {
        for (unsigned int x = 0; x < size.x; ++x)
        {
            after.setPixel({x, y}, before.getPixel({size.x - 1U - x, y}));
        }
    }
    m_impl->layers[m_impl->activeLayerIndex].image = after;
    compositeToCanvas(m_impl->compositeCanvas, m_impl->layers);
    m_impl->pushCommand(before, after);
    return true;
}

bool MiniPaint::flipActiveLayerVertical()
{
    m_impl->cancelActiveStroke();
    if (m_impl->activeLayerIndex >= m_impl->layers.size() ||
        m_impl->layers[m_impl->activeLayerIndex].locked)
    {
        return false;
    }

    const sf::Image before = m_impl->layers[m_impl->activeLayerIndex].image;
    const sf::Vector2u size = before.getSize();
    if (size.x == 0U || size.y == 0U)
    {
        return false;
    }
    sf::Image after;
    after.resize(size, sf::Color::Transparent);
    for (unsigned int y = 0; y < size.y; ++y)
    {
        for (unsigned int x = 0; x < size.x; ++x)
        {
            after.setPixel({x, y}, before.getPixel({x, size.y - 1U - y}));
        }
    }
    m_impl->layers[m_impl->activeLayerIndex].image = after;
    compositeToCanvas(m_impl->compositeCanvas, m_impl->layers);
    m_impl->pushCommand(before, after);
    return true;
}

bool MiniPaint::rotateActiveLayer(const bool clockwise)
{
    m_impl->cancelActiveStroke();
    if (m_impl->activeLayerIndex >= m_impl->layers.size() ||
        m_impl->layers[m_impl->activeLayerIndex].locked)
    {
        return false;
    }

    const sf::Image before = m_impl->layers[m_impl->activeLayerIndex].image;
    const sf::Vector2u oldSize = before.getSize();
    if (oldSize.x == 0U || oldSize.y == 0U)
    {
        return false;
    }
    const sf::Vector2u newSize{oldSize.y, oldSize.x};
    sf::Image after;
    after.resize(newSize, sf::Color::Transparent);
    for (unsigned int y = 0; y < newSize.y; ++y)
    {
        for (unsigned int x = 0; x < newSize.x; ++x)
        {
            const sf::Vector2u source = clockwise
                ? sf::Vector2u{y, oldSize.y - 1U - x}
                : sf::Vector2u{oldSize.x - 1U - y, x};
            after.setPixel({x, y}, before.getPixel(source));
        }
    }
    m_impl->layers[m_impl->activeLayerIndex].image = after;
    compositeToCanvas(m_impl->compositeCanvas, m_impl->layers);
    m_impl->syncViewportToCanvas();
    m_impl->pushCommand(before, after);
    return true;
}

bool MiniPaint::resizeCanvas(const sf::Vector2u newSize,
                             const sf::Color fillColor)
{
    // Keep accidental window/input values from allocating an unsafe image.
    constexpr unsigned int maxDimension = 4096U;
    if (newSize.x == 0U || newSize.y == 0U ||
        newSize.x > maxDimension || newSize.y > maxDimension ||
        m_impl->layers.empty())
    {
        return false;
    }

    m_impl->cancelActiveStroke();
    for (Layer& layer : m_impl->layers)
    {
        const sf::Image oldImage = layer.image;
        sf::Image resized;
        resized.resize(newSize, fillColor);
        const sf::Vector2u oldSize = oldImage.getSize();
        const unsigned int copyWidth = std::min(oldSize.x, newSize.x);
        const unsigned int copyHeight = std::min(oldSize.y, newSize.y);
        for (unsigned int y = 0; y < copyHeight; ++y)
        {
            for (unsigned int x = 0; x < copyWidth; ++x)
                resized.setPixel({x, y}, oldImage.getPixel({x, y}));
        }
        layer.image = std::move(resized);
    }

    compositeToCanvas(m_impl->compositeCanvas, m_impl->layers);
    m_impl->undoStack.clear();
    m_impl->redoStack.clear();
    m_impl->syncViewportToCanvas();
    return true;
}

bool MiniPaint::drawText(const sf::Font& font, const std::string& value,
                         const sf::Vector2f position,
                         const unsigned int characterSize)
{
    m_impl->cancelActiveStroke();
    if (value.empty() || m_impl->activeLayerIndex >= m_impl->layers.size() ||
        m_impl->layers[m_impl->activeLayerIndex].locked)
    {
        return false;
    }

    const sf::Image before = m_impl->layers[m_impl->activeLayerIndex].image;
    const sf::Vector2u size = before.getSize();
    if (size.x == 0U || size.y == 0U)
    {
        return false;
    }

    sf::RenderTexture target(size);
    target.clear(sf::Color::Transparent);
    sf::Texture baseTexture(before);
    sf::Sprite base(baseTexture);
    target.draw(base);

    const sf::String utf8Text = sf::String::fromUtf8(value.begin(), value.end());
    sf::Text label(font, utf8Text, characterSize);
    label.setPosition(position);
    label.setFillColor(m_impl->settings.primaryColor);
    target.draw(label);
    target.display();

    const sf::Image after = target.getTexture().copyToImage();
    m_impl->layers[m_impl->activeLayerIndex].image = after;
    compositeToCanvas(m_impl->compositeCanvas, m_impl->layers);
    m_impl->pushCommand(before, after);
    return true;
}

// ----------------------------------------------------------- Tools -----

void MiniPaint::setTool(const minipaint::ToolType tool)
{
    m_impl->cancelActiveStroke();
    m_impl->tool = makeTool(tool);
    m_impl->toolType = tool;
}

minipaint::ToolType MiniPaint::getTool() const
{
    return m_impl->toolType;
}

// ------------------------------------------------------------ Colors ---

void MiniPaint::setPrimaryColor(const sf::Color color)
{
    m_impl->settings.primaryColor = color;
    if (!m_impl->palette.empty())
    {
        m_impl->palette[m_impl->selectedPaletteIndex] = color;
    }
}

sf::Color MiniPaint::getPrimaryColor() const
{
    return m_impl->settings.primaryColor;
}

void MiniPaint::setSecondaryColor(const sf::Color color)
{
    m_impl->settings.secondaryColor = color;
}

sf::Color MiniPaint::getSecondaryColor() const
{
    return m_impl->settings.secondaryColor;
}

void MiniPaint::setBackgroundColor(const sf::Color color)
{
    m_impl->settings.backgroundColor = color;
}

sf::Color MiniPaint::getBackgroundColor() const
{
    return m_impl->settings.backgroundColor;
}

// ---------------------------------------------------------- Brush ------

void MiniPaint::setBrushSize(const float diameter)
{
    m_impl->settings.brushSize = std::clamp(diameter, 1.0F, 128.0F);
}

float MiniPaint::getBrushSize() const
{
    return m_impl->settings.brushSize;
}

// -------------------------------------------------------- Shape fill ---

void MiniPaint::setFillShapes(const bool enabled)
{
    m_impl->settings.fillShapes = enabled;
}

bool MiniPaint::getFillShapes() const
{
    return m_impl->settings.fillShapes;
}

// ------------------------------------------------------- Bucket fill ---

void MiniPaint::setFillTolerance(const unsigned int tolerance)
{
    m_impl->settings.fillTolerance = std::min(255U, tolerance);
}

unsigned int MiniPaint::getFillTolerance() const
{
    return m_impl->settings.fillTolerance;
}

// ----------------------------------------------------------- Palette ---

void MiniPaint::setPalette(std::vector<sf::Color> colors)
{
    m_impl->palette = std::move(colors);
    if (m_impl->palette.empty())
    {
        m_impl->selectedPaletteIndex = 0U;
        return;
    }

    m_impl->selectedPaletteIndex =
        std::min(m_impl->selectedPaletteIndex, m_impl->palette.size() - 1U);
    m_impl->settings.primaryColor =
        m_impl->palette[m_impl->selectedPaletteIndex];
}

const std::vector<sf::Color>& MiniPaint::getPalette() const
{
    return m_impl->palette;
}

bool MiniPaint::selectPaletteColor(const std::size_t index)
{
    if (index >= m_impl->palette.size())
    {
        return false;
    }

    m_impl->selectedPaletteIndex = index;
    m_impl->settings.primaryColor = m_impl->palette[index];
    return true;
}

std::size_t MiniPaint::getSelectedPaletteIndex() const
{
    return m_impl->selectedPaletteIndex;
}

// ------------------------------------------------------- Undo / Redo ---

bool MiniPaint::undo()
{
    m_impl->cancelActiveStroke();
    if (m_impl->undoStack.empty())
    {
        return false;
    }

    std::unique_ptr<Command> command = std::move(m_impl->undoStack.back());
    m_impl->undoStack.pop_back();
    command->undo();
    m_impl->redoStack.push_back(std::move(command));

    // Recomposite all visible layers to the display canvas after undo.
    compositeToCanvas(m_impl->compositeCanvas, m_impl->layers);
    m_impl->syncViewportToCanvas();
    return true;
}

bool MiniPaint::redo()
{
    m_impl->cancelActiveStroke();
    if (m_impl->redoStack.empty() || m_impl->undoLimit == 0U)
    {
        return false;
    }

    std::unique_ptr<Command> command = std::move(m_impl->redoStack.back());
    m_impl->redoStack.pop_back();
    command->execute();
    m_impl->undoStack.push_back(std::move(command));
    while (m_impl->undoStack.size() > m_impl->undoLimit)
    {
        m_impl->undoStack.pop_front();
    }

    // Recomposite all visible layers to the display canvas after redo.
    compositeToCanvas(m_impl->compositeCanvas, m_impl->layers);
    m_impl->syncViewportToCanvas();
    return true;
}

bool MiniPaint::canUndo() const
{
    return !m_impl->undoStack.empty();
}

bool MiniPaint::canRedo() const
{
    return !m_impl->redoStack.empty();
}

void MiniPaint::setUndoLimit(const std::size_t limit)
{
    m_impl->undoLimit = limit;
    while (m_impl->undoStack.size() > m_impl->undoLimit)
    {
        m_impl->undoStack.pop_front();
    }
    while (m_impl->redoStack.size() > m_impl->undoLimit)
    {
        m_impl->redoStack.pop_front();
    }
}

std::size_t MiniPaint::getUndoLimit() const
{
    return m_impl->undoLimit;
}

void MiniPaint::clearHistory()
{
    m_impl->cancelActiveStroke();
    m_impl->undoStack.clear();
    m_impl->redoStack.clear();
}

// ----------------------------------------------------------- Viewport --

void MiniPaint::setViewport(const sf::FloatRect& viewport)
{
    if (viewport.size.x > 0.0F && viewport.size.y > 0.0F)
    {
        m_impl->viewport = viewport;
        m_impl->viewportCustomized = true;
    }
}

const sf::FloatRect& MiniPaint::getViewport() const
{
    return m_impl->viewport;
}

bool MiniPaint::isDrawing() const
{
    return m_impl->drawing;
}
