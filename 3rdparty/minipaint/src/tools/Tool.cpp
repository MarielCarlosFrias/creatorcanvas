#include "Tool.hpp"

#include "tools/BucketFillTool.hpp"
#include "tools/AirbrushTool.hpp"
#include "tools/EyedropperTool.hpp"
#include "tools/EraserTool.hpp"
#include "tools/EllipseTool.hpp"
#include "tools/LineTool.hpp"
#include "tools/PencilTool.hpp"
#include "tools/RectangleTool.hpp"
#include "tools/TextTool.hpp"

namespace minipaint
{
std::unique_ptr<Tool> makeTool(const ToolType type)
{
    switch (type)
    {
        case ToolType::Pencil:
            return std::make_unique<PencilTool>();
        case ToolType::Eraser:
            return std::make_unique<EraserTool>();
        case ToolType::Line:
            return std::make_unique<LineTool>();
        case ToolType::Rectangle:
            return std::make_unique<RectangleTool>();
        case ToolType::Ellipse:
            return std::make_unique<EllipseTool>();
        case ToolType::BucketFill:
            return std::make_unique<BucketFillTool>();
        case ToolType::Eyedropper:
            return std::make_unique<EyedropperTool>();
        case ToolType::Airbrush:
            return std::make_unique<AirbrushTool>();
        case ToolType::Text:
            return std::make_unique<TextTool>();
    }

    return std::make_unique<PencilTool>();
}

std::string_view toolName(const ToolType type)
{
    switch (type)
    {
        case ToolType::Pencil:
            return "Pencil";
        case ToolType::Eraser:
            return "Eraser";
        case ToolType::Line:
            return "Line";
        case ToolType::Rectangle:
            return "Rectangle";
        case ToolType::Ellipse:
            return "Ellipse";
        case ToolType::BucketFill:
            return "Bucket Fill";
        case ToolType::Eyedropper:
            return "Eyedropper";
        case ToolType::Airbrush:
            return "Airbrush";
        case ToolType::Text:
            return "Text";
    }

    return "Unknown";
}
} // namespace minipaint
