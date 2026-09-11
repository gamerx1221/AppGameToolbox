#pragma once

#include "Render2D.hpp"

#include <functional>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace appgametoolbox {

using HtmlCssNodeId = std::uint64_t;

enum class CssPseudoState : std::uint8_t {
    Hover = 1 << 0,
    Active = 1 << 1,
    Focus = 1 << 2,
};

struct CssCompatibilityWarning {
    std::string subject;
    std::string message;
};

// A deterministic HTML/CSS subset that records 2D drawing commands. It supports
// element, class, ID, descendant, and limited pseudo-state selectors; inline
// styles; block and absolute layout; colors, opacity, text, images, and cached
// layers. It is not a browser engine.
class HtmlCssPipeline {
public:
    using ImageResolver = std::function<Render2DResourceId(const std::string& source)>;

    HtmlCssPipeline();
    ~HtmlCssPipeline();
    HtmlCssPipeline(const HtmlCssPipeline&) = delete;
    HtmlCssPipeline& operator=(const HtmlCssPipeline&) = delete;
    HtmlCssPipeline(HtmlCssPipeline&&) noexcept;
    HtmlCssPipeline& operator=(HtmlCssPipeline&&) noexcept;

    // CSS from <style> elements is combined with this stylesheet.
    bool load(std::string html, std::string css = {});
    void setImageResolver(ImageResolver resolver);
    // Sets the deterministic animation clock used by transition and keyframe CSS.
    void setTime(double seconds);

    // Runtime state is intentionally retained by the pipeline, while playback
    // remains immutable. Call record() again after changing state.
    std::optional<HtmlCssNodeId> nodeIdForElementId(const std::string& elementId) const;
    bool setPseudoState(HtmlCssNodeId node, CssPseudoState state, bool enabled);
    std::optional<HtmlCssNodeId> hitTest(Point point) const;

    // Appends the document's resolved layout to recorder. Call finish() on the
    // recorder after all producers have emitted their commands.
    bool record(Render2DRecorder& recorder, const Size& viewport);
    const std::string& lastError() const;
    const std::vector<CssCompatibilityWarning>& compatibilityWarnings() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace appgametoolbox
