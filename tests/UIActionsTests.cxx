#include <AppGameToolbox/UIActions.hpp>

#include <cassert>
#include <vector>

using namespace appgametoolbox;

void runUIActionsTests() {
    const UIActionParseResult parsed = parseUIActionExpression(
        "sequence([openPanel('inventory'),changeSetting('audio.masterVolume',0.7)])");
    assert(parsed);
    assert(parsed.action.kind == UIActionKind::Sequence);
    assert(parsed.action.actions.size() == 2);
    assert(std::get<std::string>(parsed.action.actions[0].arguments.at("panel")) == "inventory");
    assert(serializeUIActionExpression(parsed.action) == "sequence([openPanel('inventory'),changeSetting('audio.masterVolume',0.7)])");

    const UIActionParseResult invalid = parseUIActionExpression("openPanel(unknown())");
    assert(!invalid && !invalid.error.empty());

    UIAction conditional = UIAction::conditional("hasSave", {UIAction::make(UIActionKind::LoadLevel, {{"level", std::string("checkpoint")}})},
                                                  {UIAction::make(UIActionKind::ShowToast, {{"text", std::string("No save")}})});
    ActionCatalog catalog;
    assert(catalog.add("continueGame", conditional));

    std::vector<UIActionKind> called;
    ActionDispatcher dispatcher;
    dispatcher.setPredicateHandler([](const std::string& predicate) { return predicate == "hasSave"; });
    dispatcher.registerHandler(UIActionKind::LoadLevel, [&called](const UIAction& action) {
        assert(std::get<std::string>(action.arguments.at("level")) == "checkpoint");
        called.push_back(action.kind); return UIActionDispatchResult{};
    });
    assert(dispatcher.dispatch(catalog, "continueGame"));
    assert(called == std::vector<UIActionKind>{UIActionKind::LoadLevel});

    dispatcher.registerCustomHandler("refreshStore", [&called](const UIAction& action) {
        assert(std::get<bool>(action.arguments.at("featured"))); called.push_back(action.kind); return UIActionDispatchResult{};
    });
    const UIAction custom = UIAction::custom("refreshStore", {{"featured", true}});
    assert(dispatcher.dispatch(custom));
    assert(called.back() == UIActionKind::Custom);

    UIAction malformed = UIAction::make(UIActionKind::ChangeSetting, {{"key", std::string("ui.scale")}});
    assert(!malformed.validate());
    assert(dispatcher.dispatch(malformed).status == UIActionDispatchStatus::InvalidAction);
}
