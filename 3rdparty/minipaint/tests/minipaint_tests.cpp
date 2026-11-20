#include "MiniPaint.hpp"
#include "Tool.hpp"

#include <SFML/Window/Event.hpp>
#include <SFML/Window/Mouse.hpp>
#include <SFML/Window/VideoMode.hpp>
#include <SFML/Window/Window.hpp>

#include <cmath>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace
{
int failures = 0;

void check(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

sf::Event mousePressed(const int x, const int y)
{
    return sf::Event(sf::Event::MouseButtonPressed{sf::Mouse::Button::Left, {x, y}});
}

sf::Event mouseMoved(const int x, const int y)
{
    return sf::Event(sf::Event::MouseMoved{{x, y}});
}

sf::Event mouseReleased(const int x, const int y)
{
    return sf::Event(sf::Event::MouseButtonReleased{sf::Mouse::Button::Left, {x, y}});
}

sf::Event rightMousePressed(const int x, const int y)
{
    return sf::Event(sf::Event::MouseButtonPressed{sf::Mouse::Button::Right, {x, y}});
}

sf::Event rightMouseReleased(const int x, const int y)
{
    return sf::Event(sf::Event::MouseButtonReleased{sf::Mouse::Button::Right, {x, y}});
}
} // namespace

namespace
{
// Composites only a rectangle must produce exactly the pixels of the full
// composite restricted to that rectangle (blend math is pixel-local) — the
// invariant the incremental canvas sync during strokes relies on.
void testRegionCompositingMatchesFull()
{
    std::uint32_t rng = 12345U;
    auto nextByte = [&rng]() {
        rng = rng * 1664525U + 1013904223U;
        return static_cast<std::uint8_t>((rng >> 16) & 0xFFU);
    };

    for (int trial = 0; trial < 50; ++trial)
    {
        std::vector<minipaint::Layer> layers;
        const sf::Vector2u canvasSize{
            40U + static_cast<unsigned int>(nextByte() % 60U),
            40U + static_cast<unsigned int>(nextByte() % 60U)};
        const int layerCount = 1 + (nextByte() % 4);

        for (int i = 0; i < layerCount; ++i)
        {
            minipaint::Layer layer("l", canvasSize, sf::Color::Transparent);
            for (unsigned int y = 0; y < canvasSize.y; ++y)
            {
                for (unsigned int x = 0; x < canvasSize.x; ++x)
                {
                    const bool edge = (x < 2) || (y < 2);
                    const auto a = edge ? std::uint8_t{0} : static_cast<std::uint8_t>(nextByte());
                    layer.image.setPixel({x, y}, sf::Color(nextByte(), nextByte(),
                                                          nextByte(), a));
                }
            }
            switch (nextByte() % 6)
            {
                case 1: layer.blendMode = sf::BlendAdd; break;
                case 2: layer.blendMode = sf::BlendMultiply; break;
                case 3: layer.blendMode = sf::BlendMin; break;
                case 4: layer.blendMode = sf::BlendMax; break;
                case 5: layer.blendMode = sf::BlendNone; break;
                default: break;
            }
            layer.visible = (nextByte() < 60) ? false : true;
            layer.opacity = static_cast<float>(nextByte() % 4) * 0.25F;
            layers.push_back(layer);
        }

        const sf::Image full = minipaint::compositeLayers(layers);
        if (full.getSize().x == 0U || full.getSize().y == 0U)
        {
            continue; // all layers invisible
        }

        const unsigned int w = 1U + static_cast<unsigned int>(nextByte()) % full.getSize().x;
        const unsigned int h = 1U + static_cast<unsigned int>(nextByte()) % full.getSize().y;
        const unsigned int ox = static_cast<unsigned int>(nextByte()) % (full.getSize().x - w + 1U);
        const unsigned int oy = static_cast<unsigned int>(nextByte()) % (full.getSize().y - h + 1U);

        sf::Image region;
        minipaint::compositeLayersIntoRegion(region, layers, {w, h}, {ox, oy});

        bool matches = region.getSize() == sf::Vector2u(w, h);
        for (unsigned int y = 0; matches && y < h; ++y)
        {
            for (unsigned int x = 0; matches && x < w; ++x)
            {
                if (region.getPixel({x, y}) != full.getPixel({ox + x, oy + y}))
                {
                    matches = false;
                }
            }
        }
        check(matches, "region composite is pixel-identical to the full composite");
    }
}

// The composited canvas (what the user sees) must match a fresh composite of
// the layer stack after an incremental stroke sync.
void testCanvasMatchesLayersAfterStroke()
{
    minipaint::MiniPaint paint(80U, 80U);
    paint.addLayer("overlay", sf::Color::Transparent);
    paint.setTool(minipaint::ToolType::Pencil);
    paint.setPrimaryColor(sf::Color::Black);
    paint.setBrushSize(7.0F);

    const sf::FloatRect viewport({0.0F, 0.0F}, {80.0F, 80.0F});
    paint.handleEvent(mousePressed(10, 10), viewport);
    for (int i = 1; i <= 20; ++i)
    {
        paint.handleEvent(mouseMoved(10 + i * 3, 10 + i), viewport);
    }
    paint.handleEvent(mouseReleased(70, 30), viewport);

    const sf::Image expected = paint.getResult();
    const sf::Image actual = paint.getCanvasTexture().copyToImage();
    check(actual.getSize() == expected.getSize(), "canvas texture has the document size");

    int mismatches = 0;
    for (unsigned int y = 0; y < expected.getSize().y && mismatches == 0; ++y)
    {
        for (unsigned int x = 0; x < expected.getSize().x; ++x)
        {
            if (actual.getPixel({x, y}) != expected.getPixel({x, y}))
            {
                ++mismatches;
                break;
            }
        }
    }
    check(mismatches == 0, "canvas after an incremental stroke matches compositeLayers()");
}
} // namespace

int main()
{
#if !defined(_WIN32) && !defined(__APPLE__)
    // SFML only fails (aborts) later when a window needs a context that does
    // not exist, so the absence of a display is detected up front: CI runners
    // without X/Wayland skip instead of crashing.
    const char* display = std::getenv("DISPLAY");
    const char* wayland = std::getenv("WAYLAND_DISPLAY");
    const bool hasEnv = (display != nullptr && *display != '\0') ||
                        (wayland != nullptr && *wayland != '\0');
    if (!hasEnv)
    {
        std::cout << "SKIP: MiniPaint smoke tests need a display "
                     "(DISPLAY/WAYLAND_DISPLAY unset)\n";
        return 125; // CTest SKIP_RETURN_CODE
    }
#endif

    // A hidden window supplies the OpenGL context required by sf::RenderTexture.
    std::unique_ptr<sf::Window> context;
    try
    {
        context = std::make_unique<sf::Window>(sf::VideoMode({64U, 64U}),
                                               "MiniPaint test context", sf::Style::None);
    }
    catch (const std::exception&)
    {
        std::cout << "SKIP: MiniPaint smoke tests need an OpenGL context (window creation failed)\n";
        return 125; // CTest SKIP_RETURN_CODE
    }

    try
    {

        testRegionCompositingMatchesFull();
        testCanvasMatchesLayersAfterStroke();

        minipaint::MiniPaint paint(64U, 64U);
        check(paint.getSize() == sf::Vector2u(64U, 64U), "canvas has the requested dimensions");

        sf::Image redImage({64U, 64U}, sf::Color::Red);
        check(paint.loadFromImage(redImage), "loadFromImage accepts a valid image");
        check(paint.getResult().getPixel({10U, 10U}) == sf::Color::Red, "loaded pixels are preserved");

        paint.setPrimaryColor(sf::Color::Black);
        paint.setBrushSize(3.0F);
        const sf::Event press = mousePressed(10, 10);
        const sf::Event move = mouseMoved(20, 10);
        const sf::Event release = mouseReleased(20, 10);
        paint.handleEvent(press);
        paint.handleEvent(move);
        paint.handleEvent(release);

        check(paint.getResult().getPixel({10U, 10U}) == sf::Color::Black, "pencil changes the canvas");
        check(paint.canUndo(), "pencil stroke is undoable");
        check(paint.undo(), "undo succeeds");
        check(paint.getResult().getPixel({10U, 10U}) == sf::Color::Red, "undo restores previous pixels");
        check(paint.redo(), "redo succeeds");
        check(paint.getResult().getPixel({10U, 10U}) == sf::Color::Black, "redo reapplies the stroke");

        paint.setTool(minipaint::ToolType::BucketFill);
        paint.setPrimaryColor(sf::Color::Blue);
        const sf::Event fillPress = mousePressed(40, 40);
        const sf::Event fillRelease = mouseReleased(40, 40);
        paint.handleEvent(fillPress);
        paint.handleEvent(fillRelease);
        check(paint.getResult().getPixel({40U, 40U}) == sf::Color::Blue, "bucket fill replaces contiguous pixels");

        paint.setTool(minipaint::ToolType::Eyedropper);
        const sf::Event pick = mousePressed(40, 40);
        paint.handleEvent(pick);
        check(paint.getPrimaryColor() == sf::Color::Blue, "eyedropper selects the sampled color");

        paint.clear(sf::Color::White);
        check(paint.getResult().getPixel({40U, 40U}) == sf::Color::White, "clear changes the canvas");
        check(paint.undo(), "clear is undoable");

        const std::filesystem::path output = std::filesystem::temp_directory_path() / "minipaint-smoke.png";
        check(paint.saveToFile(output), "saveToFile writes a PNG");
        check(std::filesystem::exists(output), "saved PNG exists");
        check(paint.loadFromFile(output), "loadFromFile reads the PNG");
        std::filesystem::remove(output);

        const std::vector<sf::Color> palette = {sf::Color::Red, sf::Color::Green, sf::Color::Blue};
        paint.setPalette(palette);
        check(paint.getPrimaryColor() == sf::Color::Red,
              "replacing the palette synchronizes the selected primary color");
        check(paint.selectPaletteColor(2U), "valid palette entry is selected");
        check(paint.getPrimaryColor() == sf::Color::Blue, "palette selection updates primary color");
        check(!paint.selectPaletteColor(99U), "invalid palette entry is rejected");

        paint.setBrushSize(1000.0F);
        check(paint.getBrushSize() == 128.0F, "brush size is clamped");
        paint.setBrushSize(0.0F);
        check(paint.getBrushSize() == 1.0F, "minimum brush size is enforced");

        minipaint::MiniPaint shapes(32U, 32U);
        shapes.loadFromImage(sf::Image({32U, 32U}, sf::Color::White));
        shapes.setTool(minipaint::ToolType::Rectangle);
        shapes.setPrimaryColor(sf::Color::Black);
        shapes.setBrushSize(1.0F);
        shapes.setFillShapes(false);
        shapes.handleEvent(mousePressed(4, 4));
        shapes.handleEvent(mouseMoved(20, 12));
        check(shapes.getCanvasTexture().copyToImage().getPixel({4U, 4U}) == sf::Color::Black,
              "shape preview is visible before release");
        shapes.handleEvent(mouseReleased(20, 12));
        check(shapes.getResult().getPixel({4U, 4U}) == sf::Color::Black,
              "released shape is committed to the canvas");
        check(shapes.undo(), "shape command is undoable");
        check(shapes.getResult().getPixel({4U, 4U}) == sf::Color::White,
              "undo restores the shape's original pixels");

        minipaint::MiniPaint eraser(16U, 16U);
        eraser.loadFromImage(sf::Image({16U, 16U}, sf::Color::Red));
        eraser.setTool(minipaint::ToolType::Eraser);
        eraser.setBackgroundColor(sf::Color::White);
        eraser.setBrushSize(3.0F);
        eraser.handleEvent(mousePressed(8, 8));
        eraser.handleEvent(mouseReleased(8, 8));
        check(eraser.getResult().getPixel({8U, 8U}) == sf::Color::White,
              "eraser paints with the background color");

        sf::RenderTexture outputTarget({16U, 16U});
        eraser.applyToTexture(outputTarget);
        outputTarget.display();
        check(outputTarget.getTexture().copyToImage().getPixel({8U, 8U}) == sf::Color::White,
              "applyToTexture copies the current canvas");

        minipaint::MiniPaint resized(4U, 4U);
        check(resized.loadFromImage(sf::Image({12U, 20U}, sf::Color::Green)),
              "loading an image with different dimensions succeeds");
        check(resized.getSize() == sf::Vector2u(12U, 20U), "loading an image resizes the canvas");

        const sf::Color translucent(17U, 91U, 203U, 73U);
        minipaint::MiniPaint alphaCanvas(4U, 4U);
        alphaCanvas.loadFromImage(sf::Image({4U, 4U}, translucent));
        check(alphaCanvas.getCanvasTexture().copyToImage().getPixel({0U, 0U}) == translucent,
              "semi-transparent image pixels are preserved exactly");
        alphaCanvas.setPrimaryColor(sf::Color::Black);
        alphaCanvas.setBrushSize(1.0F);
        alphaCanvas.handleEvent(mousePressed(0, 0));
        alphaCanvas.handleEvent(mouseReleased(0, 0));
        check(alphaCanvas.getResult().getPixel({3U, 3U}) == translucent,
              "committed strokes preserve untouched translucent pixels");

        minipaint::MiniPaint onePixelBrush(8U, 8U);
        onePixelBrush.loadFromImage(sf::Image({8U, 8U}, sf::Color::White));
        onePixelBrush.setPrimaryColor(sf::Color::Black);
        onePixelBrush.setBrushSize(1.0F);
        onePixelBrush.handleEvent(mousePressed(3, 3));
        onePixelBrush.handleEvent(mouseReleased(3, 3));
        check(onePixelBrush.getResult().getPixel({3U, 3U}) == sf::Color::Black,
              "a one-pixel brush changes at least one canvas pixel");
        check(onePixelBrush.canUndo(), "a one-pixel brush stroke creates an undo command");

        minipaint::MiniPaint flatEllipse(16U, 16U);
        flatEllipse.loadFromImage(sf::Image({16U, 16U}, sf::Color::White));
        flatEllipse.setTool(minipaint::ToolType::Ellipse);
        flatEllipse.setPrimaryColor(sf::Color::Black);
        flatEllipse.setBrushSize(1.0F);
        flatEllipse.handleEvent(mousePressed(4, 8));
        flatEllipse.handleEvent(mouseMoved(12, 8));
        flatEllipse.handleEvent(mouseReleased(12, 8));
        check(flatEllipse.getResult().getPixel({4U, 8U}) == sf::Color::Black,
              "a horizontally flattened ellipse remains visible");

        minipaint::MiniPaint ellipseOutline(200U, 120U);
        ellipseOutline.loadFromImage(sf::Image({200U, 120U}, sf::Color::White));
        ellipseOutline.setTool(minipaint::ToolType::Ellipse);
        ellipseOutline.setPrimaryColor(sf::Color::Black);
        ellipseOutline.setBrushSize(1.0F);
        ellipseOutline.handleEvent(mousePressed(10, 10));
        ellipseOutline.handleEvent(mouseMoved(190, 110));
        ellipseOutline.handleEvent(mouseReleased(190, 110));
        const sf::Image ellipseImage = ellipseOutline.getResult();
        bool continuousOutline = true;
        constexpr float pi = 3.14159265358979323846F;
        for (unsigned int sample = 0; sample < 72U; ++sample)
        {
            const float angle = static_cast<float>(sample) * 2.0F * pi / 72.0F;
            const auto x = static_cast<unsigned int>(std::floor(100.0F + 90.0F * std::cos(angle)));
            const auto y = static_cast<unsigned int>(std::floor(60.0F + 50.0F * std::sin(angle)));
            bool foundOutlinePixel = false;
            for (int offsetY = -1; offsetY <= 1 && !foundOutlinePixel; ++offsetY)
            {
                for (int offsetX = -1; offsetX <= 1; ++offsetX)
                {
                    const int sampleX = static_cast<int>(x) + offsetX;
                    const int sampleY = static_cast<int>(y) + offsetY;
                    if (sampleX >= 0 && sampleY >= 0 && sampleX < 200 && sampleY < 120 &&
                        ellipseImage.getPixel({static_cast<unsigned int>(sampleX),
                                               static_cast<unsigned int>(sampleY)}) == sf::Color::Black)
                    {
                        foundOutlinePixel = true;
                        break;
                    }
                }
            }
            if (!foundOutlinePixel)
            {
                continuousOutline = false;
                break;
            }
        }
        check(continuousOutline, "ellipse outline has no gaps around its perimeter");

        minipaint::MiniPaint clearedHistory(8U, 8U);
        clearedHistory.loadFromImage(sf::Image({8U, 8U}, sf::Color::White));
        clearedHistory.setPrimaryColor(sf::Color::Black);
        clearedHistory.handleEvent(mousePressed(2, 2));
        clearedHistory.clearHistory();
        clearedHistory.handleEvent(mouseReleased(2, 2));
        check(!clearedHistory.isDrawing(), "clearHistory cancels the active stroke");
        check(!clearedHistory.canUndo(), "clearHistory prevents later strokes from repopulating history");
        check(clearedHistory.getResult().getPixel({2U, 2U}) == sf::Color::White,
              "clearHistory restores partially drawn pixels");

        minipaint::MiniPaint clearColors(4U, 4U);
        clearColors.loadFromImage(sf::Image({4U, 4U}, sf::Color::White));
        clearColors.setBackgroundColor(sf::Color::Blue);
        clearColors.clear(sf::Color::Red);
        check(clearColors.getBackgroundColor() == sf::Color::Blue,
              "clear does not silently change the eraser background color");
        check(clearColors.undo(), "clear can be undone");
        check(clearColors.getBackgroundColor() == sf::Color::Blue,
              "undo keeps the eraser background color unchanged");

        minipaint::MiniPaint focusLoss(8U, 8U);
        focusLoss.loadFromImage(sf::Image({8U, 8U}, sf::Color::White));
        focusLoss.setPrimaryColor(sf::Color::Black);
        focusLoss.handleEvent(mousePressed(2, 2));
        focusLoss.handleEvent(sf::Event(sf::Event::FocusLost{}));
        check(focusLoss.getResult().getPixel({2U, 2U}) == sf::Color::White,
              "focus loss cancels an uncommitted stroke");
        check(!focusLoss.isDrawing(), "focus loss ends the active interaction");

        minipaint::MiniPaint historyLimit(4U, 4U);
        historyLimit.loadFromImage(sf::Image({4U, 4U}, sf::Color::White));
        historyLimit.setUndoLimit(2U);
        historyLimit.clear(sf::Color::Red);
        historyLimit.clear(sf::Color::Green);
        check(historyLimit.undo(), "first undo succeeds");
        check(historyLimit.undo(), "second undo succeeds");
        check(!historyLimit.undo(), "undo stops at the configured history limit");
        check(historyLimit.canRedo(), "redo remains available within the history limit");
        check(historyLimit.redo(), "redo succeeds within the history limit");
        historyLimit.setUndoLimit(0U);
        check(!historyLimit.canUndo(), "a zero history limit removes undo commands");
        check(!historyLimit.canRedo(), "a zero history limit removes redo commands");
        check(!historyLimit.redo(), "redo cannot bypass a zero history limit");

        const sf::FloatRect customViewport{{10.0F, 20.0F}, {100.0F, 50.0F}};
        minipaint::MiniPaint viewportCanvas(4U, 4U);
        viewportCanvas.setViewport(customViewport);
        viewportCanvas.loadFromImage(sf::Image({12U, 20U}, sf::Color::Green));
        check(viewportCanvas.getViewport() == customViewport,
              "loading an image preserves an explicitly configured viewport");

        minipaint::MiniPaint secondaryCanvas(4U, 4U);
        secondaryCanvas.loadFromImage(sf::Image({4U, 4U}, sf::Color::White));
        secondaryCanvas.setPrimaryColor(sf::Color::Red);
        secondaryCanvas.setSecondaryColor(sf::Color::Blue);
        secondaryCanvas.handleEvent(rightMousePressed(1, 1));
        secondaryCanvas.handleEvent(rightMouseReleased(1, 1));
        check(secondaryCanvas.getResult().getPixel({1U, 1U}) == sf::Color::Blue,
              "right-button drawing uses the secondary color");

        minipaint::MiniPaint secondaryPicker(4U, 4U);
        secondaryPicker.loadFromImage(sf::Image({4U, 4U}, sf::Color::Blue));
        secondaryPicker.setTool(minipaint::ToolType::Eyedropper);
        secondaryPicker.setPrimaryColor(sf::Color::Red);
        secondaryPicker.setSecondaryColor(sf::Color::Green);
        secondaryPicker.handleEvent(rightMousePressed(1, 1));
        check(secondaryPicker.getSecondaryColor() == sf::Color::Blue,
              "right-button eyedropper updates the secondary color");
        check(secondaryPicker.getPrimaryColor() == sf::Color::Red,
              "right-button eyedropper leaves the primary color unchanged");

        // --- Regression: undo of a stroke on a non-active layer must
        //     re-composite every layer (MiniPaint.cpp undo/redo path).
        minipaint::MiniPaint multiLayer(8U, 8U);
        multiLayer.removeLayer(0U);
        check(multiLayer.addLayer("red", sf::Color::Red) == 0U, "bottom layer added");
        check(multiLayer.addLayer("top", sf::Color::Transparent) == 1U, "top layer added");
        multiLayer.setActiveLayer(0U);
        multiLayer.setPrimaryColor(sf::Color::Green);
        multiLayer.setBrushSize(2.0F);
        multiLayer.handleEvent(mousePressed(2, 2));
        multiLayer.handleEvent(mouseReleased(2, 2));
        multiLayer.setActiveLayer(1U);
        check(multiLayer.undo(), "undo of a non-active layer stroke succeeds");
        check(multiLayer.getResult().getPixel({2U, 2U}) == sf::Color::Red,
              "undo re-composites all layers, not just the active one");

        // --- Regression: rotating a layer must create an undo command.
        minipaint::MiniPaint rotateCanvas(16U, 8U);
        rotateCanvas.loadFromImage(sf::Image({16U, 8U}, sf::Color::White));
        check(rotateCanvas.rotateActiveLayer(true), "rotate succeeds");
        check(rotateCanvas.getSize() == sf::Vector2u(8U, 16U), "rotated canvas changed size");
        check(rotateCanvas.canUndo(), "rotation is undoable");
        check(rotateCanvas.undo(), "undo rotation succeeds");
        check(rotateCanvas.getSize() == sf::Vector2u(16U, 8U), "undo rotation restores size");

        // --- Regression: clear() must respect the layer lock.
        minipaint::MiniPaint lockedClear(4U, 4U);
        lockedClear.loadFromImage(sf::Image({4U, 4U}, sf::Color::Red));
        lockedClear.setLayerLocked(0U, true);
        lockedClear.clear(sf::Color::Blue);
        check(lockedClear.getResult().getPixel({0U, 0U}) == sf::Color::Red,
              "clear() does not modify a locked layer");

        // --- Regression: getLayerName() out of range returns an empty string.
        check(lockedClear.getLayerName(99U).empty(),
              "getLayerName() out of range returns an empty string");

        // --- Regression: filled rectangle must not paint outside the rect.
        minipaint::MiniPaint filledRect(32U, 32U);
        filledRect.loadFromImage(sf::Image({32U, 32U}, sf::Color::White));
        filledRect.setTool(minipaint::ToolType::Rectangle);
        filledRect.setPrimaryColor(sf::Color::Black);
        filledRect.setBrushSize(1.0F);
        filledRect.setFillShapes(true);
        filledRect.handleEvent(mousePressed(4, 4));
        filledRect.handleEvent(mouseReleased(20, 12));
        check(filledRect.getResult().getPixel({19U, 11U}) == sf::Color::Black,
              "filled rectangle paints its interior");
        check(filledRect.getResult().getPixel({20U, 11U}) == sf::Color::White,
              "filled rectangle does not paint past its right edge");
        check(filledRect.getResult().getPixel({19U, 12U}) == sf::Color::White,
              "filled rectangle does not paint past its bottom edge");

        // --- Regression: adding a layer must not invalidate undo commands.
        minipaint::MiniPaint addLayerUndo(8U, 8U);
        addLayerUndo.loadFromImage(sf::Image({8U, 8U}, sf::Color::White));
        addLayerUndo.setPrimaryColor(sf::Color::Black);
        addLayerUndo.handleEvent(mousePressed(2, 2));
        addLayerUndo.handleEvent(mouseReleased(2, 2));
        check(addLayerUndo.addLayer("extra", sf::Color::Transparent) == 1U,
              "addLayer succeeds");
        check(addLayerUndo.undo(), "undo after addLayer does not crash");
        check(addLayerUndo.getResult().getPixel({2U, 2U}) == sf::Color::White,
              "undo after addLayer restores pixels");

        // --- Regression: pencil strokes must be continuous on diagonals.
        minipaint::MiniPaint diagonal(32U, 32U);
        diagonal.loadFromImage(sf::Image({32U, 32U}, sf::Color::White));
        diagonal.setPrimaryColor(sf::Color::Black);
        diagonal.setBrushSize(1.0F);
        diagonal.handleEvent(mousePressed(2, 2));
        for (int step = 1; step <= 20; ++step)
        {
            diagonal.handleEvent(mouseMoved(2 + step, 2 + step));
        }
        diagonal.handleEvent(mouseReleased(22, 22));
        bool continuous = true;
        for (int step = 0; step <= 20; ++step)
        {
            if (diagonal.getResult().getPixel({static_cast<unsigned int>(2 + step),
                                               static_cast<unsigned int>(2 + step)}) !=
                sf::Color::Black)
            {
                continuous = false;
                break;
            }
        }
        check(continuous, "diagonal pencil strokes have no gaps");

        // --- Regression: resizeCanvas preserves a custom viewport.
        const sf::FloatRect customViewportSize{{10.0F, 20.0F}, {100.0F, 50.0F}};
        minipaint::MiniPaint resizeViewport(8U, 8U);
        resizeViewport.setViewport(customViewportSize);
        resizeViewport.resizeCanvas({16U, 16U});
        check(resizeViewport.getViewport() == customViewportSize,
              "resizeCanvas keeps an explicitly configured viewport");
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Unexpected exception: " << exception.what() << '\n';
        return EXIT_FAILURE;
    }

    if (failures != 0)
    {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All MiniPaint smoke tests passed\n";
    return EXIT_SUCCESS;
}
