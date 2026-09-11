#include <AppGameToolbox/HtmlCss.hpp>

#include <cassert>
#include <fstream>
#include <iterator>

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

void testJqueryUiSelectorsPseudoStateAndHitTesting() {
    HtmlCssPipeline pipeline;
    assert(pipeline.load(
        "<div class='ui-tabs'><ul class='ui-tabs-nav'><li id='overview' class='ui-tabs-active'>Overview</li></ul></div>",
        ".ui-tabs { --tab-color: #123456; }"
        ".ui-tabs .ui-tabs-nav li.ui-tabs-active { color: var(--tab-color); }"
        "li:hover { font-size: 21px; }"));

    const auto overview = pipeline.nodeIdForElementId("overview");
    assert(overview.has_value());
    Render2DRecorder recorder({300.0, 120.0});
    assert(pipeline.record(recorder, {300.0, 120.0}));
    assert(pipeline.hitTest({10.0, 10.0}) == overview);
    RecordedFrame2D frame;
    assert(recorder.finish(frame));
    bool matchedDescendant = false;
    for (const Render2DCommand& command : frame.commands()) if (const auto* text = std::get_if<DrawText2DCommand>(&command)) {
        if (text->text == "Overview") matchedDescendant = text->color.r > .06f && text->color.g > .19f && text->color.b > .33f && text->fontSize == 16.0;
    }
    assert(matchedDescendant);

    assert(pipeline.setPseudoState(*overview, CssPseudoState::Hover, true));
    recorder.reset({300.0, 120.0});
    assert(pipeline.record(recorder, {300.0, 120.0}));
    assert(recorder.finish(frame));
    bool matchedHover = false;
    for (const Render2DCommand& command : frame.commands()) if (const auto* text = std::get_if<DrawText2DCommand>(&command)) {
        if (text->text == "Overview") matchedHover = text->fontSize == 21.0;
    }
    assert(matchedHover);
}

void testCalcMediaAndCompatibilityWarnings() {
    HtmlCssPipeline pipeline;
    assert(pipeline.load("<div id='card'></div>",
        "#card { width: calc(50% - 10px); background: red; transform: rotate(10deg); }"
        "@media (min-width: 300px) { #card { background: blue; } }"));
    assert(!pipeline.compatibilityWarnings().empty());

    Render2DRecorder recorder({200.0, 100.0});
    assert(pipeline.record(recorder, {200.0, 100.0}));
    RecordedFrame2D frame;
    assert(recorder.finish(frame));
    bool small = false;
    for (const Render2DCommand& command : frame.commands()) if (const auto* fill = std::get_if<FillRect2DCommand>(&command))
        small = fill->rect.size.width == 90.0 && fill->color.r == 1.0f && fill->color.b == 0.0f;
    assert(small);

    recorder.reset({400.0, 100.0});
    assert(pipeline.record(recorder, {400.0, 100.0}));
    assert(recorder.finish(frame));
    bool large = false;
    for (const Render2DCommand& command : frame.commands()) if (const auto* fill = std::get_if<FillRect2DCommand>(&command))
        large = fill->rect.size.width == 190.0 && fill->color.b == 1.0f;
    assert(large);
}

void testBoxDecorationAndBackgroundImageCommands() {
    HtmlCssPipeline pipeline;
    pipeline.setImageResolver([](const std::string& source) { return source == "tile.png" ? 73 : 0; });
    assert(pipeline.load(
        "<div id='gradient'></div><div id='image'></div>",
        "#gradient { width: 20px; height: 10px; background: linear-gradient(to right, #ff0000, #0000ff); "
        "border: 2px solid #00ff00; border-radius: 3px; box-shadow: 1px 2px 4px 0px rgba(0,0,0,0.5); }"
        "#image { width: 8px; height: 6px; background-image: url('tile.png'); }"));

    Render2DRecorder recorder({100.0, 80.0});
    assert(pipeline.record(recorder, {100.0, 80.0}));
    RecordedFrame2D frame;
    assert(recorder.finish(frame));
    bool gradient = false;
    bool border = false;
    bool shadow = false;
    bool image = false;
    for (const Render2DCommand& command : frame.commands()) {
        if (const auto* fill = std::get_if<FillLinearGradient2DCommand>(&command))
            gradient = fill->rect.size.width == 20.0 && fill->startColor.r == 1.0f && fill->endColor.b == 1.0f && fill->radius == 3.0;
        else if (const auto* stroke = std::get_if<StrokeRoundedRect2DCommand>(&command))
            border = stroke->lineWidth == 2.0 && stroke->color.g == 1.0f && stroke->radius == 2.0;
        else if (const auto* boxShadow = std::get_if<DrawBoxShadow2DCommand>(&command))
            shadow = boxShadow->offset.x == 1.0 && boxShadow->offset.y == 2.0 && boxShadow->blurRadius == 4.0 && boxShadow->color.a == 0.5f;
        else if (const auto* background = std::get_if<DrawImage2DCommand>(&command))
            image = background->image == 73 && background->destination.size.width == 8.0 && background->destination.size.height == 6.0;
    }
    assert(gradient);
    assert(border);
    assert(shadow);
    assert(image);
}

void testBoundedFlexAndGridLayout() {
    HtmlCssPipeline pipeline;
    assert(pipeline.load(
        "<div id='flex'><span class='flex-a'></span><span class='flex-b'></span><span class='flex-c'></span></div>"
        "<div id='grid'><span class='grid-a'></span><span class='grid-b'></span><span class='grid-c'></span><span class='grid-d'></span></div>",
        "#flex { display: flex; width: 120px; } .flex-a { flex: 1; height: 10px; background: red; } "
        ".flex-b { flex: 1; height: 10px; background: green; } .flex-c { flex: 1; height: 10px; background: blue; } "
        "#grid { display: grid; width: 120px; grid-template-columns: repeat(3, 1fr); gap: 3px; } "
        ".grid-a { height: 5px; background: #111111; } .grid-b { height: 5px; background: #222222; } "
        ".grid-c { height: 5px; background: #333333; } .grid-d { height: 5px; background: #444444; }"));

    Render2DRecorder recorder({200.0, 100.0});
    assert(pipeline.record(recorder, {200.0, 100.0}));
    RecordedFrame2D frame;
    assert(recorder.finish(frame));
    bool flex = false;
    bool grid = false;
    for (const Render2DCommand& command : frame.commands()) if (const auto* fill = std::get_if<FillRect2DCommand>(&command)) {
        if (fill->color.r == 1.0f) flex = fill->rect.origin.x == 0.0 && fill->rect.size.width == 40.0;
        if (fill->color.g == 0.5f) flex = flex && fill->rect.origin.x == 40.0 && fill->rect.size.width == 40.0;
        if (fill->color.b == 1.0f) flex = flex && fill->rect.origin.x == 80.0 && fill->rect.size.width == 40.0;
        if (fill->color.r == 17.0f / 255.0f) grid = fill->rect.origin.x == 0.0 && fill->rect.origin.y == 10.0 && fill->rect.size.width == 38.0;
        if (fill->color.r == 68.0f / 255.0f) grid = grid && fill->rect.origin.x == 0.0 && fill->rect.origin.y == 18.0 && fill->rect.size.width == 38.0;
    }
    assert(flex);
    assert(grid);
}

void testDeterministicAnimationsAndTransitions() {
    HtmlCssPipeline animation;
    assert(animation.load("<div id='fade'></div>",
        "@keyframes fade { from { opacity: 0; background-color: red; } to { opacity: 1; background-color: blue; } }"
        "#fade { width: 10px; height: 10px; animation: fade 2s linear; }"));
    animation.setTime(1.0);
    Render2DRecorder recorder({40.0, 40.0});
    assert(animation.record(recorder, {40.0, 40.0}));
    RecordedFrame2D frame;
    assert(recorder.finish(frame));
    bool animatedOpacity = false;
    bool animatedBackground = false;
    for (const Render2DCommand& command : frame.commands()) {
        if (const auto* opacity = std::get_if<SetOpacity2DCommand>(&command)) animatedOpacity = animatedOpacity || opacity->opacity == 0.5f;
        if (const auto* fill = std::get_if<FillRect2DCommand>(&command)) animatedBackground = fill->color.r == 0.5f && fill->color.b == 0.5f;
    }
    assert(animatedOpacity);
    assert(animatedBackground);

    HtmlCssPipeline transition;
    assert(transition.load("<div id='button'></div>",
        "#button { width: 10px; height: 10px; background: red; transition: background-color 2s linear; }"
        "#button:hover { background: blue; }"));
    const auto button = transition.nodeIdForElementId("button");
    assert(button.has_value());
    transition.setTime(0.0);
    recorder.reset({40.0, 40.0});
    assert(transition.record(recorder, {40.0, 40.0}));
    assert(recorder.finish(frame));
    assert(transition.setPseudoState(*button, CssPseudoState::Hover, true));
    recorder.reset({40.0, 40.0});
    assert(transition.record(recorder, {40.0, 40.0}));
    assert(recorder.finish(frame));
    transition.setTime(1.0);
    recorder.reset({40.0, 40.0});
    assert(transition.record(recorder, {40.0, 40.0}));
    assert(recorder.finish(frame));
    bool transitionedBackground = false;
    for (const Render2DCommand& command : frame.commands()) if (const auto* fill = std::get_if<FillRect2DCommand>(&command))
        transitionedBackground = fill->color.r == 0.5f && fill->color.b == 0.5f;
    assert(transitionedBackground);
}

void testJqueryUiBaseThemeFixture() {
    std::ifstream input(std::string(APP_GAME_TOOLBOX_FIXTURE_DIR) + "/jquery-ui-1.13.2-base-subset.css");
    assert(input.good());
    const std::string css((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    HtmlCssPipeline pipeline;
    assert(pipeline.load(
        "<div class='ui-widget ui-widget-content ui-tabs'><div class='ui-widget-header'>Gallery</div>"
        "<ul class='ui-tabs-nav'><li id='news' class='ui-state-default ui-tabs-active'>News</li>"
        "<li id='recent' class='ui-state-default'>Recent</li></ul>"
        "<div class='ui-tabs-panel'>Fixture content</div></div>", css));
    assert(pipeline.compatibilityWarnings().empty());
    Render2DRecorder recorder({320.0, 160.0});
    assert(pipeline.record(recorder, {320.0, 160.0}));
    RecordedFrame2D frame;
    assert(recorder.finish(frame));
    bool activeTab = false;
    for (const Render2DCommand& command : frame.commands()) if (const auto* fill = std::get_if<FillRoundedRect2DCommand>(&command))
        activeTab = activeTab || (fill->color.g > 0.49f && fill->color.b > 0.99f);
    assert(activeTab);

    const auto recent = pipeline.nodeIdForElementId("recent");
    assert(recent.has_value());
    assert(pipeline.setPseudoState(*recent, CssPseudoState::Hover, true));
    recorder.reset({320.0, 160.0});
    assert(pipeline.record(recorder, {320.0, 160.0}));
    assert(recorder.finish(frame));
    bool hoveredTab = false;
    for (const Render2DCommand& command : frame.commands()) if (const auto* fill = std::get_if<FillRoundedRect2DCommand>(&command))
        hoveredTab = hoveredTab || (fill->color.r > 0.92f && fill->color.g > 0.92f && fill->color.b > 0.92f);
    assert(hoveredTab);
}

} // namespace

void runHtmlCssTests() {
    testStyledDocumentRecordsCommands();
    testEmbeddedStylesAndAbsoluteLayout();
    testJqueryUiSelectorsPseudoStateAndHitTesting();
    testCalcMediaAndCompatibilityWarnings();
    testBoxDecorationAndBackgroundImageCommands();
    testBoundedFlexAndGridLayout();
    testDeterministicAnimationsAndTransitions();
    testJqueryUiBaseThemeFixture();
}
