#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace appgametoolbox {

// UIAction is declarative data. Hosts provide all game-specific behavior through handlers.
enum class UIActionKind {
    OpenPanel, ClosePanel, TogglePanel, Navigate, Back,
    Start, Restart, Pause, Resume, Quit, LoadLevel,
    SaveProfile, LoadProfile, DeleteProfile, ChangeSetting, UpdateCharacterAttribute,
    Equip, Use, Craft, Buy, Sell, AcceptMission, AbandonMission,
    ShowDialog, ShowToast, PlaySound, TriggerEffect, Telemetry,
    Custom, Sequence, Conditional
};

using UIActionValue = std::variant<bool, std::int64_t, double, std::string>;
using UIActionArguments = std::map<std::string, UIActionValue>;

struct UIAction {
    UIActionKind kind = UIActionKind::OpenPanel;
    UIActionArguments arguments;
    std::string customName;
    std::string predicate;
    std::vector<UIAction> actions;
    std::vector<UIAction> otherwise;

    static UIAction make(UIActionKind kind, UIActionArguments arguments = {});
    static UIAction custom(std::string name, UIActionArguments arguments = {});
    static UIAction sequence(std::vector<UIAction> actions);
    static UIAction conditional(std::string predicate, std::vector<UIAction> whenTrue,
                                std::vector<UIAction> whenFalse = {});

    // Validates required typed fields and composition bounds without invoking host code.
    bool validate(std::string* error = nullptr, std::size_t maxDepth = 8,
                  std::size_t maxChildren = 64) const;
};

const char* uiActionName(UIActionKind kind);

struct UIActionParseResult {
    UIAction action;
    std::string error;
    std::size_t errorOffset = 0;
    explicit operator bool() const { return error.empty(); }
};

// Parses a deliberately small expression language, never JavaScript or C++.
UIActionParseResult parseUIActionExpression(const std::string& expression);

// Produces a deterministic restricted expression suitable for persistence.
std::string serializeUIActionExpression(const UIAction& action);

class ActionCatalog {
public:
    bool add(std::string id, UIAction action, std::string* error = nullptr);
    const UIAction* find(const std::string& id) const;
    bool remove(const std::string& id);
    void clear();

private:
    std::map<std::string, UIAction> m_actions;
};

enum class UIActionDispatchStatus { Success, NotFound, InvalidAction, HandlerMissing, PredicateMissing, HandlerFailed };

struct UIActionDispatchResult {
    UIActionDispatchStatus status = UIActionDispatchStatus::Success;
    std::string message;
    std::size_t dispatchedCount = 0;
    explicit operator bool() const { return status == UIActionDispatchStatus::Success; }
};

using UIActionHandler = std::function<UIActionDispatchResult(const UIAction&)>;
using UIActionPredicate = std::function<bool(const std::string&)>;

class ActionDispatcher {
public:
    void registerHandler(UIActionKind kind, UIActionHandler handler);
    void registerCustomHandler(std::string name, UIActionHandler handler);
    void setPredicateHandler(UIActionPredicate handler);
    UIActionDispatchResult dispatch(const UIAction& action) const;
    UIActionDispatchResult dispatch(const ActionCatalog& catalog, const std::string& id) const;

private:
    UIActionDispatchResult dispatchImpl(const UIAction& action, std::size_t depth) const;
    std::map<UIActionKind, UIActionHandler> m_handlers;
    std::map<std::string, UIActionHandler> m_customHandlers;
    UIActionPredicate m_predicateHandler;
};

} // namespace appgametoolbox
