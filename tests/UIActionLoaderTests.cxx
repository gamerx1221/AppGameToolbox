#include <AppGameToolbox/UIActionLoader.hpp>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <unordered_map>
#include <vector>

using namespace appgametoolbox;

namespace {
class TestPackageSource final : public UIActionPackageSource {
public:
    bool loadManifestText(const std::string& packageId, std::string& manifest, std::string& error) override {
        if (packageId != "menu") { error = "package not found"; return false; }
        ++manifestLoads;
        manifest = R"({"packageId":"menu","entryRoute":"main","localization":"locale/en.json","routes":[{"id":"main","html":"main.html","css":"menu.css"},{"id":"settings","html":"settings.html","css":"menu.css","preload":true}],"assets":[{"id":"logo","path":"images/logo.png"}]})";
        return true;
    }
    bool loadResourceText(const std::string&, const std::string& resource, std::string& text, std::string& error) override {
        ++loads[resource];
        if (resource == "main.html") text = "<div data-route-enter-duration='480ms' data-route-enter-easing='cubic-bezier(.22,1,.36,1)' data-route-enter-stagger='75ms' data-route-enter-reduced-duration='120ms'><div id='card' data-route-enter data-action=\"sequence([playSound('menu.navigate'),navigate('settings')])\" data-focus-order='1'><span id='label' data-bind-text='title'></span><div id='progress' data-bind-style-width='progress'></div></div></div>";
        else if (resource == "settings.html") text = "<div data-route-enter-duration='480ms' data-route-enter-easing='cubic-bezier(.22,1,.36,1)' data-route-enter-stagger='75ms' data-route-enter-reduced-duration='120ms'><div id='card' data-route-enter data-action=\"playSound('menu.open')\" data-focus-order='1'><span id='settings-label'>Audio</span></div></div>";
        else if (resource == "menu.css") text = "@keyframes route-enter { from { opacity:0; } to { opacity:1; } } #card { width: 40px; height: 30px; } #label, #settings-label { position:absolute; left:2px; top:2px; width:20px; height:12px; } #progress { position:absolute; left:2px; top:20px; height:2px; }";
        else { error = "resource not found"; return false; }
        return true;
    }
    std::unordered_map<std::string, int> loads;
    int manifestLoads = 0;
};
}

void runUIActionLoaderTests() {
    UIActionPackageManifest manifest;
    std::string manifestError;
    assert(parseUIActionPackageManifest(
        R"({"packageId":"test","entryRoute":"home","localization":"locale/en.json","routes":[{"id":"home","html":"home.html","css":"site.css"}],"assets":[{"id":"icon","path":"images/icon.png"}]})",
        manifest, manifestError));
    assert(manifest.packageId == "test" && manifest.entryRoute == "home");
    assert(manifest.routes.front().html == "home.html" && manifest.assets.front().id == "icon");

    UIActionLoader loader;
    TestPackageSource source;
    assert(loader.load(source, "menu"));
    assert(loader.route() == "main");
    assert(source.manifestLoads == 1);
    assert(source.loads["settings.html"] == 1); // Preloaded before its first navigation.
    assert(loader.setTextBinding("title", "Inventory"));
    assert(loader.setStyleBinding("progress", "width", "25px"));
    loader.advance(0.2);
    loader.setTime(0.1);
    assert(loader.routeTime() == 0.2);
    Render2DRecorder recorder({40.0, 30.0});
    assert(loader.document().record(recorder, {40.0, 30.0}));

    std::vector<std::string> sounds;
    ActionDispatcher dispatcher;
    dispatcher.registerHandler(UIActionKind::PlaySound, [&sounds](const UIAction& action) {
        sounds.push_back(std::get<std::string>(action.arguments.at("sound")));
        return UIActionDispatchResult{};
    });
    loader.pointerMove({5.0, 5.0});
    loader.pointerPress({5.0, 5.0});
    assert(loader.pointerRelease({5.0, 5.0}, dispatcher));
    assert(loader.route() == "settings");
    assert(loader.routeTime() == 0.0);
    assert(std::get<std::string>(*loader.state().value("route")) == "settings");
    assert(source.loads["settings.html"] == 1);
    assert(sounds == std::vector<std::string>{"menu.navigate"});
    // Route changes discard the old focus and select the first node in the new route.
    assert(loader.activateFocused(dispatcher));
    assert(sounds == std::vector<std::string>({"menu.navigate", "menu.open"}));
    Render2DRecorder settingsRecorder({40.0, 30.0});
    assert(loader.document().record(settingsRecorder, {40.0, 30.0}));
    const auto nestedLabel = loader.document().nodeIdForElementId("settings-label");
    assert(nestedLabel && loader.document().hitTest({5.0, 5.0}) == nestedLabel);
    loader.pointerMove({5.0, 5.0});
    loader.pointerPress({5.0, 5.0});
    assert(loader.pointerRelease({5.0, 5.0}, dispatcher));
    assert(sounds == std::vector<std::string>({"menu.navigate", "menu.open", "menu.open"}));
    assert(loader.activateFocused(dispatcher));
    assert(sounds == std::vector<std::string>({"menu.navigate", "menu.open", "menu.open", "menu.open"}));
    assert(loader.dispatchBack(dispatcher));
    assert(loader.route() == "main");
    assert(loader.routeTime() == 0.0);
    const auto undeclared = loader.dispatch(UIAction::make(UIActionKind::Navigate, {{"route", std::string("missing")}}), dispatcher);
    assert(undeclared.status == UIActionDispatchStatus::HandlerFailed);
    assert(loader.lastError() == "route is not declared by package manifest");
    assert(source.loads["main.html"] == 1);

    UIActionLoader focus;
    assert(focus.load(
        "<div id='first' data-action=\"custom('first')\" data-focus-order='20'></div>"
        "<div id='second' data-action=\"custom('second')\" data-focus-order='10' data-nav-right='third'></div>"
        "<div id='third' data-action=\"custom('third')\" data-focus-order='30'></div>"));
    std::vector<std::string> focusedActions;
    ActionDispatcher focusDispatcher;
    for (const std::string name : {"first", "second", "third"})
        focusDispatcher.registerCustomHandler(name, [&focusedActions, name](const UIAction&) {
            focusedActions.push_back(name);
            return UIActionDispatchResult{};
        });
    assert(focus.activateFocused(focusDispatcher)); // Order 10 is selected first.
    assert(focus.focusNext() && focus.activateFocused(focusDispatcher));
    assert(focus.focusPrevious() && focus.activateFocused(focusDispatcher));
    assert(focus.moveFocus(UIActionLoader::FocusDirection::Right) && focus.activateFocused(focusDispatcher));
    assert(focus.moveFocus(UIActionLoader::FocusDirection::Left) && focus.activateFocused(focusDispatcher));
    assert(focusedActions == std::vector<std::string>({"second", "first", "second", "third", "first"}));

    UIStateStore state;
    int notifications = 0;
    state.subscribe([&notifications](const std::string&, const UIStateValue&) { ++notifications; });
    assert(state.set("enabled", true));
    assert(state.set("count", std::int64_t(-7)));
    assert(state.set("ratio", 1.5));
    assert(state.set("name", std::string("Atlas")));
    assert(!state.set("enabled", true));
    assert(notifications == 4);
    assert(state.isDirty());
    assert(state.consumeDirtyKeys() == std::vector<std::string>({"enabled", "count", "ratio", "name"}));
    assert(!state.isDirty());
    assert(UIStateStore::toString(UIStateValue(true)) == "true");
    assert(UIStateStore::toString(UIStateValue(std::int64_t(-7))) == "-7");
    assert(UIStateStore::toString(UIStateValue(1.5)) == "1.5");
    assert(UIStateStore::toString(UIStateValue(std::string("Atlas"))) == "Atlas");

    UIActionLoader bindings;
    assert(bindings.load("<div><span data-bind-text='title'></span><div id='progress' data-bind-style-width='width'></div><div data-bind-visible='visible'><span>VISIBLE</span></div></div>",
        "div { position:absolute; } span { position:absolute; color:#ffffff; } #progress { height:2px; background:#ffffff; }"));
    assert(bindings.state().set("title", std::int64_t(42)));
    assert(bindings.state().set("width", std::string("25px")));
    assert(bindings.state().set("visible", false));
    assert(!bindings.state().set("visible", false));
    Render2DRecorder bindingsRecorder({100.0, 100.0});
    assert(bindings.document().record(bindingsRecorder, {100.0, 100.0}));
    RecordedFrame2D bindingsFrame;
    assert(bindingsRecorder.finish(bindingsFrame));
    bool sawTitle = false;
    bool sawVisible = false;
    bool sawWidth = false;
    for (const Render2DCommand& command : bindingsFrame.commands()) {
        const auto* text = std::get_if<DrawText2DCommand>(&command);
        if (text != nullptr) {
            sawTitle = sawTitle || text->text == "42";
            sawVisible = sawVisible || text->text == "VISIBLE";
        }
        if (const auto* fill = std::get_if<FillRect2DCommand>(&command)) sawWidth = sawWidth || fill->rect.size.width == 25.0;
    }
    assert(sawTitle);
    assert(!sawVisible);
    assert(sawWidth);
    assert(bindings.state().set("visible", true));
    bindingsRecorder.reset({100.0, 100.0});
    assert(bindings.document().record(bindingsRecorder, {100.0, 100.0}));
    assert(bindingsRecorder.finish(bindingsFrame));
    sawVisible = false;
    for (const Render2DCommand& command : bindingsFrame.commands()) {
        const auto* text = std::get_if<DrawText2DCommand>(&command);
        sawVisible = sawVisible || (text != nullptr && text->text == "VISIBLE");
    }
    assert(sawVisible);

    UIActionLoader panelProgress;
    assert(panelProgress.load("<div class='track'><div id='fill' data-bind-style-width='task.progressWidth'></div></div>",
        ".track { width:392px; height:7px; background:#213038; } #fill { height:7px; background:#ffffff; }"));
    assert(panelProgress.state().set("task.progressWidth", std::string("196px")));
    Render2DRecorder panelProgressRecorder({400.0, 20.0});
    assert(panelProgress.document().record(panelProgressRecorder, {400.0, 20.0}));
    RecordedFrame2D panelProgressFrame;
    assert(panelProgressRecorder.finish(panelProgressFrame));
    bool sawPanelFill = false;
    for (const Render2DCommand& command : panelProgressFrame.commands())
        if (const auto* fill = std::get_if<FillRect2DCommand>(&command)) sawPanelFill = sawPanelFill || fill->rect.size.width == 196.0;
    assert(sawPanelFill);

    const std::string transitionHtml =
        "<div data-route-enter-duration='480ms' data-route-enter-easing='cubic-bezier(.22,1,.36,1)' data-route-enter-stagger='75ms' data-route-enter-reduced-duration='120ms'>"
        "<div id='first' data-route-enter></div><div id='second' data-route-enter></div></div>";
    const std::string transitionCss = "@keyframes route-enter { from { opacity:0; transform:translate(-8px,4px); } to { opacity:1; transform:translate(0px,0px); } } #first, #second { position:absolute; width:10px; height:10px; background:#ffffff; } #second { top:10px; }";
    const auto opacityValues = [](UIActionLoader& animated) {
        Render2DRecorder animationRecorder({30.0, 30.0});
        assert(animated.document().record(animationRecorder, {30.0, 30.0}));
        RecordedFrame2D animationFrame;
        assert(animationRecorder.finish(animationFrame));
        std::vector<float> values;
        for (const Render2DCommand& command : animationFrame.commands())
            if (const auto* opacity = std::get_if<SetOpacity2DCommand>(&command)) values.push_back(opacity->opacity);
        return values;
    };
    UIActionLoader transitionLoader;
    assert(transitionLoader.load(transitionHtml, transitionCss));
    const auto initialOpacity = opacityValues(transitionLoader);
    assert(std::count(initialOpacity.begin(), initialOpacity.end(), 0.0f) == 2);
    const auto initialFirstBounds = transitionLoader.document().bounds(*transitionLoader.document().nodeIdForElementId("first"));
    const auto initialSecondBounds = transitionLoader.document().bounds(*transitionLoader.document().nodeIdForElementId("second"));
    assert(initialFirstBounds && initialSecondBounds);
    assert(initialFirstBounds->origin.x == -8.0 && initialFirstBounds->origin.y == 4.0);
    assert(initialSecondBounds->origin.x == -8.0 && initialSecondBounds->origin.y == 14.0);
    transitionLoader.advance(0.05);
    const auto staggeredOpacity = opacityValues(transitionLoader);
    assert(std::count(staggeredOpacity.begin(), staggeredOpacity.end(), 0.0f) == 1);
    assert(std::any_of(staggeredOpacity.begin(), staggeredOpacity.end(), [](float value) { return value > 0.0f && value < 1.0f; }));
    const auto staggeredFirstBounds = transitionLoader.document().bounds(*transitionLoader.document().nodeIdForElementId("first"));
    const auto staggeredSecondBounds = transitionLoader.document().bounds(*transitionLoader.document().nodeIdForElementId("second"));
    assert(staggeredFirstBounds && staggeredSecondBounds);
    assert(staggeredFirstBounds->origin.x > -8.0 && staggeredFirstBounds->origin.x < 0.0);
    assert(staggeredSecondBounds->origin.x == -8.0 && staggeredSecondBounds->origin.y == 14.0);
    transitionLoader.advance(0.43);
    const auto completedFirstOpacity = opacityValues(transitionLoader);
    assert(std::count(completedFirstOpacity.begin(), completedFirstOpacity.end(), 1.0f) >= 1);
    assert(std::any_of(completedFirstOpacity.begin(), completedFirstOpacity.end(), [](float value) { return value > 0.0f && value < 1.0f; }));

    UIActionLoader reducedMotionLoader;
    reducedMotionLoader.setReducedMotion(true);
    assert(reducedMotionLoader.load(transitionHtml, transitionCss));
    reducedMotionLoader.advance(0.13);
    const auto reducedOpacity = opacityValues(reducedMotionLoader);
    assert(std::none_of(reducedOpacity.begin(), reducedOpacity.end(), [](float value) { return value < 1.0f; }));

    UIActionLoader effects;
    assert(effects.load(
        "<div id='anchor' data-action=\"custom('launch')\" data-focus-order='1' data-effect-kind='press-ripple' data-effect-variant='1' data-effect-anchor='anchor' data-effect-clip-radius='4' data-effect-duration='500ms' data-effect-task='true' data-effect-title-key='mission.title' data-effect-description-key='mission.description' data-effect-progress-key='mission.progress' data-effect-completion-key='mission.complete' data-effect-visibility-key='mission.visible' data-effect-accent='#66e6ff' data-effect-start-sound='task.start' data-effect-progress-sound='task.progress' data-effect-completion-sound='task.complete'><span data-bind-text='mission.title'></span><span data-bind-text='mission.description'></span><span data-bind-text='mission.progress'></span><div data-bind-visible='mission.visible'>VISIBLE</div></div>",
        "#anchor { width:100px; height:40px; } span { position:absolute; }"));
    effects.state().set("mission.title", std::string("Deploy"));
    effects.state().set("mission.description", std::string("Preparing"));
    effects.state().set("mission.progress", 0.25);
    effects.state().set("mission.complete", false);
    Render2DRecorder effectRecorder({120.0, 80.0});
    assert(effects.document().record(effectRecorder, {120.0, 80.0}));
    RecordedFrame2D effectDocumentFrame;
    assert(effectRecorder.finish(effectDocumentFrame));
    bool sawTaskTitle = false;
    bool sawTaskDescription = false;
    for (const Render2DCommand& command : effectDocumentFrame.commands()) {
        const auto* text = std::get_if<DrawText2DCommand>(&command);
        sawTaskTitle = sawTaskTitle || (text != nullptr && text->text == "Deploy");
        sawTaskDescription = sawTaskDescription || (text != nullptr && text->text == "Preparing");
    }
    assert(sawTaskTitle && sawTaskDescription);
    std::vector<std::string> launches;
    ActionDispatcher effectDispatcher;
    effectDispatcher.registerCustomHandler("launch", [&launches](const UIAction&) { launches.push_back("launch"); return UIActionDispatchResult{}; });
    assert(effects.focusFirst() && effects.activateFocused(effectDispatcher));
    assert(launches == std::vector<std::string>{"launch"});
    assert(effects.activeEffect());
    assert(effects.activeEffect()->descriptor.kind == EffectKind::PressRipple);
    assert(effects.activeEffect()->descriptor.anchorId == "anchor");
    assert(effects.activeEffect()->descriptor.variant == 1);
    assert(effects.activeEffect()->descriptor.clipRadius == 4.0);
    assert(effects.activeEffect()->descriptor.accentColor == "#66e6ff");
    assert(effects.activeEffect()->lifecycle == UIActionLoader::EffectLifecycle::Running);
    assert(effects.triggerEffectForElement("anchor"));
    assert(effects.activeEffect()->descriptor.duration == 0.5);
    assert(!effects.triggerEffectForElement("missing"));
    assert(effects.takeEffectAudioCues() == std::vector<std::string>({"task.start", "task.start"}));
    const EffectFrame startedFrame = effects.sampleEffectFrame();
    assert(!startedFrame.primitives.empty());
    assert(startedFrame.clip);
    assert(startedFrame.clip->origin.x == 0.0 && startedFrame.clip->origin.y == 0.0);
    assert(startedFrame.clip->size.width == 100.0 && startedFrame.clip->size.height == 40.0);
    assert(startedFrame.clipRadius == 4.0);
    assert(startedFrame.primitives.front().points.front().x == 50.0);
    effects.cancelActiveEffect();
    assert(effects.activeEffect()->lifecycle == UIActionLoader::EffectLifecycle::Cancelled);
    assert(effects.sampleEffectFrame().primitives.empty());
    assert(effects.activateFocused(effectDispatcher));
    assert(effects.activeEffect()->lifecycle == UIActionLoader::EffectLifecycle::Running);
    assert(effects.takeEffectAudioCues() == std::vector<std::string>{"task.start"});
    effects.advance(2.0);
    assert(effects.activeEffect()->lifecycle == UIActionLoader::EffectLifecycle::Running); // Task timing cannot complete game work.
    effects.state().set("mission.progress", 0.75);
    assert(effects.activeEffect()->progress == 0.75);
    assert(effects.takeEffectAudioCues() == std::vector<std::string>{"task.progress"});
    effects.state().set("mission.complete", true);
    assert(effects.activeEffect()->lifecycle == UIActionLoader::EffectLifecycle::Completed);
    assert(effects.takeEffectAudioCues() == std::vector<std::string>{"task.complete"});
    assert(effects.sampleEffectFrame().primitives.empty());

    UIActionLoader taskVisibility;
    assert(taskVisibility.load(
        "<div id='first' data-action=\"custom('task')\" data-focus-order='1' data-effect-kind='press-ripple' data-effect-task='true' data-effect-progress-key='first.progress' data-effect-completion-key='first.complete' data-effect-visibility-key='first.visible'></div>"
        "<div id='second' data-action=\"custom('task')\" data-focus-order='2' data-effect-kind='press-ripple' data-effect-task='true' data-effect-progress-key='second.progress' data-effect-completion-key='second.complete' data-effect-visibility-key='second.visible'></div>"));
    taskVisibility.state().set("first.visible", false);
    taskVisibility.state().set("second.visible", false);
    ActionDispatcher taskVisibilityDispatcher;
    taskVisibilityDispatcher.registerCustomHandler("task", [](const UIAction&) { return UIActionDispatchResult{}; });
    assert(taskVisibility.activateFocused(taskVisibilityDispatcher));
    assert(std::get<bool>(*taskVisibility.state().value("first.visible")));
    assert(taskVisibility.focusNext() && taskVisibility.activateFocused(taskVisibilityDispatcher));
    assert(!std::get<bool>(*taskVisibility.state().value("first.visible")));
    assert(std::get<bool>(*taskVisibility.state().value("second.visible")));

    UIActionLoader repeated;
    assert(repeated.state().setCollection("sectors", {
        {{"id", std::string("glass")}, {"order", std::int64_t(2)}, {"title", std::string("Glass")}, {"top", std::string("10px")}, {"visible", true}, {"action", std::string("custom('launch',{sector:'glass'})")}, {"kind", std::string("crystal-burst")}},
        {{"id", std::string("iron")}, {"order", std::int64_t(1)}, {"title", std::string("Iron")}, {"top", std::string("40px")}, {"visible", false}, {"action", std::string("custom('launch',{sector:'iron'})")}, {"kind", std::string("ember-trail")}}
    }));
    assert(repeated.load("<div data-repeat=\"sectors\" data-repeat-item=\"sector\" id=\"{{sector.id}}\" data-action=\"{{sector.action}}\" data-focus-order=\"{{sector.order}}\" data-effect-kind=\"{{sector.kind}}\" data-effect-anchor=\"{{sector.id}}\" data-effect-duration=\"1s\"><span data-bind-text=\"sector.title\"></span><span data-bind-style-top=\"sector.top\"></span><span data-bind-visible=\"sector.visible\">VISIBLE</span></div>"));
    const auto glass = repeated.document().nodeIdForElementId("glass");
    const auto iron = repeated.document().nodeIdForElementId("iron");
    assert(glass && iron); // Explicit item IDs become stable element IDs.
    assert(repeated.document().nodesWithDataAttribute("data-bind-text", "__repeat.sectors.glass.title").size() == 1);
    assert(repeated.document().nodesWithDataAttribute("data-bind-style-top", "__repeat.sectors.iron.top").size() == 1);
    assert(repeated.document().nodesWithDataAttribute("data-bind-visible", "__repeat.sectors.iron.visible").size() == 1);
    std::vector<std::string> repeatedSectors;
    ActionDispatcher repeatedDispatcher;
    repeatedDispatcher.registerCustomHandler("launch", [&repeatedSectors](const UIAction& action) {
        repeatedSectors.push_back(std::get<std::string>(action.arguments.at("sector")));
        return UIActionDispatchResult{};
    });
    assert(repeated.activateFocused(repeatedDispatcher)); // Markup order is retained for ties; explicit order still applies.
    assert(repeated.focusNext() && repeated.activateFocused(repeatedDispatcher));
    assert(repeatedSectors == std::vector<std::string>({"iron", "glass"}));
    Render2DRecorder repeatedRecorder({100.0, 100.0});
    assert(repeated.document().record(repeatedRecorder, {100.0, 100.0}));
    assert(repeated.focusFirst() && repeated.focusNext() && repeated.activateFocused(repeatedDispatcher));
    assert(repeated.activeEffect());
    assert(repeated.activeEffect()->descriptor.kind == EffectKind::CrystalBurst);
    assert(repeated.activeEffect()->descriptor.anchorId == "glass");
    assert(!repeated.sampleEffectFrame().primitives.empty());
    assert(repeated.state().setCollection("sectors", {
        {{"id", std::string("iron")}, {"order", std::int64_t(1)}, {"title", std::string("Iron II")}, {"top", std::string("10px")}, {"visible", true}, {"action", std::string("custom('launch',{sector:'iron'})")}, {"kind", std::string("ember-trail")}}
    }));
    assert(!repeated.document().nodeIdForElementId("glass"));
    assert(repeated.document().nodeIdForElementId("iron"));

    UIActionLoader scopedProgress;
    assert(scopedProgress.state().setCollection("tasks", {
        {{"id", std::string("first")}, {"width", std::string("10px")}, {"visible", false}},
        {{"id", std::string("second")}, {"width", std::string("0px")}, {"visible", false}}
    }));
    assert(scopedProgress.load("<div data-repeat='tasks' data-repeat-item='task'><div class='card-task' data-bind-visible='task.visible'><div class='card-task-fill' data-bind-style-width='task.width'></div></div></div>",
        ".card-task { width:20px; height:2px; } .card-task-fill { height:2px; background:#ffffff; }"));
    assert(scopedProgress.state().set("__repeat.tasks.first.visible", true));
    assert(scopedProgress.state().set("__repeat.tasks.second.visible", false));
    Render2DRecorder scopedRecorder({40.0, 20.0});
    assert(scopedProgress.document().record(scopedRecorder, {40.0, 20.0}));
    RecordedFrame2D scopedFrame;
    assert(scopedRecorder.finish(scopedFrame));
    int visibleProgressFills = 0;
    for (const Render2DCommand& command : scopedFrame.commands()) if (const auto* fill = std::get_if<FillRect2DCommand>(&command))
        visibleProgressFills += fill->rect.size.width == 10.0 ? 1 : 0;
    assert(visibleProgressFills == 1);
}
