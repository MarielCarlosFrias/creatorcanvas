#pragma once

#include "PixelRun.hpp"
#include "commands/Command.hpp"

#include "Layer.hpp"

#include <SFML/Graphics/Image.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

namespace minipaint
{
namespace detail
{
/// Stores sparse runs of changed pixels (same-size edits) or full before/after
/// images (size-changing edits). Holds the layer stack by reference and the
/// layer index, so commands survive reallocation/insertion/removal of layers.
/// If the layer no longer exists at execute/undo time the command is a no-op.
class ImageCommand final : public Command
{
public:
    /// Creates a command when before and after differ; otherwise returns nullptr.
    [[nodiscard]] static std::unique_ptr<ImageCommand> create(std::vector<Layer>& layers,
                                                              std::size_t layerIndex,
                                                              const sf::Image& before,
                                                              const sf::Image& after);

    ImageCommand(std::vector<Layer>& layers,
                 std::size_t layerIndex,
                 std::vector<PixelRun> runs,
                 std::optional<sf::Image> beforeImage,
                 std::optional<sf::Image> afterImage);

    /// Remaps the stored layer index after a layer was removed. Commands that
    /// targeted the removed layer are disabled (they become no-ops).
    void notifyLayerRemoved(std::size_t removedIndex);

    void execute() override;
    void undo() override;

private:
    bool applyRuns(const std::vector<PixelRun>& runs, bool useAfterColors);

    std::vector<Layer>& m_layers;
    std::size_t m_layerIndex{0U};
    std::vector<PixelRun> m_runs;
    // Full images are stored for size-changing edits (e.g. layer rotation);
    // when set, they take precedence over the sparse runs.
    std::optional<sf::Image> m_beforeImage;
    std::optional<sf::Image> m_afterImage;
    bool m_disabled{false};
};
} // namespace detail
} // namespace minipaint
