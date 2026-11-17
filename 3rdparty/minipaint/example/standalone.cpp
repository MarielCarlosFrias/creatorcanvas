#include "MiniPaint.hpp"
#include "Tool.hpp"
#include "EmbeddedIcons.hpp"

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/ConvexShape.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/Graphics/Vertex.hpp>
#include <SFML/System/Exception.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <SFML/Window/VideoMode.hpp>
#include <SFML/Window/Cursor.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iostream>
#include <optional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------------
// MiniPaint standalone example - a Paint-like editor with a classic MS Paint
// inspired layout, written only in SFML (no third-party UI dependency).
//
//   Window
//   ├── Ribbon (top): Tools | Pincel | Edição | Camadas
//   ├── Canvas (center, checkerboard)
//   ├── Color bar (bottom): current colors + palette swatches
//   └── Status bar (very bottom): hints, canvas size, zoom slider
// ---------------------------------------------------------------------------

namespace
{
// ------------------------------------------------------------- palette ---
constexpr sf::Color kWindowBg{229, 236, 244};
constexpr sf::Color kRibbonBg{245, 246, 247};
constexpr sf::Color kRibbonBorder{218, 222, 227};
constexpr sf::Color kDivider{222, 227, 233};
constexpr sf::Color kCaptionText{100, 100, 100};
constexpr sf::Color kHoverFill{232, 242, 252};
constexpr sf::Color kHoverBorder{126, 180, 234};
constexpr sf::Color kActiveFill{205, 228, 252};
constexpr sf::Color kActiveBorder{51, 140, 230};
constexpr sf::Color kDisabledText{176, 176, 176};
constexpr sf::Color kStatusBg{240, 240, 240};
constexpr sf::Color kStatusBorder{214, 218, 222};
constexpr sf::Color kStatusText{68, 68, 68};
constexpr sf::Color kAccent{25, 121, 202};
constexpr sf::Color kIcon{48, 48, 48};
constexpr sf::Color kSwatchBorder{190, 195, 200};
constexpr sf::Color kPanelBg{255, 255, 255};
constexpr sf::Color kPanelBorder{200, 200, 200};

// Layout of the top area, from top to bottom (MS Paint style):
//   row 0 (30 px): quick access toolbar (paint palette icon, save/undo/redo, filename)
//   row 1 (30 px): File menu + tabs ("Início" | "Exibir")
//   row 2 (100 px): ribbon content (tool groups, brushes, shapes, size, color palette)
constexpr float kQuickAccessHeight = 30.0F;
constexpr float kTabHeight = 30.0F;
constexpr float kRibbonContentTop = kQuickAccessHeight + kTabHeight; // 60
constexpr float kRibbonHeight = 162.0F;                             // total top area
constexpr float kPaletteHeight = 0.0F;                              // palette integrated into ribbon
constexpr float kStatusHeight = 26.0F;

// Ribbon geometry shared by drawing and events:
constexpr float kBrushSliderX = 470.0F;
constexpr float kBrushSliderY = 104.0F;
constexpr float kBrushSliderWidth = 92.0F;
constexpr float kFillCheckboxX = 352.0F;
constexpr float kFillCheckboxY = 102.0F;
constexpr float kColor1BoxX = 582.0F;
constexpr float kColor1BoxY = 66.0F;
constexpr float kColor2BoxX = 624.0F;
constexpr float kColor2BoxY = 66.0F;
constexpr float kPaletteStartX = 672.0F;
constexpr float kPaletteStartY = 66.0F;

struct CliOptions
{
    std::filesystem::path input;
    std::filesystem::path output;
    minipaint::ToolType tool{minipaint::ToolType::Pencil};
    float brushSize{4.0F};
    std::string primaryColor{"000000FF"};
    std::string secondaryColor{"FFFFFFFF"};
    std::string backgroundColor{"FFFFFFFF"};
    std::string title{"MiniPaint"};
    bool help{false};
};

sf::String utf8(const std::string& string)
{
    return sf::String::fromUtf8(string.begin(), string.end());
}

bool parseHexColor(const std::string& hex, sf::Color& out)
{
    if (hex.size() != 8U)
    {
        return false;
    }

    unsigned int value{0U};
    try
    {
        value = std::stoul(hex, nullptr, 16);
    }
    catch (...)
    {
        return false;
    }

    out = sf::Color(static_cast<std::uint8_t>((value >> 24) & 0xFFU),
                    static_cast<std::uint8_t>((value >> 16) & 0xFFU),
                    static_cast<std::uint8_t>((value >> 8) & 0xFFU),
                    static_cast<std::uint8_t>(value & 0xFFU));
    return true;
}

CliOptions parseCli(int argc, char* argv[])
{
    CliOptions opts;

    for (int i = 1; i < argc; ++i)
    {
        std::string_view arg = argv[i];

        if (arg == "--help" || arg == "-h")
        {
            opts.help = true;
        }
        else if (arg == "--input" && i + 1 < argc)
        {
            opts.input = argv[++i];
        }
        else if (arg == "--output" && i + 1 < argc)
        {
            opts.output = argv[++i];
        }
        else if (arg == "--tool" && i + 1 < argc)
        {
            const std::string_view t = argv[++i];
            if (t == "pencil") opts.tool = minipaint::ToolType::Pencil;
            else if (t == "eraser") opts.tool = minipaint::ToolType::Eraser;
            else if (t == "line") opts.tool = minipaint::ToolType::Line;
            else if (t == "rectangle") opts.tool = minipaint::ToolType::Rectangle;
            else if (t == "ellipse") opts.tool = minipaint::ToolType::Ellipse;
            else if (t == "bucket") opts.tool = minipaint::ToolType::BucketFill;
            else if (t == "eyedropper") opts.tool = minipaint::ToolType::Eyedropper;
            else if (t == "airbrush") opts.tool = minipaint::ToolType::Airbrush;
            else if (t == "text") opts.tool = minipaint::ToolType::Text;
        }
        else if (arg == "--brush-size" && i + 1 < argc)
        {
            try
            {
                opts.brushSize = std::stof(std::string(argv[++i]));
            }
            catch (...)
            {
            }
        }
        else if (arg == "--primary-color" && i + 1 < argc)
        {
            opts.primaryColor = argv[++i];
        }
        else if (arg == "--secondary-color" && i + 1 < argc)
        {
            opts.secondaryColor = argv[++i];
        }
        else if (arg == "--background-color" && i + 1 < argc)
        {
            opts.backgroundColor = argv[++i];
        }
        else if (arg == "--title" && i + 1 < argc)
        {
            opts.title = argv[++i];
        }
    }

    return opts;
}

void printHelp()
{
    std::cout << "MiniPaint - Paint-like editor for SFML 3\n"
              << "Usage: minipaint-standalone [options]\n\n"
              << "Options:\n"
              << "  --help, -h               Show this help\n"
              << "  --input <path>          Load PNG file on startup\n"
              << "  --output <path>         Save PNG file with Ctrl+S / on exit\n"
              << "  --tool <name>           pencil, eraser, line, rectangle, ellipse,\n"
              << "                          bucket, eyedropper, airbrush, text\n"
              << "  --brush-size <px>       Brush diameter (1-128)\n"
              << "  --primary-color <hex>   RRGGBBAA (default 000000FF)\n"
              << "  --secondary-color <hex> RRGGBBAA (default FFFFFFFF)\n"
              << "  --background-color <hex> RRGGBBAA (default FFFFFFFF)\n"
              << "  --title <text>          Window title\n\n"
              << "Controls:\n"
              << "  Left click/drag         Draw with primary color\n"
              << "  Right click/drag        Draw with secondary color\n"
              << "  Left-click palette      Set primary color\n"
              << "  Right-click palette     Set secondary color\n"
              << "  1-9                     Select tool\n"
              << "  H / V                   Flip horizontally / vertically\n"
              << "  R                       Rotate 90 degrees clockwise\n"
              << "  Ctrl+Z / Ctrl+Y         Undo / redo\n"
              << "  Ctrl+S                  Save PNG\n"
              << "  + / -                   Zoom in / out\n"
              << "  Mouse wheel             Zoom in / out\n"
              << "  Esc                     Quit (or cancel text entry)\n";
}

bool loadFont(sf::Font& font)
{
    static const std::array<const char*, 5> candidates = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf"
    };

    for (const char* path : candidates)
    {
        try
        {
            if (font.openFromFile(path))
            {
                return true;
            }
        }
        catch (const sf::Exception&)
        {
        }
    }

    return false;
}

sf::Vector2f screenToCanvas(const sf::Vector2i& position,
                            const sf::FloatRect& viewport,
                            const sf::Vector2u canvasSize)
{
    return {
        (static_cast<float>(position.x) - viewport.position.x) *
            static_cast<float>(canvasSize.x) / viewport.size.x,
        (static_cast<float>(position.y) - viewport.position.y) *
            static_cast<float>(canvasSize.y) / viewport.size.y
    };
}

void appendUtf8(std::string& output, const std::uint32_t codepoint)
{
    if (codepoint <= 0x7FU)
    {
        output.push_back(static_cast<char>(codepoint));
    }
    else if (codepoint <= 0x7FFU)
    {
        output.push_back(static_cast<char>(0xC0U | (codepoint >> 6U)));
        output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    }
    else if (codepoint <= 0xFFFFU)
    {
        output.push_back(static_cast<char>(0xE0U | (codepoint >> 12U)));
        output.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    }
    else if (codepoint <= 0x10FFFFU)
    {
        output.push_back(static_cast<char>(0xF0U | (codepoint >> 18U)));
        output.push_back(static_cast<char>(0x80U | ((codepoint >> 12U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    }
}

// ------------------------------------------------------------ drawing ---
const sf::Texture* iconTexture(const std::string_view icon)
{
    static std::unordered_map<std::string, std::unique_ptr<sf::Texture>> cache;
    const std::string key(icon);
    const auto found = cache.find(key);
    if (found != cache.end())
    {
        return found->second.get();
    }

    sf::Image image;
    bool loaded = false;

    // Check custom icons from disk first (user icons directory)
    const std::string candidates[] = {
        std::string("icons/") + std::string(icon) + ".png",
        std::string("example/icons/") + std::string(icon) + ".png"
    };
    for (const auto& path : candidates)
    {
        if (std::filesystem::exists(path))
        {
            try
            {
                if (image.loadFromFile(path))
                {
                    loaded = true;
                    break;
                }
            }
            catch (...)
            {
            }
        }
    }

    if (!loaded)
    {
        const auto embedded = minipaint::example::findEmbeddedIcon(icon);
        if (embedded.data == nullptr)
        {
            return nullptr;
        }

        if (!image.loadFromMemory(embedded.data, embedded.size))
        {
            return nullptr;
        }
    }

    auto texture = std::make_unique<sf::Texture>(image);
    if (texture->getSize().x == 0U)
    {
        return nullptr;
    }
    texture->setSmooth(true);

    const sf::Texture* raw = texture.get();
    cache.emplace(std::move(key), std::move(texture));
    return raw;
}

void drawIcon(sf::RenderTarget& target,
              const std::string_view icon,
              const sf::Vector2f center,
              const float size,
              const sf::Color color = sf::Color::White)
{
    const sf::Texture* texture = iconTexture(icon);
    if (texture == nullptr)
    {
        return;
    }

    const sf::Vector2u textureSize = texture->getSize();
    const float scale = size / static_cast<float>(textureSize.x);
    sf::Sprite sprite(*texture);
    sprite.setOrigin({static_cast<float>(textureSize.x) * 0.5F,
                      static_cast<float>(textureSize.y) * 0.5F});
    sprite.setPosition(center);
    sprite.setScale({scale, scale});
    sprite.setColor(color);
    target.draw(sprite);
}


// --------------------------------------------------------------- UI ---
struct IconButton
{
    sf::FloatRect bounds;
    std::string_view icon;
    std::function<void()> action;
    std::string hint;
    bool enabled{true};
    bool active{false};
};

struct Slider
{
    sf::FloatRect track;
    float value01{0.0F};
    std::string hint;
};

// MS Paint-style color grid (two rows of fourteen).
constexpr std::array<sf::Color, 28> kPaintPalette = {
    sf::Color(0, 0, 0),       sf::Color(127, 63, 0),   sf::Color(136, 0, 21),
    sf::Color(237, 28, 36),   sf::Color(255, 174, 201), sf::Color(255, 127, 39),
    sf::Color(255, 201, 14),  sf::Color(239, 228, 176), sf::Color(34, 177, 76),
    sf::Color(181, 230, 29),  sf::Color(0, 162, 232),  sf::Color(153, 217, 234),
    sf::Color(63, 72, 204),   sf::Color(112, 146, 190),
    sf::Color(255, 255, 255), sf::Color(195, 195, 195), sf::Color(136, 136, 136),
    sf::Color(128, 128, 128), sf::Color(95, 45, 13),   sf::Color(163, 73, 164),
    sf::Color(185, 122, 87),  sf::Color(133, 97, 0),   sf::Color(0, 91, 24),
    sf::Color(97, 124, 54),   sf::Color(0, 80, 239),   sf::Color(49, 84, 163),
    sf::Color(68, 35, 89),    sf::Color(163, 2, 0)
};

struct uiContext
{
    sf::Font font;
    bool hasFont{false};
    sf::Vector2i mouse{-1, -1};
};

void drawText(sf::RenderTarget& target,
              const sf::Font& font,
              const std::string& text,
              const sf::Vector2f& position,
              const unsigned int size,
              const sf::Color& color)
{
    sf::Text label(font, utf8(text), size);
    label.setPosition(position);
    label.setFillColor(color);
    target.draw(label);
}

void drawButton(sf::RenderTarget& target,
                const uiContext& ui,
                const IconButton& button,
                const float iconSize = 22.0F)
{
    const bool hovered = button.enabled && button.bounds.contains(
        {static_cast<float>(ui.mouse.x), static_cast<float>(ui.mouse.y)});

    sf::RectangleShape face(button.bounds.size);
    face.setPosition(button.bounds.position);
    if (!button.enabled)
    {
        face.setFillColor(sf::Color(245, 245, 245, 100));
        face.setOutlineColor(sf::Color(230, 230, 230));
        face.setOutlineThickness(1.0F);
    }
    else if (button.active)
    {
        face.setFillColor(kActiveFill);
        face.setOutlineColor(kActiveBorder);
        face.setOutlineThickness(1.0F);
    }
    else if (hovered)
    {
        face.setFillColor(kHoverFill);
        face.setOutlineColor(kHoverBorder);
        face.setOutlineThickness(1.0F);
    }
    else
    {
        face.setFillColor(sf::Color::Transparent);
        face.setOutlineColor(sf::Color::Transparent);
        face.setOutlineThickness(0.0F);
    }
    target.draw(face);

    // Keep full authentic colors for enabled buttons, attenuate for disabled
    const sf::Color iconTint = button.enabled ? sf::Color::White : sf::Color(255, 255, 255, 90);
    drawIcon(target, button.icon,
             {button.bounds.position.x + button.bounds.size.x * 0.5F,
              button.bounds.position.y + button.bounds.size.y * 0.5F},
             iconSize,
             iconTint);
}

void drawSectionCaption(sf::RenderTarget& target,
                        const uiContext& ui,
                        const float x,
                        const float width,
                        const std::string& caption)
{
    if (!ui.hasFont)
    {
        return;
    }

    sf::Text label(ui.font, utf8(caption), 10);
    const auto rect = label.getLocalBounds();
    label.setPosition({x + (width - rect.size.x) * 0.5F,
                       kRibbonHeight - 18.0F});
    label.setFillColor(kCaptionText);
    target.draw(label);
}

void drawDivider(sf::RenderTarget& target, const float x)
{
    sf::RectangleShape divider({1.0F, kRibbonHeight - kRibbonContentTop - 10.0F});
    divider.setPosition({x, kRibbonContentTop + 4.0F});
    divider.setFillColor(kDivider);
    target.draw(divider);
}
} // namespace

namespace
{
float canvasAreaTop()
{
    return kRibbonHeight;
}

float canvasAreaHeight(const unsigned int windowHeight)
{
    return static_cast<float>(windowHeight) - kRibbonHeight - kPaletteHeight - kStatusHeight;
}

float paletteTop(const unsigned int windowHeight)
{
    return static_cast<float>(windowHeight) - kPaletteHeight - kStatusHeight;
}

float statusTop(const unsigned int windowHeight)
{
    return static_cast<float>(windowHeight) - kStatusHeight;
}

void updateCanvasViewport(const sf::Vector2u windowSize,
                          sf::FloatRect& viewport,
                          minipaint::MiniPaint& paint,
                          const float zoom)
{
    const float canvasWidth = static_cast<float>(paint.getSize().x);
    const float canvasHeight = static_cast<float>(paint.getSize().y);

    const float availableWidth = std::max(1.0F, static_cast<float>(windowSize.x) - 32.0F);
    const float availableHeight = std::max(1.0F, canvasAreaHeight(windowSize.y) - 24.0F);
    const float fitScale = std::min(availableWidth / canvasWidth,
                                    availableHeight / canvasHeight);
    const float scale = std::clamp(fitScale * zoom, fitScale * 0.25F,
                                   fitScale * 8.0F);

    viewport = {{16.0F + std::max(0.0F, (availableWidth - canvasWidth * scale) * 0.5F),
                 kRibbonHeight + 12.0F + std::max(0.0F, (availableHeight - canvasHeight * scale) * 0.5F)},
                {canvasWidth * scale, canvasHeight * scale}};
    paint.setViewport(viewport);
}

constexpr std::array<minipaint::ToolType, 9> kTools = {
    minipaint::ToolType::Pencil, minipaint::ToolType::Eraser,
    minipaint::ToolType::Line,   minipaint::ToolType::Rectangle,
    minipaint::ToolType::Ellipse, minipaint::ToolType::BucketFill,
    minipaint::ToolType::Eyedropper, minipaint::ToolType::Airbrush,
    minipaint::ToolType::Text
};
constexpr std::array<std::string_view, 9> kToolIcons = {
    "pencil", "eraser", "line", "rectangle", "ellipse",
    "bucket", "eyedropper", "airbrush", "text"
};

struct uiState
{
    float zoom{1.0F};
    bool layerPanelOpen{false};
    bool zoomDragging{false};
    bool brushDragging{false};
    bool gridVisible{false};
    int activeColorSlot{1}; // 1 = Cor 1 (Primeiro plano), 2 = Cor 2 (Plano de fundo)
    std::string activeTab{"home"};
    std::string currentFile; // empty = "Sem título"
    sf::FloatRect canvasViewport{};
    sf::Vector2i mouse{-1, -1};
};

std::vector<IconButton> makeQuickAccessButtons(minipaint::MiniPaint& paint,
                                               const std::function<void()>& saveImage)
{
    const float y = 2.0F;
    constexpr float w = 26.0F;
    constexpr float h = 26.0F;
    std::vector<IconButton> buttons;
    buttons.push_back({{{40.0F, y}, {w, h}}, "save", saveImage, "Guardar imagem (Ctrl+S)", true, false});
    buttons.push_back({{{68.0F, y}, {w, h}}, "undo", [&paint] { (void)paint.undo(); },
                       "Anular (Ctrl+Z)", paint.canUndo(), false});
    buttons.push_back({{{96.0F, y}, {w, h}}, "redo", [&paint] { (void)paint.redo(); },
                       "Refazer (Ctrl+Y)", paint.canRedo(), false});
    return buttons;
}

std::vector<IconButton> makeRibbonButtons(minipaint::MiniPaint& paint,
                                          uiState& state,
                                          const CliOptions& opts,
                                          const std::function<void()>& newDocument,
                                          const std::function<void()>& resizeCanvas,
                                          const std::function<void()>& openDialog,
                                          const std::function<void()>& saveDialog)
{
    std::vector<IconButton> buttons;
    const auto add = [&](const float x, const float y, const float w, const float h,
                         const std::string_view icon, std::function<void()> action,
                         std::string hint, const bool enabled, const bool active = false) {
        buttons.push_back({{{x, y}, {w, h}}, icon,
                           std::move(action), std::move(hint), enabled, active});
    };

    if (state.activeTab == "view")
    {
        // --- Grupo: Zoom
        add(12.0F, 66.0F, 30.0F, 30.0F, "minus", [&state] { state.zoom = std::max(0.25F, state.zoom / 1.25F); },
            "Reduzir zoom (-)", true);
        add(46.0F, 66.0F, 30.0F, 30.0F, "plus", [&state] { state.zoom = std::min(8.0F, state.zoom * 1.25F); },
            "Ampliar zoom (+)", true);
        add(80.0F, 66.0F, 30.0F, 30.0F, "fit", [&state] { state.zoom = 1.0F; },
            "Ajustar à janela (100%)", true);

        // --- Grupo: Mostrar
        add(160.0F, 66.0F, 30.0F, 30.0F, "grid", [&state] { state.gridVisible = !state.gridVisible; },
            "Mostrar/ocultar a grelha (Ctrl+G)", true, state.gridVisible);
        add(194.0F, 66.0F, 30.0F, 30.0F, "layers", [&state] { state.layerPanelOpen = !state.layerPanelOpen; },
            "Mostrar/ocultar o painel de camadas", true, state.layerPanelOpen);
        return buttons;
    }

    // --- Página Inicial: Grupo Imagem (x = 12 a 144)
    // Linha 1 (y = 66)
    add(12.0F, 66.0F, 28.0F, 28.0F, "filePlus", newDocument, "Novo ficheiro (Ctrl+N)", true);
    add(44.0F, 66.0F, 28.0F, 28.0F, "load", openDialog, "Abrir imagem (Ctrl+O)", true);
    add(76.0F, 66.0F, 28.0F, 28.0F, "save", saveDialog, "Guardar imagem (Ctrl+S)", true);
    add(108.0F, 66.0F, 28.0F, 28.0F, "resize", resizeCanvas, "Redimensionar tela", true);
    // Linha 2 (y = 98)
    add(12.0F, 98.0F, 28.0F, 28.0F, "rotate", [&paint] { (void)paint.rotateActiveLayer(true); },
        "Rodar 90° para a direita (R)", true);
    add(44.0F, 98.0F, 28.0F, 28.0F, "flipH", [&paint] { (void)paint.flipActiveLayerHorizontal(); },
        "Inverter na horizontal (H)", true);
    add(76.0F, 98.0F, 28.0F, 28.0F, "flipV", [&paint] { (void)paint.flipActiveLayerVertical(); },
        "Inverter na vertical (V)", true);
    add(108.0F, 98.0F, 28.0F, 28.0F, "clear", [&paint] { paint.clear(); },
        "Limpar a camada ativa", true);

    // --- Página Inicial: Grupo Ferramentas (Grade 3x2 clássica em x = 158 a 258)
    // Coluna 1
    add(160.0F, 66.0F, 28.0F, 28.0F, "pencil", [&paint] { paint.setTool(minipaint::ToolType::Pencil); },
        "Lápis (tecla 1)", true, paint.getTool() == minipaint::ToolType::Pencil);
    add(160.0F, 98.0F, 28.0F, 28.0F, "eraser", [&paint] { paint.setTool(minipaint::ToolType::Eraser); },
        "Borracha (tecla 2)", true, paint.getTool() == minipaint::ToolType::Eraser);
    // Coluna 2
    add(192.0F, 66.0F, 28.0F, 28.0F, "bucket", [&paint] { paint.setTool(minipaint::ToolType::BucketFill); },
        "Preenchimento com cor / Balde (tecla 6)", true, paint.getTool() == minipaint::ToolType::BucketFill);
    add(192.0F, 98.0F, 28.0F, 28.0F, "eyedropper", [&paint] { paint.setTool(minipaint::ToolType::Eyedropper); },
        "Conta-gotas (tecla 7)", true, paint.getTool() == minipaint::ToolType::Eyedropper);
    // Coluna 3
    add(224.0F, 66.0F, 28.0F, 28.0F, "text", [&paint] { paint.setTool(minipaint::ToolType::Text); },
        "Texto (tecla 9)", true, paint.getTool() == minipaint::ToolType::Text);
    add(224.0F, 98.0F, 28.0F, 28.0F, "fit", [&state] { state.zoom = 1.0F; },
        "Ajustar à janela (100%)", true, state.zoom == 1.0F);

    // --- Página Inicial: Grupo Pincéis (x = 268 a 340)
    add(272.0F, 66.0F, 30.0F, 32.0F, "brush", [&paint] { paint.setTool(minipaint::ToolType::Pencil); },
        "Pincel Artístico", true, paint.getTool() == minipaint::ToolType::Pencil);
    add(304.0F, 66.0F, 30.0F, 32.0F, "airbrush", [&paint] { paint.setTool(minipaint::ToolType::Airbrush); },
        "Aerossol / Spray (tecla 8)", true, paint.getTool() == minipaint::ToolType::Airbrush);

    // --- Página Inicial: Grupo Formas (x = 352 a 460)
    add(356.0F, 66.0F, 28.0F, 28.0F, "line", [&paint] { paint.setTool(minipaint::ToolType::Line); },
        "Linha (tecla 3)", true, paint.getTool() == minipaint::ToolType::Line);
    add(388.0F, 66.0F, 28.0F, 28.0F, "rectangle", [&paint] { paint.setTool(minipaint::ToolType::Rectangle); },
        "Retângulo (tecla 4)", true, paint.getTool() == minipaint::ToolType::Rectangle);
    add(420.0F, 66.0F, 28.0F, 28.0F, "ellipse", [&paint] { paint.setTool(minipaint::ToolType::Ellipse); },
        "Elipse (tecla 5)", true, paint.getTool() == minipaint::ToolType::Ellipse);

    // --- Página Inicial: Grupo Tamanho (botões de espessura rápida em x = 470 a 566)
    add(470.0F, 66.0F, 22.0F, 22.0F, "", [&paint] { paint.setBrushSize(1.0F); },
        "Espessura: 1 px", true, std::abs(paint.getBrushSize() - 1.0F) < 0.5F);
    add(494.0F, 66.0F, 22.0F, 22.0F, "", [&paint] { paint.setBrushSize(3.0F); },
        "Espessura: 3 px", true, std::abs(paint.getBrushSize() - 3.0F) < 0.5F);
    add(518.0F, 66.0F, 22.0F, 22.0F, "", [&paint] { paint.setBrushSize(6.0F); },
        "Espessura: 6 px", true, std::abs(paint.getBrushSize() - 6.0F) < 0.5F);
    add(542.0F, 66.0F, 22.0F, 22.0F, "", [&paint] { paint.setBrushSize(12.0F); },
        "Espessura: 12 px", true, std::abs(paint.getBrushSize() - 12.0F) < 0.5F);

    // --- Página Inicial: Grupo Camadas & Histórico (x = 988 a 1150)
    add(990.0F, 66.0F, 28.0F, 28.0F, "undo", [&paint] { (void)paint.undo(); },
        "Anular (Ctrl+Z)", paint.canUndo());
    add(1022.0F, 66.0F, 28.0F, 28.0F, "redo", [&paint] { (void)paint.redo(); },
        "Refazer (Ctrl+Y)", paint.canRedo());
    add(1060.0F, 66.0F, 28.0F, 28.0F, "layers", [&state] { state.layerPanelOpen = !state.layerPanelOpen; },
        "Mostrar/ocultar o painel de camadas", true, state.layerPanelOpen);
    add(1092.0F, 66.0F, 28.0F, 28.0F, "plus",
        [&paint] { (void)paint.addLayer("Camada " + std::to_string(paint.getLayerCount() + 1U), sf::Color::Transparent); },
        "Nova camada", true);
    add(1124.0F, 66.0F, 28.0F, 28.0F, "minus",
        [&paint] { (void)paint.removeLayer(paint.getActiveLayerIndex()); },
        "Excluir camada ativa", paint.getLayerCount() > 1U);

    return buttons;
}
} // namespace

namespace
{
constexpr float kSwatchWidth = 15.0F;
constexpr float kSwatchHeight = 15.0F;
constexpr float kSwatchGap = 3.0F;

std::vector<sf::FloatRect> paletteSwatchBounds(const float startX = kPaletteStartX, const float startY = kPaletteStartY)
{
    std::vector<sf::FloatRect> bounds;
    constexpr std::size_t perRow = 14U;
    for (std::size_t index = 0; index < kPaintPalette.size(); ++index)
    {
        const float column = static_cast<float>(index % perRow);
        const float row = static_cast<float>(index / perRow);
        bounds.push_back({{startX + column * (kSwatchWidth + kSwatchGap),
                           startY + row * (kSwatchHeight + kSwatchGap + 2.0F)},
                          {kSwatchWidth, kSwatchHeight}});
    }
    return bounds;
}

struct LayerPanel
{
    sf::FloatRect bounds{};
    struct Row
    {
        sf::FloatRect row;
        sf::FloatRect eye;
        sf::FloatRect lock;
        sf::FloatRect name;
        std::size_t index{0U};
    };
    std::vector<Row> rows;
    sf::FloatRect addButton{};
    sf::FloatRect removeButton{};
};

LayerPanel computeLayerPanel(const sf::Vector2u windowSize, minipaint::MiniPaint& paint)
{
    LayerPanel panel;
    constexpr float width = 232.0F;
    const float x = static_cast<float>(windowSize.x) - width - 8.0F;
    const float y = kRibbonHeight + 8.0F;

    const float available = canvasAreaHeight(windowSize.y) - 16.0F;
    const std::size_t maxRows = available > 92.0F
        ? static_cast<std::size_t>((available - 92.0F) / 32.0F)
        : 0U;
    const std::size_t layerCount = std::min(paint.getLayerCount(), std::max<std::size_t>(1U, maxRows));

    const float height = 34.0F + static_cast<float>(layerCount) * 32.0F + 46.0F;
    panel.bounds = {{x, y}, {width, std::min(height, available)}};

    float rowY = y + 34.0F;
    for (std::size_t index = 0; index < layerCount; ++index)
    {
        LayerPanel::Row row;
        row.index = index;
        row.row = {{x + 4.0F, rowY}, {width - 8.0F, 30.0F}};
        row.eye = {{x + 8.0F, rowY + 4.0F}, {22.0F, 22.0F}};
        row.lock = {{x + 34.0F, rowY + 4.0F}, {22.0F, 22.0F}};
        row.name = {{x + 60.0F, rowY}, {width - 68.0F, 30.0F}};
        panel.rows.push_back(row);
        rowY += 32.0F;
    }

    const float buttonsY = rowY + 6.0F;
    panel.addButton = {{x + 8.0F, buttonsY}, {22.0F, 22.0F}};
    panel.removeButton = {{x + 36.0F, buttonsY}, {22.0F, 22.0F}};
    return panel;
}

sf::FloatRect zoomMinusBounds(const unsigned int windowWidth, const float barTop, const float barHeight)
{
    return {{static_cast<float>(windowWidth) - 244.0F, barTop + (barHeight - 20.0F) * 0.5F}, {20.0F, 20.0F}};
}

sf::FloatRect zoomTrackBounds(const unsigned int windowWidth, const float barTop, const float barHeight)
{
    return {{static_cast<float>(windowWidth) - 220.0F, barTop + barHeight * 0.5F - 2.0F}, {112.0F, 4.0F}};
}

sf::FloatRect zoomPlusBounds(const unsigned int windowWidth, const float barTop, const float barHeight)
{
    return {{static_cast<float>(windowWidth) - 104.0F, barTop + (barHeight - 20.0F) * 0.5F}, {20.0F, 20.0F}};
}

void drawCheckerboard(sf::RenderTarget& target, const sf::FloatRect& area)
{
    constexpr float tile = 16.0F;
    const sf::Color light(232, 234, 238);
    const sf::Color dark(205, 210, 218);
    sf::RectangleShape square({tile, tile});

    const float startY = area.position.y;
    const float endY = area.position.y + area.size.y;
    const float startX = area.position.x;
    const float endX = area.position.x + area.size.x;

    for (float y = startY; y < endY; y += tile)
    {
        for (float x = startX; x < endX; x += tile)
        {
            const int column = static_cast<int>((x - startX) / tile);
            const int row = static_cast<int>((y - startY) / tile);
            square.setPosition({x, y});
            square.setFillColor(((column + row) % 2 == 0) ? light : dark);
            square.setSize({std::min(tile, endX - x), std::min(tile, endY - y)});
            target.draw(square);
        }
    }
}

void drawLayerPanel(sf::RenderTarget& target,
                    const uiContext& ui,
                    const LayerPanel& panel,
                    minipaint::MiniPaint& paint)
{
    sf::RectangleShape background(panel.bounds.size);
    background.setPosition(panel.bounds.position);
    background.setFillColor(kPanelBg);
    background.setOutlineColor(kPanelBorder);
    background.setOutlineThickness(1.0F);
    target.draw(background);

    if (ui.hasFont)
    {
        drawText(target, ui.font, "Camadas", {panel.bounds.position.x + 10.0F,
                                              panel.bounds.position.y + 8.0F},
                 13U, kIcon);
    }

    const std::size_t activeIndex = paint.getActiveLayerIndex();
    for (const LayerPanel::Row& row : panel.rows)
    {
        const bool active = row.index == activeIndex;

        if (active)
        {
            sf::RectangleShape highlight(row.row.size);
            highlight.setPosition(row.row.position);
            highlight.setFillColor(kHoverFill);
            target.draw(highlight);

            sf::RectangleShape bar({3.0F, row.row.size.y});
            bar.setPosition(row.row.position);
            bar.setFillColor(kAccent);
            target.draw(bar);
        }

        const bool visible = paint.getLayerVisibility(row.index);
        const bool locked = paint.getLayerLocked(row.index);
        const bool eyeHovered = row.eye.contains({static_cast<float>(ui.mouse.x), static_cast<float>(ui.mouse.y)});
        const bool lockHovered = row.lock.contains({static_cast<float>(ui.mouse.x), static_cast<float>(ui.mouse.y)});

        if (eyeHovered || lockHovered)
        {
            sf::RectangleShape hover(eyeHovered ? row.eye.size : row.lock.size);
            hover.setPosition(eyeHovered ? row.eye.position : row.lock.position);
            hover.setFillColor(kHoverFill);
            target.draw(hover);
        }

        drawIcon(target, visible ? "eye" : "eyeOff",
                 {row.eye.position.x + 11.0F, row.eye.position.y + 11.0F},
                 16.0F, visible ? kIcon : kDisabledText);
        drawIcon(target, locked ? "lock" : "unlock",
                 {row.lock.position.x + 11.0F, row.lock.position.y + 11.0F},
                 16.0F, locked ? kAccent : kDisabledText);

        if (ui.hasFont)
        {
            std::string name = paint.getLayerName(row.index);
            sf::Text label(ui.font, utf8(name), 13U);
            while (label.getLocalBounds().size.x > row.name.size.x - 4.0F && name.size() > 1U)
            {
                name.pop_back();
                label.setString(utf8(name + "…"));
            }
            label.setPosition({row.name.position.x + 2.0F, row.name.position.y + 6.0F});
            label.setFillColor(kIcon);
            target.draw(label);
        }
    }

    IconButton plus{panel.addButton, "plus", {}, "Nova camada", true, false};
    IconButton minus{panel.removeButton, "minus", {}, "Excluir camada ativa", paint.getLayerCount() > 1U, false};
    drawButton(target, ui, plus, 14.0F);
    drawButton(target, ui, minus, 14.0F);
}

void drawRibbonGroup(sf::RenderTarget& target,
                     const uiContext& ui,
                     const float x,
                     const float width,
                     const std::string& caption)
{
    // Divider line on the right side of the group
    sf::RectangleShape divider({1.0F, kRibbonHeight - kRibbonContentTop - 14.0F});
    divider.setPosition({x + width - 1.0F, kRibbonContentTop + 6.0F});
    divider.setFillColor(kDivider);
    target.draw(divider);

    if (ui.hasFont && !caption.empty())
    {
        sf::Text label(ui.font, utf8(caption), 11U);
        const auto rect = label.getLocalBounds();
        label.setPosition({x + (width - rect.size.x) * 0.5F,
                           kRibbonHeight - 17.0F});
        label.setFillColor(kCaptionText);
        target.draw(label);
    }
}

void drawSizeSection(sf::RenderTarget& target,
                     const uiContext& ui,
                     minipaint::MiniPaint& paint,
                     std::string& hint)
{
    // 4 quick brush size buttons with horizontal line previews
    constexpr float sizes[4] = {1.0F, 3.0F, 6.0F, 12.0F};
    for (int i = 0; i < 4; ++i)
    {
        const sf::FloatRect b{{470.0F + static_cast<float>(i) * 24.0F, 66.0F}, {22.0F, 22.0F}};
        const bool hovered = b.contains({static_cast<float>(ui.mouse.x), static_cast<float>(ui.mouse.y)});
        const bool active = std::abs(paint.getBrushSize() - sizes[i]) < 0.8F;

        sf::RectangleShape face(b.size);
        face.setPosition(b.position);
        face.setFillColor(active ? kActiveFill : (hovered ? kHoverFill : sf::Color::Transparent));
        face.setOutlineColor(active ? kActiveBorder : (hovered ? kHoverBorder : sf::Color::Transparent));
        face.setOutlineThickness(1.0F);
        target.draw(face);

        const float lineThickness = std::max(1.0F, std::min(sizes[i], 8.0F));
        sf::RectangleShape line({14.0F, lineThickness});
        line.setOrigin({7.0F, lineThickness * 0.5F});
        line.setPosition({b.position.x + 11.0F, b.position.y + 11.0F});
        line.setFillColor(active ? sf::Color(10, 60, 140) : sf::Color(60, 60, 60));
        target.draw(line);

        if (hovered)
        {
            hint = "Espessura de linha: " + std::to_string(static_cast<int>(sizes[i])) + " px";
        }
    }

    // Brush slider for fine tuning
    const float sliderX = kBrushSliderX;
    const float sliderY = kBrushSliderY;
    const float sliderWidth = kBrushSliderWidth;
    sf::RectangleShape track({sliderWidth, 4.0F});
    track.setPosition({sliderX, sliderY + 14.0F});
    track.setFillColor(sf::Color(210, 215, 220));
    target.draw(track);

    const float brushFraction = std::clamp((paint.getBrushSize() - 1.0F) / 127.0F, 0.0F, 1.0F);
    sf::CircleShape handle(6.0F, 16);
    handle.setFillColor(sf::Color::White);
    handle.setOutlineColor(kAccent);
    handle.setOutlineThickness(1.6F);
    handle.setPosition({sliderX + brushFraction * sliderWidth - 6.0F, sliderY + 10.0F});
    target.draw(handle);

    if (ui.hasFont)
    {
        drawText(target, ui.font,
                 std::to_string(static_cast<int>(paint.getBrushSize())) + " px",
                 {sliderX + sliderWidth - 28.0F, 66.0F + 2.0F}, 11U, kCaptionText);
    }
}

void drawShapesSection(sf::RenderTarget& target,
                       const uiContext& ui,
                       minipaint::MiniPaint& paint,
                       std::string& hint)
{
    const sf::FloatRect checkbox{{kFillCheckboxX, kFillCheckboxY}, {14.0F, 14.0F}};
    const bool hovered = checkbox.contains({static_cast<float>(ui.mouse.x), static_cast<float>(ui.mouse.y)});

    sf::RectangleShape box(checkbox.size);
    box.setPosition(checkbox.position);
    box.setFillColor(hovered ? kHoverFill : sf::Color::White);
    box.setOutlineColor(kSwatchBorder);
    box.setOutlineThickness(1.2F);
    target.draw(box);

    if (paint.getFillShapes())
    {
        sf::RectangleShape checkMark({8.0F, 8.0F});
        checkMark.setPosition({checkbox.position.x + 3.0F, checkbox.position.y + 3.0F});
        checkMark.setFillColor(kAccent);
        target.draw(checkMark);
    }

    if (ui.hasFont)
    {
        drawText(target, ui.font, "Preencher",
                 {checkbox.position.x + 18.0F, checkbox.position.y - 1.0F},
                 11U, kCaptionText);
    }

    if (hovered)
    {
        hint = "Preencher formas: retângulo e elipse com cor sólida";
    }
}

void drawRibbonColors(sf::RenderTarget& target,
                      const uiContext& ui,
                      minipaint::MiniPaint& paint,
                      const uiState& state,
                      std::string& hint)
{
    const auto primary = paint.getPrimaryColor();
    const auto secondary = paint.getSecondaryColor();

    // Box 1: Cor 1 (Primeiro Plano)
    const sf::FloatRect box1Rect{{kColor1BoxX, kColor1BoxY}, {36.0F, 36.0F}};
    const bool box1Hovered = box1Rect.contains({static_cast<float>(ui.mouse.x), static_cast<float>(ui.mouse.y)});
    
    sf::RectangleShape box1Card({38.0F, 50.0F});
    box1Card.setPosition({kColor1BoxX - 1.0F, kColor1BoxY - 2.0F});
    if (state.activeColorSlot == 1)
    {
        box1Card.setFillColor(kActiveFill);
        box1Card.setOutlineColor(kActiveBorder);
        box1Card.setOutlineThickness(1.2F);
    }
    else if (box1Hovered)
    {
        box1Card.setFillColor(kHoverFill);
        box1Card.setOutlineColor(kHoverBorder);
        box1Card.setOutlineThickness(1.0F);
    }
    else
    {
        box1Card.setFillColor(sf::Color::Transparent);
        box1Card.setOutlineColor(sf::Color::Transparent);
    }
    target.draw(box1Card);

    sf::RectangleShape box1(box1Rect.size);
    box1.setPosition(box1Rect.position);
    box1.setFillColor(primary);
    box1.setOutlineColor(sf::Color(110, 110, 110));
    box1.setOutlineThickness(1.0F);
    target.draw(box1);

    if (ui.hasFont)
    {
        drawText(target, ui.font, "Cor 1", {kColor1BoxX + 3.0F, kColor1BoxY + 38.0F}, 10U, kCaptionText);
    }

    // Box 2: Cor 2 (Plano de Fundo)
    const sf::FloatRect box2Rect{{kColor2BoxX, kColor2BoxY}, {36.0F, 36.0F}};
    const bool box2Hovered = box2Rect.contains({static_cast<float>(ui.mouse.x), static_cast<float>(ui.mouse.y)});

    sf::RectangleShape box2Card({38.0F, 50.0F});
    box2Card.setPosition({kColor2BoxX - 1.0F, kColor2BoxY - 2.0F});
    if (state.activeColorSlot == 2)
    {
        box2Card.setFillColor(kActiveFill);
        box2Card.setOutlineColor(kActiveBorder);
        box2Card.setOutlineThickness(1.2F);
    }
    else if (box2Hovered)
    {
        box2Card.setFillColor(kHoverFill);
        box2Card.setOutlineColor(kHoverBorder);
        box2Card.setOutlineThickness(1.0F);
    }
    else
    {
        box2Card.setFillColor(sf::Color::Transparent);
        box2Card.setOutlineColor(sf::Color::Transparent);
    }
    target.draw(box2Card);

    sf::RectangleShape box2(box2Rect.size);
    box2.setPosition(box2Rect.position);
    box2.setFillColor(secondary);
    box2.setOutlineColor(sf::Color(110, 110, 110));
    box2.setOutlineThickness(1.0F);
    target.draw(box2);

    if (ui.hasFont)
    {
        drawText(target, ui.font, "Cor 2", {kColor2BoxX + 3.0F, kColor2BoxY + 38.0F}, 10U, kCaptionText);
    }

    if (box1Hovered)
    {
        hint = "Cor 1 (Primeiro Plano) - Usada com o botão esquerdo do rato";
    }
    else if (box2Hovered)
    {
        hint = "Cor 2 (Plano de Fundo) - Usada com o botão direito do rato e borracha";
    }

    // Palette swatches (2 rows of 14)
    const auto bounds = paletteSwatchBounds();
    const std::size_t selected = paint.getPalette().empty() ? static_cast<std::size_t>(-1)
                                                            : paint.getSelectedPaletteIndex();
    for (std::size_t index = 0; index < bounds.size() && index < paint.getPalette().size(); ++index)
    {
        sf::RectangleShape swatch(bounds[index].size);
        swatch.setPosition(bounds[index].position);
        swatch.setFillColor(paint.getPalette()[index]);

        const bool hovered = bounds[index].contains({static_cast<float>(ui.mouse.x), static_cast<float>(ui.mouse.y)});
        if (hovered)
        {
            swatch.setOutlineColor(kAccent);
            swatch.setOutlineThickness(2.0F);
            hint = "Paleta: botão esquerdo define Cor " + std::to_string(state.activeColorSlot) + ", direito define Cor 2";
        }
        else if (index == selected)
        {
            swatch.setOutlineColor(kAccent);
            swatch.setOutlineThickness(1.5F);
        }
        else
        {
            swatch.setOutlineColor(kSwatchBorder);
            swatch.setOutlineThickness(1.0F);
        }
        target.draw(swatch);
    }

    // Edit colors button
    const sf::FloatRect editColorsBtn{{kPaletteStartX + 14.0F * 18.0F + 8.0F, kPaletteStartY}, {36.0F, 50.0F}};
    const bool editHovered = editColorsBtn.contains({static_cast<float>(ui.mouse.x), static_cast<float>(ui.mouse.y)});
    sf::RectangleShape editFace(editColorsBtn.size);
    editFace.setPosition(editColorsBtn.position);
    editFace.setFillColor(editHovered ? kHoverFill : sf::Color::Transparent);
    editFace.setOutlineColor(editHovered ? kHoverBorder : sf::Color::Transparent);
    editFace.setOutlineThickness(1.0F);
    target.draw(editFace);
    drawIcon(target, "colorEdit", {editColorsBtn.position.x + 18.0F, editColorsBtn.position.y + 16.0F}, 24.0F);
    if (ui.hasFont)
    {
        drawText(target, ui.font, "Editar", {editColorsBtn.position.x + 4.0F, editColorsBtn.position.y + 32.0F}, 10U, kCaptionText);
        drawText(target, ui.font, "cores", {editColorsBtn.position.x + 4.0F, editColorsBtn.position.y + 42.0F}, 10U, kCaptionText);
    }
    if (editHovered)
    {
        hint = "Editar cores - Personalizar a cor selecionada";
    }
}

void drawStatusBar(sf::RenderTarget& target,
                   const uiContext& ui,
                   const sf::FloatRect& bar,
                   const std::string& leftText,
                   const std::string& centerText,
                   const float zoom)
{

    sf::RectangleShape background(bar.size);
    background.setPosition(bar.position);
    background.setFillColor(kStatusBg);
    target.draw(background);

    sf::RectangleShape topBorder({bar.size.x, 1.0F});
    topBorder.setPosition(bar.position);
    topBorder.setFillColor(kStatusBorder);
    target.draw(topBorder);

    if (ui.hasFont)
    {
        drawText(target, ui.font, leftText, {bar.position.x + 8.0F, bar.position.y + 6.0F},
                 12U, kStatusText);

        sf::Text center(ui.font, utf8(centerText), 12U);
        const auto cb = center.getLocalBounds();
        center.setPosition({bar.position.x + (bar.size.x - cb.size.x) * 0.5F,
                            bar.position.y + 6.0F});
        center.setFillColor(kStatusText);
        target.draw(center);

        const std::string percent = std::to_string(static_cast<int>(zoom * 100.0F)) + "%";
        sf::Text zoomText(ui.font, utf8(percent), 12U);
        const auto zb = zoomText.getLocalBounds();
        zoomText.setPosition({bar.position.x + bar.size.x - 8.0F - zb.size.x,
                              bar.position.y + 6.0F});
        zoomText.setFillColor(kStatusText);
        target.draw(zoomText);
    }

    // Zoom controls on the right end of the status bar.
    const unsigned int w = static_cast<unsigned int>(bar.size.x) + static_cast<unsigned int>(bar.position.x);

    IconButton minus{zoomMinusBounds(w, bar.position.y, bar.size.y), "minus", {}, "Reduzir zoom (-)", true, false};
    IconButton plus{zoomPlusBounds(w, bar.position.y, bar.size.y), "plus", {}, "Ampliar zoom (+)", true, false};
    drawButton(target, ui, minus, 12.0F);
    drawButton(target, ui, plus, 12.0F);

    const sf::FloatRect track = zoomTrackBounds(w, bar.position.y, bar.size.y);
    sf::RectangleShape trackShape(track.size);
    trackShape.setPosition(track.position);
    trackShape.setFillColor(sf::Color(216, 216, 216));
    target.draw(trackShape);

    const float t = (zoom - 0.25F) / (8.0F - 0.25F);
    const float handleX = track.position.x + t * track.size.x;
    sf::CircleShape handle(7.0F, 16);
    handle.setFillColor(sf::Color::White);
    handle.setOutlineColor(kAccent);
    handle.setOutlineThickness(1.6F);
    handle.setPosition({handleX - 7.0F, track.position.y + track.size.y * 0.5F - 7.0F});
    target.draw(handle);
}
} // namespace


// ------------------------------------------------------------- Dialogs ---
enum class DialogKind
{
    None,
    Open,
    Save,
    Resize
};

/// In-app modal dialog: file browser, save-as and canvas resize.
struct AppDialog
{
    bool open{false};
    DialogKind kind{DialogKind::None};
    std::string path;
    std::string width{"800"};
    std::string height{"600"};
    int activeField{0}; // 0 = path, 1 = width, 2 = height
    std::vector<std::string> entries;
    std::size_t selected{0U};
    std::string message;

    sf::FloatRect bounds{};
    sf::FloatRect listBounds{};
    sf::FloatRect pathField{};
    sf::FloatRect widthField{};
    sf::FloatRect heightField{};
    sf::FloatRect confirmButton{};
    sf::FloatRect cancelButton{};
};

bool hasImageExtension(const std::string& name)
{
    static const std::array<std::string, 8> extensions = {
        ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".gif", ".webp", ".psd"};
    const std::string lower = [&name] {
        std::string copy = name;
        std::transform(copy.begin(), copy.end(), copy.begin(),
                       [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return copy;
    }();

    for (const std::string& extension : extensions)
    {
        if (lower.size() >= extension.size() &&
            lower.compare(lower.size() - extension.size(), extension.size(), extension) == 0)
        {
            return true;
        }
    }
    return false;
}

void refreshDirectory(AppDialog& dialog, const std::filesystem::path& directory)
{
    dialog.entries.clear();
    dialog.selected = 0U;

    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(directory, error))
    {
        std::string name = entry.path().filename().string();
        if (entry.is_directory(error))
        {
            dialog.entries.push_back("[" + name + "]");
        }
        else if (hasImageExtension(name))
        {
            dialog.entries.push_back(name);
        }
    }

    std::sort(dialog.entries.begin(), dialog.entries.end(),
              [](const std::string& left, const std::string& right) {
                  const bool leftDir = !left.empty() && left.front() == '[';
                  const bool rightDir = !right.empty() && right.front() == '[';
                  if (leftDir != rightDir)
                  {
                      return leftDir;
                  }
                  return left < right;
              });
}

void layoutDialog(AppDialog& dialog, const sf::Vector2u windowSize)
{
    const bool isOpen = dialog.kind == DialogKind::Open;
    const float width = isOpen ? 560.0F : 440.0F;
    const float height = isOpen ? 400.0F : 220.0F;
    const float x = (static_cast<float>(windowSize.x) - width) * 0.5F;
    const float y = (static_cast<float>(windowSize.y) - height) * 0.5F;

    dialog.bounds = {{x, y}, {width, height}};

    if (isOpen)
    {
        dialog.listBounds = {{x + 14.0F, y + 46.0F}, {width - 28.0F, height - 148.0F}};
    }

    const float fieldY = y + height - 92.0F;
    dialog.pathField = {{x + 74.0F, fieldY}, {width - 92.0F, 26.0F}};
    dialog.widthField = {{x + 96.0F, fieldY}, {130.0F, 26.0F}};
    dialog.heightField = {{x + 246.0F, fieldY}, {130.0F, 26.0F}};

    const float buttonY = y + height - 48.0F;
    dialog.confirmButton = {{x + width - 236.0F, buttonY}, {140.0F, 30.0F}};
    dialog.cancelButton = {{x + width - 88.0F, buttonY}, {74.0F, 30.0F}};
}

void drawTextButton(sf::RenderTarget& target,
                    const uiContext& ui,
                    const sf::FloatRect& bounds,
                    const std::string& label,
                    const bool primary)
{
    const bool hovered = bounds.contains({static_cast<float>(ui.mouse.x),
                                          static_cast<float>(ui.mouse.y)});

    sf::RectangleShape face(bounds.size);
    face.setPosition(bounds.position);
    if (primary)
    {
        face.setFillColor(hovered ? sf::Color(0, 103, 192) : kAccent);
        face.setOutlineColor(sf::Color(0, 86, 160));
    }
    else
    {
        face.setFillColor(hovered ? sf::Color(240, 243, 247) : sf::Color(250, 250, 250));
        face.setOutlineColor(sf::Color(200, 200, 200));
    }
    face.setOutlineThickness(1.0F);
    target.draw(face);

    if (ui.hasFont)
    {
        sf::Text text(ui.font, utf8(label), 13U);
        const auto bounds2 = text.getLocalBounds();
        text.setPosition({bounds.position.x + (bounds.size.x - bounds2.size.x) * 0.5F,
                          bounds.position.y + (bounds.size.y - bounds2.size.y) * 0.5F});
        text.setFillColor(primary ? sf::Color::White : sf::Color(45, 45, 45));
        target.draw(text);
    }
}

void drawTextField(sf::RenderTarget& target,
                   const uiContext& ui,
                   const sf::FloatRect& bounds,
                   const std::string& value,
                   const bool focused)
{
    sf::RectangleShape field(bounds.size);
    field.setPosition(bounds.position);
    field.setFillColor(sf::Color::White);
    field.setOutlineColor(focused ? kAccent : sf::Color(190, 190, 190));
    field.setOutlineThickness(focused ? 1.8F : 1.0F);
    target.draw(field);

    if (ui.hasFont)
    {
        sf::Text text(ui.font, utf8(value), 13U);
        text.setPosition({bounds.position.x + 6.0F, bounds.position.y + 5.0F});
        text.setFillColor(sf::Color(35, 35, 35));
        target.draw(text);

        if (focused)
        {
            const float caretX = bounds.position.x + 6.0F + text.getLocalBounds().size.x + 1.0F;
            sf::RectangleShape caret({1.5F, 16.0F});
            caret.setPosition({caretX, bounds.position.y + 5.0F});
            caret.setFillColor(sf::Color(35, 35, 35));
            target.draw(caret);
        }
    }
}

void drawDialog(sf::RenderTarget& target, const uiContext& ui, const AppDialog& dialog)
{
    // Dim the application behind the modal panel.
    sf::RectangleShape dim({static_cast<float>(target.getSize().x),
                            static_cast<float>(target.getSize().y)});
    dim.setFillColor(sf::Color(0, 0, 0, 90));
    target.draw(dim);

    sf::RectangleShape panel(dialog.bounds.size);
    panel.setPosition(dialog.bounds.position);
    panel.setFillColor(kPanelBg);
    panel.setOutlineColor(sf::Color(120, 120, 120));
    panel.setOutlineThickness(1.0F);
    target.draw(panel);

    std::string title = "MiniPaint";
    switch (dialog.kind)
    {
        case DialogKind::Open: title = "Abrir imagem"; break;
        case DialogKind::Save: title = "Guardar imagem"; break;
        case DialogKind::Resize: title = "Redimensionar tela"; break;
        case DialogKind::None: break;
    }

    if (ui.hasFont)
    {
        drawText(target, ui.font, title, {dialog.bounds.position.x + 16.0F,
                                          dialog.bounds.position.y + 11.0F},
                 15U, sf::Color(30, 30, 30));

        if (!dialog.message.empty())
        {
            drawText(target, ui.font, dialog.message,
                     {dialog.bounds.position.x + 16.0F, dialog.bounds.position.y + 34.0F},
                     12U, sf::Color(190, 40, 40));
        }
    }

    if (dialog.kind == DialogKind::Open)
    {
        // File list.
        sf::RectangleShape list(dialog.listBounds.size);
        list.setPosition(dialog.listBounds.position);
        list.setFillColor(sf::Color(252, 252, 252));
        list.setOutlineColor(sf::Color(200, 200, 200));
        list.setOutlineThickness(1.0F);
        target.draw(list);

        if (ui.hasFont)
        {
            const float rowHeight = 19.0F;
            const std::size_t visible = static_cast<std::size_t>(
                std::max(1.0F, dialog.listBounds.size.y / rowHeight));
            const std::size_t first = dialog.selected < visible
                                          ? 0U
                                          : std::min(dialog.selected - visible + 1U,
                                                     dialog.entries.size());

            for (std::size_t index = first; index < dialog.entries.size() &&
                                           index < first + visible;
                 ++index)
            {
                const float y = dialog.listBounds.position.y +
                                static_cast<float>(index - first) * rowHeight;
                if (index == dialog.selected)
                {
                    sf::RectangleShape highlight({dialog.listBounds.size.x, rowHeight});
                    highlight.setPosition({dialog.listBounds.position.x, y});
                    highlight.setFillColor(kHoverFill);
                    target.draw(highlight);
                }
                drawText(target, ui.font, dialog.entries[index],
                         {dialog.listBounds.position.x + 6.0F, y + 2.0F}, 13U,
                         sf::Color(40, 40, 40));
            }
        }
    }

    const float fieldY = dialog.pathField.position.y;
    if (ui.hasFont)
    {
        if (dialog.kind == DialogKind::Resize)
        {
            drawText(target, ui.font, "Largura:",
                     {dialog.bounds.position.x + 20.0F, fieldY + 6.0F}, 13U, sf::Color(60, 60, 60));
            drawText(target, ui.font, "Altura:",
                     {dialog.bounds.position.x + 170.0F, fieldY + 6.0F}, 13U, sf::Color(60, 60, 60));
        }
        else
        {
            drawText(target, ui.font, "Ficheiro:",
                     {dialog.bounds.position.x + 16.0F, fieldY + 6.0F}, 13U, sf::Color(60, 60, 60));
        }
    }

    if (dialog.kind == DialogKind::Resize)
    {
        drawTextField(target, ui, dialog.widthField, dialog.width, dialog.activeField == 1);
        drawTextField(target, ui, dialog.heightField, dialog.height, dialog.activeField == 2);
    }
    else
    {
        drawTextField(target, ui, dialog.pathField, dialog.path, dialog.activeField == 0);
    }

    drawTextButton(target, ui, dialog.confirmButton,
                   dialog.kind == DialogKind::Open ? "Abrir"
                   : dialog.kind == DialogKind::Save ? "Guardar"
                                                     : "Redimensionar",
                   true);
    drawTextButton(target, ui, dialog.cancelButton, "Cancelar", false);
}

int main(int argc, char* argv[])
{
    const CliOptions opts = parseCli(argc, argv);

    if (opts.help)
    {
        printHelp();
        return 0;
    }

    uiContext ui;
    ui.hasFont = loadFont(ui.font);

    sf::RenderWindow window(sf::VideoMode({1200U, 780U}), opts.title);
    window.setFramerateLimit(60U);

    // App icon: a little paint palette.
    {
        sf::Image icon;
        icon.resize({16U, 16U}, sf::Color::Transparent);
        const sf::Vector2u iconSize = icon.getSize();
        const auto setPx = [&icon, iconSize](const int x, const int y, const sf::Color color) {
            if (x >= 0 && x < static_cast<int>(iconSize.x) &&
                y >= 0 && y < static_cast<int>(iconSize.y))
            {
                icon.setPixel({static_cast<unsigned int>(x), static_cast<unsigned int>(y)}, color);
            }
        };
        for (int y = 0; y < 16; ++y)
        {
            for (int x = 0; x < 16; ++x)
            {
                const float dx = (static_cast<float>(x) - 8.0F) / 7.0F;
                const float dy = (static_cast<float>(y) - 9.0F) / 6.0F;
                if (dx * dx + dy * dy <= 1.0F)
                {
                    setPx(x, y, sf::Color(70, 70, 70));
                }
            }
        }
        const struct { int x; int y; sf::Color c; } dots[5] = {
            {5, 6, sf::Color(237, 28, 36)},
            {10, 5, sf::Color(255, 201, 14)},
            {12, 9, sf::Color(34, 177, 76)},
            {5, 11, sf::Color(0, 162, 232)},
            {10, 12, sf::Color(255, 255, 255)}
        };
        for (const auto& dot : dots)
        {
            for (int dy = -1; dy <= 1; ++dy)
            {
                for (int dx = -1; dx <= 1; ++dx)
                {
                    setPx(dot.x + dx, dot.y + dy, dot.c);
                }
            }
        }
        window.setIcon(icon);
    }

    // Cursors: arrow over the chrome, crosshair over the canvas, I-beam in text mode.
    const std::optional<sf::Cursor> arrowCursor = sf::Cursor::createFromSystem(sf::Cursor::Type::Arrow);
    const std::optional<sf::Cursor> crossCursor = sf::Cursor::createFromSystem(sf::Cursor::Type::Cross);
    const std::optional<sf::Cursor> iBeamCursor = sf::Cursor::createFromSystem(sf::Cursor::Type::Text);

    minipaint::MiniPaint paint(800U, 600U);

    // Hook the Paint-style palette into the module palette so swatch
    // selection, eyedropper, and palette APIs stay consistent.
    paint.setPalette(std::vector<sf::Color>(kPaintPalette.begin(), kPaintPalette.end()));
    paint.selectPaletteColor(0U);

    paint.setTool(opts.tool);
    paint.setBrushSize(opts.brushSize);

    sf::Color primaryColor;
    if (parseHexColor(opts.primaryColor, primaryColor))
    {
        paint.setPrimaryColor(primaryColor);
    }

    sf::Color secondaryColor;
    if (parseHexColor(opts.secondaryColor, secondaryColor))
    {
        paint.setSecondaryColor(secondaryColor);
    }

    sf::Color backgroundColor;
    if (parseHexColor(opts.backgroundColor, backgroundColor))
    {
        paint.setBackgroundColor(backgroundColor);
    }

    uiState state;
    if (!opts.input.empty())
    {
        std::error_code inputError;
        if (!std::filesystem::exists(opts.input, inputError))
        {
            std::cerr << "MiniPaint: o ficheiro '" << opts.input.string()
                      << "' nao existe - a comecar com uma imagem em branco.\n";
        }
        else if (!paint.loadFromFile(opts.input))
        {
            std::cerr << "MiniPaint: nao foi possivel carregar '" << opts.input.string()
                      << "'.\n";
        }
        else
        {
            state.currentFile = opts.input.string();
        }
    }

    updateCanvasViewport(window.getSize(), state.canvasViewport, paint, state.zoom);

    std::chrono::steady_clock::time_point hintOverrideUntil{};
    std::string transientHint;

    bool textEntryActive = false;
    sf::Vector2f textEntryPosition{};
    std::string textEntry;

    std::string lastTitle;
    auto updateWindowTitle = [&]() {
        const std::string base = state.currentFile.empty()
            ? std::string("Sem título")
            : std::filesystem::path(state.currentFile).filename().string();
        const std::string title = base + " - MiniPaint";
        if (title != lastTitle)
        {
            window.setTitle(utf8(title));
            lastTitle = title;
        }
    };

    AppDialog dialog;

    const auto showHint = [&](const std::string& text) {
        transientHint = text;
        hintOverrideUntil = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    };

    const auto saveTo = [&](const std::filesystem::path& path) {
        std::error_code error;
        if (path.has_parent_path() && !path.parent_path().empty())
        {
            std::filesystem::create_directories(path.parent_path(), error);
        }

        if (paint.saveToFile(path))
        {
            state.currentFile = path.string();
            updateWindowTitle();
            return true;
        }
        return false;
    };

    const auto loadFrom = [&](const std::filesystem::path& path) {
        std::error_code error;
        if (!std::filesystem::exists(path, error))
        {
            return false;
        }
        if (!paint.loadFromFile(path))
        {
            return false;
        }
        state.currentFile = path.string();
        updateWindowTitle();
        return true;
    };

    const auto newDocument = [&]() {
        const sf::Vector2u size = paint.getSize();
        const sf::Image blank(size, sf::Color::White);
        (void)paint.loadFromImage(blank);
        state.currentFile.clear();
        updateWindowTitle();
        showHint("Nova imagem criada");
    };

    const auto openDialog = [&]() {
        dialog.open = true;
        dialog.kind = DialogKind::Open;
        dialog.activeField = 0;
        dialog.message.clear();
        dialog.path = state.currentFile.empty() ? std::string("minipaint-output.png")
                                                : state.currentFile;
        const std::filesystem::path directory =
            std::filesystem::path(dialog.path).has_parent_path()
                ? std::filesystem::path(dialog.path).parent_path()
                : std::filesystem::path(".");
        refreshDirectory(dialog, directory);
        layoutDialog(dialog, window.getSize());
    };

    const auto saveDialog = [&]() {
        dialog.open = true;
        dialog.kind = DialogKind::Save;
        dialog.activeField = 0;
        dialog.message.clear();
        dialog.path = state.currentFile.empty() ? std::string("minipaint-output.png")
                                                : state.currentFile;
        layoutDialog(dialog, window.getSize());
    };

    const auto resizeDialog = [&]() {
        dialog.open = true;
        dialog.kind = DialogKind::Resize;
        dialog.activeField = 1;
        dialog.message.clear();
        dialog.width = std::to_string(paint.getSize().x);
        dialog.height = std::to_string(paint.getSize().y);
        layoutDialog(dialog, window.getSize());
    };

    const auto confirmDialog = [&]() -> bool {
        if (dialog.kind == DialogKind::Resize)
        {
            const int width = std::atoi(dialog.width.c_str());
            const int height = std::atoi(dialog.height.c_str());
            if (width <= 0 || height <= 0 || width > 4096 || height > 4096)
            {
                dialog.message = "Dimensões inválidas (1 a 4096 pixels).";
                return false;
            }
            if (!paint.resizeCanvas({static_cast<unsigned int>(width),
                                     static_cast<unsigned int>(height)}))
            {
                dialog.message = "Não foi possível redimensionar a tela.";
                return false;
            }
            showHint("Tela redimensionada");
            dialog.open = false;
            return true;
        }

        const std::filesystem::path path(dialog.path);
        if (dialog.kind == DialogKind::Save)
        {
            if (path.empty())
            {
                dialog.message = "Indique um nome de ficheiro.";
                return false;
            }
            if (!saveTo(path))
            {
                dialog.message = "Não foi possível guardar o ficheiro.";
                return false;
            }
            showHint("Imagem guardada: " + path.string());
            dialog.open = false;
            return true;
        }

        // Open
        if (path.empty())
        {
            dialog.message = "Indique um nome de ficheiro.";
            return false;
        }
        if (!loadFrom(path))
        {
            dialog.message = "Ficheiro não encontrado ou formato não suportado.";
            return false;
        }
        showHint("Imagem carregada: " + path.string());
        dialog.open = false;
        return true;
    };

    // Redraw only when something actually changed: an idle frame used to
// re-render the whole ribbon + canvas + checkerboard (dozens of shapes and
// texts) 60 times a second for nothing. Every state change in this app flows
// through an event, so "no events this frame" means "nothing to draw".
bool needsRedraw = true;
    // Safety net: the window must also repaint when it was covered by another
    // window and gets exposed again (SFML 3 has no expose event). ~2 repaints
    // per second idle instead of 60 keeps that case seamless.
    unsigned int frameCounter = 0;

    while (window.isOpen())
    {
        updateCanvasViewport(window.getSize(), state.canvasViewport, paint, state.zoom);
        updateWindowTitle();
        if ((++frameCounter % 30U) == 0U)
        {
            needsRedraw = true;
        }
        const auto quickButtons = makeQuickAccessButtons(paint, saveDialog);
        const auto buttons = makeRibbonButtons(paint, state, opts, newDocument, resizeDialog,
                                                openDialog, saveDialog);

        std::string hoverHint;
        bool overCanvas = false;
        sf::Vector2f canvasCoords{};

        while (const std::optional event = window.pollEvent())
        {
            // Any event (input, hover, focus) can change what is on screen.
            needsRedraw = true;
            if (dialog.open)
            {
                // The modal dialog swallows every input while it is open.
                if (const auto* moved = event->getIf<sf::Event::MouseMoved>())
                {
                    state.mouse = moved->position;
                    ui.mouse = moved->position;
                }
                else if (const auto* pressed = event->getIf<sf::Event::MouseButtonPressed>())
                {
                    state.mouse = pressed->position;
                    ui.mouse = pressed->position;
                    const sf::Vector2f point(static_cast<float>(pressed->position.x),
                                             static_cast<float>(pressed->position.y));

                    if (dialog.confirmButton.contains(point))
                    {
                        (void)confirmDialog();
                    }
                    else if (dialog.cancelButton.contains(point))
                    {
                        dialog.open = false;
                    }
                    else if (dialog.kind == DialogKind::Resize)
                    {
                        if (dialog.widthField.contains(point))
                        {
                            dialog.activeField = 1;
                        }
                        else if (dialog.heightField.contains(point))
                        {
                            dialog.activeField = 2;
                        }
                    }
                    else if (dialog.pathField.contains(point))
                    {
                        dialog.activeField = 0;
                    }
                    else if (dialog.kind == DialogKind::Open &&
                             dialog.listBounds.contains(point) &&
                             !dialog.entries.empty())
                    {
                        const float rowHeight = 19.0F;
                        const std::size_t visible = static_cast<std::size_t>(
                            std::max(1.0F, dialog.listBounds.size.y / rowHeight));
                        const std::size_t first = dialog.selected < visible
                                                      ? 0U
                                                      : std::min(dialog.selected - visible + 1U,
                                                                 dialog.entries.size());
                        const auto row = static_cast<std::size_t>(
                            std::floor((point.y - dialog.listBounds.position.y) / rowHeight));
                        const std::size_t index = first + row;
                        if (index < dialog.entries.size())
                        {
                            dialog.selected = index;
                            const std::string& entry = dialog.entries[index];
                            if (!entry.empty() && entry.front() == '[')
                            {
                                const std::filesystem::path directory =
                                    std::filesystem::path(dialog.path).parent_path().empty()
                                        ? std::filesystem::path(entry.substr(1, entry.size() - 2))
                                        : std::filesystem::path(dialog.path).parent_path() /
                                              entry.substr(1, entry.size() - 2);
                                refreshDirectory(dialog, directory);
                            }
                            else
                            {
                                dialog.path = entry;
                            }
                        }
                    }
                }
                else if (const auto* wheel = event->getIf<sf::Event::MouseWheelScrolled>())
                {
                    if (dialog.kind == DialogKind::Open && !dialog.entries.empty() &&
                        wheel->delta != 0.0F)
                    {
                        if (wheel->delta > 0.0F)
                        {
                            dialog.selected = dialog.selected > 0U ? dialog.selected - 1U : 0U;
                        }
                        else
                        {
                            dialog.selected = std::min(dialog.selected + 1U,
                                                      dialog.entries.size() - 1U);
                        }
                    }
                }
                else if (const auto* key = event->getIf<sf::Event::KeyPressed>())
                {
                    if (key->code == sf::Keyboard::Key::Escape)
                    {
                        dialog.open = false;
                    }
                    else if (key->code == sf::Keyboard::Key::Enter)
                    {
                        (void)confirmDialog();
                    }
                    else if (key->code == sf::Keyboard::Key::Backspace)
                    {
                        std::string* field = dialog.activeField == 0 ? &dialog.path
                                      : dialog.activeField == 1 ? &dialog.width
                                                               : &dialog.height;
                        if (!field->empty())
                        {
                            field->pop_back();
                        }
                    }
                }
                else if (event->is<sf::Event::TextEntered>())
                {
                    const auto* entered = event->getIf<sf::Event::TextEntered>();
                    if (entered && entered->unicode >= 32U && entered->unicode != 127U)
                    {
                        std::string* field = dialog.activeField == 0 ? &dialog.path
                                      : dialog.activeField == 1 ? &dialog.width
                                                               : &dialog.height;
                        appendUtf8(*field, entered->unicode);
                    }
                }
                continue;
            }

            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }
            else if (const auto* resized = event->getIf<sf::Event::Resized>())
            {
                (void)resized;
                updateCanvasViewport(window.getSize(), state.canvasViewport, paint, state.zoom);
            }
            else if (const auto* moved = event->getIf<sf::Event::MouseMoved>())
            {
                state.mouse = moved->position;
                ui.mouse = moved->position;

                if (state.zoomDragging || state.brushDragging)
                {
                    if (state.zoomDragging)
                    {
                        const auto track = zoomTrackBounds(window.getSize().x,
                                                          statusTop(window.getSize().y),
                                                          kStatusHeight);
                        const float t = std::clamp(
                            (static_cast<float>(moved->position.x) - track.position.x) / track.size.x,
                            0.0F, 1.0F);
                        state.zoom = 0.25F + t * (8.0F - 0.25F);
                    }
                    else
                    {
                        const float fraction = std::clamp(
                            (static_cast<float>(moved->position.x) - kBrushSliderX) / kBrushSliderWidth,
                            0.0F, 1.0F);
                        paint.setBrushSize(1.0F + fraction * 127.0F);
                    }
                }

                const auto canvasPos = screenToCanvas(moved->position, state.canvasViewport, paint.getSize());
                overCanvas = state.canvasViewport.contains(
                    {static_cast<float>(moved->position.x), static_cast<float>(moved->position.y)});
                canvasCoords = canvasPos;
                paint.handleEvent(*event, state.canvasViewport);
            }
            else if (const auto* pressed = event->getIf<sf::Event::MouseButtonPressed>())
            {
                state.mouse = pressed->position;
                ui.mouse = pressed->position;
                const sf::Vector2f point(static_cast<float>(pressed->position.x),
                                         static_cast<float>(pressed->position.y));
                const auto panel = state.layerPanelOpen ? std::optional<LayerPanel>(computeLayerPanel(window.getSize(), paint))
                                                        : std::nullopt;
                const float barTop = paletteTop(window.getSize().y);
                const sf::FloatRect paletteBar{{0.0F, barTop},
                                               {static_cast<float>(window.getSize().x), kPaletteHeight}};
                const float statusY = statusTop(window.getSize().y);

                if (pressed->button == sf::Mouse::Button::Left && panel && panel->bounds.contains(point))
                {
                    bool consumed = false;
                    for (const LayerPanel::Row& row : panel->rows)
                    {
                        if (row.eye.contains(point))
                        {
                            paint.setLayerVisibility(row.index, !paint.getLayerVisibility(row.index));
                            consumed = true;
                            break;
                        }
                        if (row.lock.contains(point))
                        {
                            paint.setLayerLocked(row.index, !paint.getLayerLocked(row.index));
                            consumed = true;
                            break;
                        }
                        if (row.name.contains(point) || row.row.contains(point))
                        {
                            paint.setActiveLayer(row.index);
                            consumed = true;
                            break;
                        }
                    }

                    if (!consumed && panel->addButton.contains(point))
                    {
                        (void)paint.addLayer("Camada " + std::to_string(paint.getLayerCount() + 1U),
                                             sf::Color::Transparent);
                        consumed = true;
                    }
                    else if (!consumed && panel->removeButton.contains(point) && paint.getLayerCount() > 1U)
                    {
                        (void)paint.removeLayer(paint.getActiveLayerIndex());
                        consumed = true;
                    }

                    if (consumed)
                    {
                        continue;
                    }
                }

                // Status bar zoom interactions.
                if (point.y >= statusY)
                {
                    const unsigned int w = window.getSize().x;
                    if (zoomMinusBounds(w, statusY, kStatusHeight).contains(point))
                    {
                        state.zoom = std::max(0.25F, state.zoom / 1.25F);
                        continue;
                    }
                    if (zoomPlusBounds(w, statusY, kStatusHeight).contains(point))
                    {
                        state.zoom = std::min(8.0F, state.zoom * 1.25F);
                        continue;
                    }
                    if (pressed->button == sf::Mouse::Button::Left)
                    {
                        const auto track = zoomTrackBounds(w, statusY, kStatusHeight);
                        if (track.contains(point))
                        {
                            const float t = std::clamp((point.x - track.position.x) / track.size.x, 0.0F, 1.0F);
                            state.zoom = 0.25F + t * (8.0F - 0.25F);
                            state.zoomDragging = true;
                            continue;
                        }
                    }
                    continue;
                }

                // Row 0: quick access toolbar.
                if (point.y < kQuickAccessHeight && pressed->button == sf::Mouse::Button::Left)
                {
                    bool consumed = false;
                    for (const IconButton& button : quickButtons)
                    {
                        if (button.enabled && button.bounds.contains(point))
                        {
                            if (button.action)
                            {
                                button.action();
                            }
                            consumed = true;
                            break;
                        }
                    }
                    if (consumed)
                    {
                        continue;
                    }
                }

                // Row 1: File button and tabs ("Arquivo", "Início", "Exibir").
                if (point.y >= kQuickAccessHeight && point.y < kRibbonContentTop &&
                    pressed->button == sf::Mouse::Button::Left)
                {
                    const sf::FloatRect fileMenuBtn{{8.0F, kQuickAccessHeight + 1.0F}, {62.0F, 28.0F}};
                    const sf::FloatRect homeTab{{74.0F, kQuickAccessHeight}, {90.0F, kTabHeight}};
                    const sf::FloatRect viewTab{{168.0F, kQuickAccessHeight}, {70.0F, kTabHeight}};

                    if (fileMenuBtn.contains(point))
                    {
                        openDialog();
                        continue;
                    }
                    if (homeTab.contains(point))
                    {
                        state.activeTab = "home";
                        continue;
                    }
                    if (viewTab.contains(point))
                    {
                        state.activeTab = "view";
                        continue;
                    }
                }

                // Ribbon content interactions (Buttons, Colors, Brush size, Shape fill).
                if (point.y >= kRibbonContentTop && point.y < kRibbonHeight)
                {
                    if (state.activeTab == "home")
                    {
                        // Color 1 / Color 2 box selection
                        const sf::FloatRect box1Rect{{kColor1BoxX, kColor1BoxY}, {38.0F, 50.0F}};
                        const sf::FloatRect box2Rect{{kColor2BoxX, kColor2BoxY}, {38.0F, 50.0F}};
                        if (box1Rect.contains(point))
                        {
                            state.activeColorSlot = 1;
                            continue;
                        }
                        if (box2Rect.contains(point))
                        {
                            state.activeColorSlot = 2;
                            continue;
                        }

                        // Swatches in Ribbon
                        const auto swatchBounds = paletteSwatchBounds();
                        bool hitSwatch = false;
                        for (std::size_t index = 0; index < swatchBounds.size() && index < paint.getPalette().size(); ++index)
                        {
                            if (swatchBounds[index].contains(point))
                            {
                                if (pressed->button == sf::Mouse::Button::Left)
                                {
                                    if (state.activeColorSlot == 1)
                                    {
                                        paint.selectPaletteColor(index);
                                    }
                                    else
                                    {
                                        paint.setSecondaryColor(kPaintPalette[index]);
                                    }
                                }
                                else if (pressed->button == sf::Mouse::Button::Right)
                                {
                                    paint.setSecondaryColor(kPaintPalette[index]);
                                }
                                hitSwatch = true;
                                break;
                            }
                        }
                        if (hitSwatch)
                        {
                            continue;
                        }

                        // Edit colors button
                        const sf::FloatRect editColorsBtn{{kPaletteStartX + 14.0F * 18.0F + 8.0F, kPaletteStartY}, {36.0F, 50.0F}};
                        if (editColorsBtn.contains(point))
                        {
                            // Cycle to a rich custom vibrant accent color
                            static std::size_t editIdx = 0;
                            const sf::Color vibrantCols[] = {
                                sf::Color(255, 0, 128), sf::Color(0, 255, 200), sf::Color(255, 128, 0),
                                sf::Color(128, 0, 255), sf::Color(0, 200, 0), sf::Color(255, 215, 0)
                            };
                            const sf::Color chosen = vibrantCols[(editIdx++) % 6];
                            if (state.activeColorSlot == 1)
                            {
                                paint.setPrimaryColor(chosen);
                            }
                            else
                            {
                                paint.setSecondaryColor(chosen);
                            }
                            continue;
                        }

                        // Brush size slider
                        const sf::FloatRect brushTrack{{kBrushSliderX - 4.0F, kBrushSliderY + 8.0F},
                                                       {kBrushSliderWidth + 8.0F, 16.0F}};
                        if (brushTrack.contains(point) && pressed->button == sf::Mouse::Button::Left)
                        {
                            const float fraction = std::clamp(
                                (point.x - kBrushSliderX) / kBrushSliderWidth, 0.0F, 1.0F);
                            paint.setBrushSize(1.0F + fraction * 127.0F);
                            state.brushDragging = true;
                            continue;
                        }

                        // Fill shapes checkbox
                        const sf::FloatRect checkbox{{kFillCheckboxX - 4.0F, kFillCheckboxY - 4.0F}, {80.0F, 22.0F}};
                        if (checkbox.contains(point) && pressed->button == sf::Mouse::Button::Left)
                        {
                            paint.setFillShapes(!paint.getFillShapes());
                            continue;
                        }
                    }

                    if (pressed->button == sf::Mouse::Button::Left)
                    {
                        bool consumed = false;
                        for (const IconButton& button : buttons)
                        {
                            if (button.enabled && button.bounds.contains(point))
                            {
                                if (button.action)
                                {
                                    button.action();
                                }
                                consumed = true;
                                break;
                            }
                        }
                        if (consumed)
                        {
                            continue;
                        }
                    }
                }

                // Text tool placement.
                if (pressed->button == sf::Mouse::Button::Left &&
                    paint.getTool() == minipaint::ToolType::Text &&
                    state.canvasViewport.contains(point))
                {
                    textEntryActive = true;
                    textEntry.clear();
                    textEntryPosition = screenToCanvas(pressed->position, state.canvasViewport,
                                                        paint.getSize());
                    transientHint = "Escreva o texto e prima Enter";
                    hintOverrideUntil = std::chrono::steady_clock::now() + std::chrono::seconds(3);
                    continue;
                }

                paint.handleEvent(*event, state.canvasViewport);
            }
            else if (const auto* released = event->getIf<sf::Event::MouseButtonReleased>())
            {
                state.mouse = released->position;
                ui.mouse = released->position;
                state.zoomDragging = false;
                state.brushDragging = false;
                paint.handleEvent(*event, state.canvasViewport);
            }
            else if (const auto* wheel = event->getIf<sf::Event::MouseWheelScrolled>())
            {
                if (wheel->wheel == sf::Mouse::Wheel::Vertical)
                {
                    state.zoom = std::clamp(state.zoom * (wheel->delta > 0.0F ? 1.2F : 1.0F / 1.2F),
                                            0.25F, 8.0F);
                }
            }
            else if (const auto* key = event->getIf<sf::Event::KeyPressed>())
            {
                if (textEntryActive)
                {
                    if (key->code == sf::Keyboard::Key::Escape)
                    {
                        textEntryActive = false;
                        textEntry.clear();
                        transientHint = "Entrada de texto cancelada";
                        hintOverrideUntil = std::chrono::steady_clock::now() + std::chrono::seconds(3);
                    }
                    else if (key->code == sf::Keyboard::Key::Enter)
                    {
                        if (paint.drawText(ui.font, textEntry, textEntryPosition))
                        {
                            transientHint = "Texto inserido";
                        }
                        else
                        {
                            transientHint = "Não foi possível inserir o texto";
                        }
                        hintOverrideUntil = std::chrono::steady_clock::now() + std::chrono::seconds(3);
                        textEntryActive = false;
                        textEntry.clear();
                    }
                    else if (key->code == sf::Keyboard::Key::Backspace)
                    {
                        if (!textEntry.empty())
                        {
                            textEntry.pop_back();
                        }
                    }
                    continue;
                }

                else if (key->control && key->code == sf::Keyboard::Key::Num0)
                {
                    state.zoom = 1.0F;
                }
                else if (key->control && key->code == sf::Keyboard::Key::S)
                {
                    saveDialog();
                }
                else if (key->control && key->code == sf::Keyboard::Key::O)
                {
                    openDialog();
                }
                else if (key->control && key->code == sf::Keyboard::Key::N)
                {
                    newDocument();
                }
                else if (key->control && key->code == sf::Keyboard::Key::G)
                {
                    state.gridVisible = !state.gridVisible;
                }
                else if (key->control && key->code == sf::Keyboard::Key::Z && !key->shift)
                {
                    (void)paint.undo();
                }
                else if (key->control &&
                         (key->code == sf::Keyboard::Key::Y ||
                          (key->code == sf::Keyboard::Key::Z && key->shift)))
                {
                    (void)paint.redo();
                }
                else if (!key->control && key->code == sf::Keyboard::Key::H)
                {
                    (void)paint.flipActiveLayerHorizontal();
                }
                else if (!key->control && key->code == sf::Keyboard::Key::V)
                {
                    (void)paint.flipActiveLayerVertical();
                }
                else if (!key->control && key->code == sf::Keyboard::Key::R)
                {
                    (void)paint.rotateActiveLayer(true);
                }
                else if (!key->control &&
                         (key->code == sf::Keyboard::Key::Equal ||
                          key->code == sf::Keyboard::Key::Add))
                {
                    state.zoom = std::min(8.0F, state.zoom * 1.25F);
                }
                else if (!key->control &&
                         (key->code == sf::Keyboard::Key::Hyphen ||
                          key->code == sf::Keyboard::Key::Subtract))
                {
                    state.zoom = std::max(0.25F, state.zoom / 1.25F);
                }
                else if (key->code == sf::Keyboard::Key::Escape)
                {
                    window.close();
                }
                else if (key->code >= sf::Keyboard::Key::Num1 &&
                         key->code <= sf::Keyboard::Key::Num9)
                {
                    const auto index = static_cast<std::size_t>(
                        static_cast<int>(key->code) -
                        static_cast<int>(sf::Keyboard::Key::Num1));
                    paint.setTool(kTools[index]);
                    transientHint = "Ferramenta: " + std::string(minipaint::toolName(kTools[index]));
                    hintOverrideUntil = std::chrono::steady_clock::now() + std::chrono::seconds(3);
                }
            }
            else if (textEntryActive && event->is<sf::Event::TextEntered>())
            {
                const auto* entered = event->getIf<sf::Event::TextEntered>();
                if (entered && entered->unicode >= 32U && entered->unicode != 127U)
                {
                    appendUtf8(textEntry, entered->unicode);
                }
            }
        }

        // ------------------------------------------------- hover hints --
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        if (now < hintOverrideUntil)
        {
            hoverHint = transientHint;
        }
        else
        {
            hoverHint = std::string("Ferramenta: ") + std::string(minipaint::toolName(paint.getTool()));
            const auto hoverOver = [&](const IconButton& button) {
                return button.bounds.contains(
                    {static_cast<float>(state.mouse.x), static_cast<float>(state.mouse.y)});
            };
            for (const IconButton& button : quickButtons)
            {
                if (hoverOver(button))
                {
                    hoverHint = button.hint;
                    break;
                }
            }
            for (const IconButton& button : buttons)
            {
                if (button.bounds.contains(
                        {static_cast<float>(state.mouse.x), static_cast<float>(state.mouse.y)}))
                {
                    hoverHint = button.hint;
                    break;
                }
            }
        }

        // ----------------------------------------------------- drawing --
        // Cursor: I-beam in text entry, crosshair over the canvas, arrow elsewhere.
        if (textEntryActive && iBeamCursor.has_value())
        {
            window.setMouseCursor(*iBeamCursor);
        }
        else if (overCanvas && crossCursor.has_value())
        {
            window.setMouseCursor(*crossCursor);
        }
        else if (arrowCursor.has_value())
        {
            window.setMouseCursor(*arrowCursor);
        }

        if (!needsRedraw)
        {
            // Nothing changed since the last presented frame: skip the entire
            // UI/canvas draw (and its double-buffer blit) this iteration.
            continue;
        }

        window.clear(kWindowBg);

        // Top bar background (quick access + tabs + ribbon content).
        sf::RectangleShape topBar({static_cast<float>(window.getSize().x), kRibbonHeight});
        topBar.setFillColor(kRibbonBg);
        window.draw(topBar);
        sf::RectangleShape topBorder({static_cast<float>(window.getSize().x), 1.0F});
        topBorder.setPosition({0.0F, kRibbonHeight - 1.0F});
        topBorder.setFillColor(kDivider);
        window.draw(topBorder);
        sf::RectangleShape accessDivider({static_cast<float>(window.getSize().x), 1.0F});
        accessDivider.setPosition({0.0F, kQuickAccessHeight - 0.5F});
        accessDivider.setFillColor(kDivider);
        window.draw(accessDivider);
        sf::RectangleShape tabDivider({static_cast<float>(window.getSize().x), 1.0F});
        tabDivider.setPosition({0.0F, kRibbonContentTop - 0.5F});
        tabDivider.setFillColor(kDivider);
        window.draw(tabDivider);

        // Row 0: quick access toolbar (paint palette icon, save/undo/redo, filename).
        drawIcon(window, "palette", {18.0F, 15.0F}, 22.0F, sf::Color::White);
        for (const IconButton& button : quickButtons)
        {
            drawButton(window, ui, button, 16.0F);
        }
        if (ui.hasFont)
        {
            const std::string docName = state.currentFile.empty()
                ? std::string("Sem título")
                : std::filesystem::path(state.currentFile).filename().string();
            const std::string windowTitle = docName + " - Paint";
            sf::Text titleLabel(ui.font, utf8(windowTitle), 12U);
            const auto tb = titleLabel.getLocalBounds();
            titleLabel.setPosition({(static_cast<float>(window.getSize().x) - tb.size.x) * 0.5F, 7.0F});
            titleLabel.setFillColor(kStatusText);
            window.draw(titleLabel);
        }

        // Row 1: File button and tabs ("Arquivo", "Início", "Exibir").
        // "Arquivo" button (Classic MS Paint blue accent button)
        const sf::FloatRect fileBtnRect{{8.0F, kQuickAccessHeight + 2.0F}, {60.0F, 26.0F}};
        const bool fileHovered = fileBtnRect.contains({static_cast<float>(ui.mouse.x), static_cast<float>(ui.mouse.y)});
        sf::RectangleShape fileBtn(fileBtnRect.size);
        fileBtn.setPosition(fileBtnRect.position);
        fileBtn.setFillColor(fileHovered ? sf::Color(40, 135, 220) : kAccent);
        window.draw(fileBtn);
        if (ui.hasFont)
        {
            drawText(window, ui.font, "Arquivo", {fileBtnRect.position.x + 10.0F, fileBtnRect.position.y + 6.0F},
                     12U, sf::Color::White);
        }

        const auto drawTab = [&](const sf::FloatRect& rect, const std::string& label,
                                 const bool active) {
            const bool hovered = rect.contains({static_cast<float>(ui.mouse.x),
                                                static_cast<float>(ui.mouse.y)});
            if (active)
            {
                sf::RectangleShape tabBg({rect.size.x, rect.size.y + 1.0F});
                tabBg.setPosition(rect.position);
                tabBg.setFillColor(sf::Color::White);
                tabBg.setOutlineColor(kDivider);
                tabBg.setOutlineThickness(1.0F);
                window.draw(tabBg);
            }
            else if (hovered)
            {
                sf::RectangleShape hover(rect.size);
                hover.setPosition(rect.position);
                hover.setFillColor(kHoverFill);
                window.draw(hover);
            }
            if (ui.hasFont)
            {
                sf::Text tab(ui.font, utf8(label), 12U);
                const auto bounds = tab.getLocalBounds();
                tab.setPosition({rect.position.x + (rect.size.x - bounds.size.x) * 0.5F,
                                 rect.position.y + (rect.size.y - bounds.size.y) * 0.5F - 2.0F});
                tab.setFillColor(active ? sf::Color(20, 20, 20) : kCaptionText);
                window.draw(tab);
            }
        };
        drawTab({{72.0F, kQuickAccessHeight}, {84.0F, kTabHeight}}, "Início", state.activeTab == "home");
        drawTab({{158.0F, kQuickAccessHeight}, {68.0F, kTabHeight}}, "Exibir", state.activeTab == "view");

        // Row 2: ribbon content (groups & tools per active tab).
        if (state.activeTab == "view")
        {
            drawRibbonGroup(window, ui, 6.0F, 120.0F, "Zoom");
            drawRibbonGroup(window, ui, 130.0F, 110.0F, "Mostrar");
        }
        else
        {
            drawRibbonGroup(window, ui, 6.0F, 144.0F, "Imagem");
            drawRibbonGroup(window, ui, 154.0F, 106.0F, "Ferramentas");
            drawRibbonGroup(window, ui, 264.0F, 78.0F, "Pincéis");
            drawRibbonGroup(window, ui, 346.0F, 114.0F, "Formas");
            drawRibbonGroup(window, ui, 464.0F, 104.0F, "Tamanho");
            drawRibbonGroup(window, ui, 572.0F, 404.0F, "Cores");
            drawRibbonGroup(window, ui, 980.0F, 180.0F, "Camadas & Ações");

            // Shapes fill checkbox
            drawShapesSection(window, ui, paint, hoverHint);
            // Size buttons & brush slider
            drawSizeSection(window, ui, paint, hoverHint);
            // Authentic MS Paint Colors (Cor 1, Cor 2, 28 swatches, edit colors)
            drawRibbonColors(window, ui, paint, state, hoverHint);
        }

        for (const IconButton& button : buttons)
        {
            drawButton(window, ui, button, 22.0F);
        }

        // Canvas area with checkerboard and shadow.
        const sf::FloatRect viewport = state.canvasViewport;
        sf::RectangleShape shadowOuter({viewport.size.x + 8.0F, viewport.size.y + 8.0F});
        shadowOuter.setPosition({viewport.position.x + 3.0F, viewport.position.y + 3.0F});
        shadowOuter.setFillColor(sf::Color(0, 0, 0, 20));
        window.draw(shadowOuter);
        sf::RectangleShape shadowInner(viewport.size);
        shadowInner.setPosition({viewport.position.x + 1.0F, viewport.position.y + 1.0F});
        shadowInner.setFillColor(sf::Color(0, 0, 0, 30));
        window.draw(shadowInner);

        drawCheckerboard(window, viewport);
        paint.draw(window, viewport);

        // Optional grid lines (Ctrl+G or the "Exibir" tab).
        if (state.gridVisible)
        {
            const float scale = viewport.size.x / static_cast<float>(paint.getSize().x);
            if (scale >= 0.5F)
            {
                const sf::Color grid(0, 0, 0, 28);
                sf::VertexArray lines(sf::PrimitiveType::Lines);
                const float step = 8.0F * scale;
                for (float x = viewport.position.x; x <= viewport.position.x + viewport.size.x; x += step)
                {
                    lines.append({{x, viewport.position.y}, grid});
                    lines.append({{x, viewport.position.y + viewport.size.y}, grid});
                }
                for (float y = viewport.position.y; y <= viewport.position.y + viewport.size.y; y += step)
                {
                    lines.append({{viewport.position.x, y}, grid});
                    lines.append({{viewport.position.x + viewport.size.x, y}, grid});
                }
                window.draw(lines);
            }
        }

        sf::RectangleShape canvasBorder(viewport.size);
        canvasBorder.setPosition(viewport.position);
        canvasBorder.setFillColor(sf::Color::Transparent);
        canvasBorder.setOutlineColor(sf::Color(160, 165, 175));
        canvasBorder.setOutlineThickness(1.0F);
        window.draw(canvasBorder);

        // MS Paint Canvas Resize Handles (right, bottom, bottom-right corner)
        const sf::Vector2f handles[3] = {
            {viewport.position.x + viewport.size.x, viewport.position.y + viewport.size.y * 0.5F},
            {viewport.position.x + viewport.size.x * 0.5F, viewport.position.y + viewport.size.y},
            {viewport.position.x + viewport.size.x, viewport.position.y + viewport.size.y}
        };
        for (const auto& hpos : handles)
        {
            sf::RectangleShape handle({6.0F, 6.0F});
            handle.setOrigin({3.0F, 3.0F});
            handle.setPosition(hpos);
            handle.setFillColor(sf::Color::White);
            handle.setOutlineColor(sf::Color(100, 105, 115));
            handle.setOutlineThickness(1.0F);
            window.draw(handle);
        }

        // Text entry preview.
        if (textEntryActive && ui.hasFont)
        {
            const float scale = viewport.size.x / static_cast<float>(paint.getSize().x);
            const sf::Vector2f screenPosition{viewport.position.x + textEntryPosition.x * scale,
                                              viewport.position.y + textEntryPosition.y * scale};
            sf::Text preview(ui.font, utf8(textEntry.empty() ? "|" : textEntry + "|"), 24);
            preview.setPosition(screenPosition);
            preview.setFillColor(paint.getPrimaryColor());
            window.draw(preview);
            sf::RectangleShape caret({2.0F, 24.0F});
            caret.setPosition({screenPosition.x + preview.findCharacterPos(textEntry.size()).x + 1.0F,
                               screenPosition.y});
            caret.setFillColor(paint.getPrimaryColor());
            window.draw(caret);
        }

        // Brush cursor preview when hovering over the canvas.
        if (overCanvas)
        {
            const float scale = viewport.size.x / static_cast<float>(paint.getSize().x);
            const float brushRadius = paint.getBrushSize() * 0.5F * scale;
            const sf::Vector2f center{static_cast<float>(state.mouse.x),
                                      static_cast<float>(state.mouse.y)};

            sf::CircleShape outline(brushRadius);
            outline.setPosition({center.x - brushRadius, center.y - brushRadius});
            outline.setFillColor(sf::Color(paint.getPrimaryColor().r,
                                           paint.getPrimaryColor().g,
                                           paint.getPrimaryColor().b, 40));
            outline.setOutlineColor(paint.getPrimaryColor());
            outline.setOutlineThickness(1.5F);
            window.draw(outline);
        }

        // Palette is now integrated in the Ribbon (no footer palette bar).

        // Layer panel overlay.
        if (state.layerPanelOpen)
        {
            const LayerPanel panel = computeLayerPanel(window.getSize(), paint);
            drawLayerPanel(window, ui, panel, paint);
        }

        // Status bar.
        const float statusY = statusTop(window.getSize().y);
        const sf::FloatRect statusBar{{0.0F, statusY},
                                      {static_cast<float>(window.getSize().x), kStatusHeight}};

        std::string canvasText = std::to_string(paint.getSize().x) + " × " +
                                 std::to_string(paint.getSize().y) +
                                 " pixels";

        if (overCanvas)
        {
            const int cx = static_cast<int>(std::floor(canvasCoords.x));
            const int cy = static_cast<int>(std::floor(canvasCoords.y));
            if (cx >= 0 && cy >= 0 && cx < static_cast<int>(paint.getSize().x) &&
                cy < static_cast<int>(paint.getSize().y))
            {
                hoverHint = std::to_string(cx) + ", " + std::to_string(cy);
            }
        }

        drawStatusBar(window, ui, statusBar, hoverHint, canvasText, state.zoom);

        if (dialog.open)
        {
            drawDialog(window, ui, dialog);
        }

        window.display();
        needsRedraw = false;
    }

    if (!opts.output.empty() && !state.currentFile.empty() &&
        state.currentFile != opts.output.string())
    {
        if (!paint.saveToFile(opts.output))
        {
            std::cerr << "MiniPaint: nao foi possivel guardar '" << opts.output.string() << "'.\n";
        }
    }

    std::cout << "MiniPaint example finished.\n";
    return 0;
}
