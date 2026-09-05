#include <AppGameToolbox/HtmlCss.hpp>

#include <cassert>

namespace {

using namespace appgametoolbox;

void testStyledDocumentRecordsCommands() {
    HtmlCssPipeline pipeline;
    pipeline.setImageResolver([](const std::string& source) { return source == "icon.png" ? 99 : 0; });
    assert(pipeline.load(
        "<div id=panel><span class=title>Hello</span><img src=icon.png></div>",
        "#panel { width: 100px; padding: 4px; background: #123456; render-mode: cached-layer; }"
        ".title { color: red; font-size: 20px; } img { width: 10px; height: 8px; }"));

    Render2DRecorder recorder({320.0, 240.0});
    assert(pipeline.record(recorder, {320.0, 240.0}));
    RecordedFrame2D frame;
    assert(recorder.finish(frame));

    bool hasCache = false;
    bool hasBackground = false;
    bool hasRedText = false;
    bool hasImage = false;
    for (const Render2DCommand& command : frame.commands()) {
        if (const auto* cache = std::get_if<BeginCachedLayer2DCommand>(&command)) {
            hasCache = cache->cacheKey != 0 && cache->contentVersion != 0;
        } else if (const auto* fill = std::get_if<FillRect2DCommand>(&command)) {
            hasBackground = fill->color.r > 0.06f && fill->color.g > 0.19f && fill->color.b > 0.33f;
        } else if (const auto* text = std::get_if<DrawText2DCommand>(&command)) {
            hasRedText = text->text == "Hello" && text->color.r == 1.0f && text->color.g == 0.0f && text->fontSize == 20.0;
        } else if (const auto* image = std::get_if<DrawImage2DCommand>(&command)) {
            hasImage = image->image == 99 && image->destination.size.width == 10.0 && image->destination.size.height == 8.0;
        }
    }
    assert(hasCache);
    assert(hasBackground);
    assert(hasRedText);
    assert(hasImage);
}

void testEmbeddedStylesAndAbsoluteLayout() {
    HtmlCssPipeline pipeline;
    assert(pipeline.load(
        "<style>.note { background: blue; position: absolute; left: 12px; top: 8px; width: 20px; height: 10px; }</style>"
        "<div class=note></div>"));

    Render2DRecorder recorder({100.0, 100.0});
    assert(pipeline.record(recorder, {100.0, 100.0}));
    RecordedFrame2D frame;
    assert(recorder.finish(frame));

    std::size_t fillCount = 0;
    for (const Render2DCommand& command : frame.commands()) {
        if (const auto* fill = std::get_if<FillRect2DCommand>(&command)) {
            ++fillCount;
            assert(fill->rect.origin.x == 12.0);
            assert(fill->rect.origin.y == 8.0);
            assert(fill->rect.size.width == 20.0);
            assert(fill->rect.size.height == 10.0);
        }
    }
    assert(fillCount == 1);
}

} // namespace

void runHtmlCssTests() {
    testStyledDocumentRecordsCommands();
    testEmbeddedStylesAndAbsoluteLayout();
}
