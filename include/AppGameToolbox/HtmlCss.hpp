#pragma once

#include "Render2D.hpp"

#include <functional>
#include <memory>
#include <string>

namespace appgametoolbox {

// A deterministic HTML/CSS subset that records 2D drawing commands. It supports
// element, class, and ID selectors; inline styles; block and absolute layout;
// colors, opacity, text, images, and cached layers. It is not a browser engine.
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

    // Appends the document's resolved layout to recorder. Call finish() on the
    // recorder after all producers have emitted their commands.
    bool record(Render2DRecorder& recorder, const Size& viewport);
    const std::string& lastError() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace appgametoolbox
