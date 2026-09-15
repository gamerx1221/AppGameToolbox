#include <AppGameToolbox/UIActions.hpp>

#include <cctype>
#include <sstream>

namespace appgametoolbox {
namespace {

bool isIdentifier(const std::string& value) {
    if (value.empty() || !(std::isalpha(static_cast<unsigned char>(value[0])) || value[0] == '_')) return false;
    for (char c : value) if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.')) return false;
    return true;
}

bool hasString(const UIAction& action, const char* key) {
    const auto found = action.arguments.find(key);
    return found != action.arguments.end() && std::holds_alternative<std::string>(found->second) &&
           !std::get<std::string>(found->second).empty();
}

bool hasValue(const UIAction& action, const char* key) { return action.arguments.find(key) != action.arguments.end(); }

const char* requiredKey(UIActionKind kind) {
    switch (kind) {
    case UIActionKind::OpenPanel: case UIActionKind::ClosePanel: case UIActionKind::TogglePanel: return "panel";
    case UIActionKind::Navigate: return "route";
    case UIActionKind::LoadLevel: return "level";
    case UIActionKind::SaveProfile: case UIActionKind::LoadProfile: case UIActionKind::DeleteProfile: return "profile";
    case UIActionKind::ChangeSetting: return "key";
    case UIActionKind::UpdateCharacterAttribute: return "character";
    case UIActionKind::Equip: case UIActionKind::Use: case UIActionKind::Buy: case UIActionKind::Sell: return "item";
    case UIActionKind::Craft: return "recipe";
    case UIActionKind::AcceptMission: case UIActionKind::AbandonMission: return "mission";
    case UIActionKind::ShowDialog: case UIActionKind::ShowToast: return "text";
    case UIActionKind::PlaySound: return "sound";
    case UIActionKind::TriggerEffect: return "effect";
    case UIActionKind::Telemetry: return "event";
    default: return nullptr;
    }
}

void appendEscaped(std::ostringstream& output, const std::string& value) {
    output << '\'';
    for (char c : value) {
        if (c == '\\' || c == '\'') output << '\\';
        if (c == '\n') output << "\\n";
        else if (c != '\n') output << c;
    }
    output << '\'';
}

void appendValue(std::ostringstream& output, const UIActionValue& value) {
    if (const auto* text = std::get_if<std::string>(&value)) appendEscaped(output, *text);
    else if (const auto* boolean = std::get_if<bool>(&value)) output << (*boolean ? "true" : "false");
    else if (const auto* integer = std::get_if<std::int64_t>(&value)) output << *integer;
    else output << std::get<double>(value);
}

class Parser {
public:
    explicit Parser(const std::string& input) : m_input(input) {}
    UIActionParseResult parse() {
        UIActionParseResult result;
        result.action = action();
        skip();
        if (m_error.empty() && m_position != m_input.size()) fail("unexpected trailing input");
        if (m_error.empty()) {
            std::string validationError;
            if (!result.action.validate(&validationError)) fail(validationError);
        }
        result.error = m_error;
        result.errorOffset = m_errorOffset;
        return result;
    }
private:
    UIAction action() {
        const std::string name = identifier();
        if (!expect('(')) return {};
        if (name == "sequence") {
            std::vector<UIAction> values = actions();
            expect(')');
            return UIAction::sequence(std::move(values));
        }
        if (name == "ifState") {
            const std::string predicate = string();
            if (!expect(',')) return {};
            std::vector<UIAction> yes = actions();
            std::vector<UIAction> no;
            if (consume(',')) no = actions();
            expect(')');
            return UIAction::conditional(predicate, std::move(yes), std::move(no));
        }
        if (name == "custom") {
            const std::string customName = string();
            UIActionArguments arguments;
            if (consume(',')) arguments = object();
            expect(')');
            return UIAction::custom(customName, std::move(arguments));
        }
        UIActionKind kind;
        if (!kindForName(name, kind)) { fail("unknown action '" + name + "'"); return {}; }
        UIActionArguments arguments;
        const char* key = requiredKey(kind);
        if (key != nullptr) arguments[key] = value();
        if (kind == UIActionKind::ChangeSetting) {
            if (!expect(',')) return {};
            arguments["value"] = value();
        } else if (kind == UIActionKind::UpdateCharacterAttribute) {
            if (!expect(',')) return {};
            arguments["attribute"] = value();
            if (!expect(',')) return {};
            arguments["value"] = value();
        }
        expect(')');
        return UIAction::make(kind, std::move(arguments));
    }
    std::vector<UIAction> actions() {
        std::vector<UIAction> values;
        if (!expect('[')) return values;
        if (!consume(']')) {
            do { values.push_back(action()); } while (consume(','));
            expect(']');
        }
        return values;
    }
    UIActionArguments object() {
        UIActionArguments values;
        if (!expect('{')) return values;
        if (!consume('}')) {
            do {
                const std::string key = identifier();
                if (!expect(':')) return values;
                values[key] = value();
            } while (consume(','));
            expect('}');
        }
        return values;
    }
    UIActionValue value() {
        skip();
        if (peek() == '\'') return string();
        if (m_input.compare(m_position, 4, "true") == 0) { m_position += 4; return true; }
        if (m_input.compare(m_position, 5, "false") == 0) { m_position += 5; return false; }
        const std::size_t start = m_position;
        if (peek() == '-') ++m_position;
        while (std::isdigit(static_cast<unsigned char>(peek()))) ++m_position;
        bool decimal = false;
        if (peek() == '.') { decimal = true; ++m_position; while (std::isdigit(static_cast<unsigned char>(peek()))) ++m_position; }
        if (start == m_position || (m_input[start] == '-' && start + 1 == m_position)) { fail("expected literal"); return {}; }
        const std::string number = m_input.substr(start, m_position - start);
        try { return decimal ? UIActionValue(std::stod(number)) : UIActionValue(static_cast<std::int64_t>(std::stoll(number))); }
        catch (...) { fail("invalid number"); return {}; }
    }
    std::string string() {
        skip();
        if (!expect('\'')) return {};
        std::string result;
        while (m_position < m_input.size() && m_input[m_position] != '\'') {
            char c = m_input[m_position++];
            if (c == '\\') {
                if (m_position == m_input.size()) { fail("unfinished escape"); return {}; }
                c = m_input[m_position++];
                if (c == 'n') c = '\n'; else if (c != '\\' && c != '\'') { fail("unsupported escape"); return {}; }
            }
            result += c;
        }
        expect('\'');
        return result;
    }
    std::string identifier() {
        skip(); const std::size_t start = m_position;
        while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_' || peek() == '.') ++m_position;
        const std::string result = m_input.substr(start, m_position - start);
        if (!isIdentifier(result)) fail("expected identifier");
        return result;
    }
    char peek() const { return m_position < m_input.size() ? m_input[m_position] : '\0'; }
    void skip() { while (std::isspace(static_cast<unsigned char>(peek()))) ++m_position; }
    bool consume(char value) { skip(); if (peek() != value) return false; ++m_position; return true; }
    bool expect(char value) { if (consume(value)) return true; fail(std::string("expected '") + value + "'"); return false; }
    void fail(std::string error) { if (m_error.empty()) { m_error = std::move(error); m_errorOffset = m_position; } }
    static bool kindForName(const std::string& name, UIActionKind& kind) {
        for (int i = 0; i <= static_cast<int>(UIActionKind::Telemetry); ++i) {
            const auto candidate = static_cast<UIActionKind>(i);
            std::string expected = uiActionName(candidate);
            expected[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(expected[0])));
            if (name == expected) { kind = candidate; return true; }
        }
        return false;
    }
    const std::string& m_input; std::size_t m_position = 0; std::string m_error; std::size_t m_errorOffset = 0;
};
} // namespace

const char* uiActionName(UIActionKind kind) {
    static const char* names[] = {"OpenPanel", "ClosePanel", "TogglePanel", "Navigate", "Back", "Start", "Restart", "Pause", "Resume", "Quit", "LoadLevel", "SaveProfile", "LoadProfile", "DeleteProfile", "ChangeSetting", "UpdateCharacterAttribute", "Equip", "Use", "Craft", "Buy", "Sell", "AcceptMission", "AbandonMission", "ShowDialog", "ShowToast", "PlaySound", "TriggerEffect", "Telemetry", "Custom", "Sequence", "Conditional"};
    return names[static_cast<int>(kind)];
}

UIAction UIAction::make(UIActionKind kind, UIActionArguments arguments) { UIAction action; action.kind = kind; action.arguments = std::move(arguments); return action; }
UIAction UIAction::custom(std::string name, UIActionArguments arguments) { UIAction action = make(UIActionKind::Custom, std::move(arguments)); action.customName = std::move(name); return action; }
UIAction UIAction::sequence(std::vector<UIAction> actions) { UIAction action; action.kind = UIActionKind::Sequence; action.actions = std::move(actions); return action; }
UIAction UIAction::conditional(std::string predicate, std::vector<UIAction> yes, std::vector<UIAction> no) { UIAction action; action.kind = UIActionKind::Conditional; action.predicate = std::move(predicate); action.actions = std::move(yes); action.otherwise = std::move(no); return action; }

bool UIAction::validate(std::string* error, std::size_t maxDepth, std::size_t maxChildren) const {
    std::function<bool(const UIAction&, std::size_t)> check = [&](const UIAction& current, std::size_t depth) {
        if (depth > maxDepth) { if (error) *error = "action nesting exceeds maximum depth"; return false; }
        if (current.kind == UIActionKind::Sequence || current.kind == UIActionKind::Conditional) {
            if (current.actions.empty() || current.actions.size() > maxChildren || current.otherwise.size() > maxChildren) { if (error) *error = "composition must contain 1 to maximum children"; return false; }
            if (current.kind == UIActionKind::Conditional && !isIdentifier(current.predicate)) { if (error) *error = "conditional predicate must be an identifier"; return false; }
            for (const UIAction& child : current.actions) if (!check(child, depth + 1)) return false;
            for (const UIAction& child : current.otherwise) if (!check(child, depth + 1)) return false;
            return true;
        }
        if (current.kind == UIActionKind::Custom) {
            if (!isIdentifier(current.customName)) { if (error) *error = "custom action name must be an identifier"; return false; }
            return true;
        }
        const char* key = requiredKey(current.kind);
        if (key && !hasString(current, key)) { if (error) *error = std::string("action requires string '") + key + "'"; return false; }
        if ((current.kind == UIActionKind::ChangeSetting || current.kind == UIActionKind::UpdateCharacterAttribute) && !hasValue(current, "value")) { if (error) *error = "action requires 'value'"; return false; }
        if (current.kind == UIActionKind::UpdateCharacterAttribute && !hasString(current, "attribute")) { if (error) *error = "action requires string 'attribute'"; return false; }
        return true;
    };
    return check(*this, 0);
}

UIActionParseResult parseUIActionExpression(const std::string& expression) { return Parser(expression).parse(); }

std::string serializeUIActionExpression(const UIAction& action) {
    std::ostringstream output;
    if (action.kind == UIActionKind::Sequence) {
        output << "sequence([";
        for (std::size_t i = 0; i < action.actions.size(); ++i) { if (i) output << ','; output << serializeUIActionExpression(action.actions[i]); }
        return output.str() + "])";
    }
    if (action.kind == UIActionKind::Conditional) {
        output << "ifState("; appendEscaped(output, action.predicate); output << ",[";
        for (std::size_t i = 0; i < action.actions.size(); ++i) { if (i) output << ','; output << serializeUIActionExpression(action.actions[i]); }
        output << ']';
        if (!action.otherwise.empty()) { output << ",["; for (std::size_t i = 0; i < action.otherwise.size(); ++i) { if (i) output << ','; output << serializeUIActionExpression(action.otherwise[i]); } output << ']'; }
        return output.str() + ")";
    }
    if (action.kind == UIActionKind::Custom) {
        output << "custom("; appendEscaped(output, action.customName);
        if (!action.arguments.empty()) { output << ",{ "; bool first = true; for (const auto& pair : action.arguments) { if (!first) output << ','; output << pair.first << ':'; appendValue(output, pair.second); first = false; } output << '}'; }
        return output.str() + ")";
    }
    std::string name = uiActionName(action.kind); name[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(name[0])));
    output << name << '(';
    const char* key = requiredKey(action.kind);
    if (key) appendValue(output, action.arguments.at(key));
    if (action.kind == UIActionKind::ChangeSetting || action.kind == UIActionKind::UpdateCharacterAttribute) { output << ','; if (action.kind == UIActionKind::UpdateCharacterAttribute) { appendValue(output, action.arguments.at("attribute")); output << ','; } appendValue(output, action.arguments.at("value")); }
    return output.str() + ')';
}

bool ActionCatalog::add(std::string id, UIAction action, std::string* error) { if (!isIdentifier(id)) { if (error) *error = "catalog ID must be an identifier"; return false; } if (!action.validate(error)) return false; m_actions[std::move(id)] = std::move(action); return true; }
const UIAction* ActionCatalog::find(const std::string& id) const { const auto found = m_actions.find(id); return found == m_actions.end() ? nullptr : &found->second; }
bool ActionCatalog::remove(const std::string& id) { return m_actions.erase(id) != 0; }
void ActionCatalog::clear() { m_actions.clear(); }

void ActionDispatcher::registerHandler(UIActionKind kind, UIActionHandler handler) { m_handlers[kind] = std::move(handler); }
void ActionDispatcher::registerCustomHandler(std::string name, UIActionHandler handler) { m_customHandlers[std::move(name)] = std::move(handler); }
void ActionDispatcher::setPredicateHandler(UIActionPredicate handler) { m_predicateHandler = std::move(handler); }
UIActionDispatchResult ActionDispatcher::dispatch(const UIAction& action) const { return dispatchImpl(action, 0); }
UIActionDispatchResult ActionDispatcher::dispatch(const ActionCatalog& catalog, const std::string& id) const { const UIAction* action = catalog.find(id); return action ? dispatch(*action) : UIActionDispatchResult{UIActionDispatchStatus::NotFound, "action ID not found", 0}; }
UIActionDispatchResult ActionDispatcher::dispatchImpl(const UIAction& action, std::size_t depth) const {
    std::string error;
    if (depth > 8 || !action.validate(&error)) return {UIActionDispatchStatus::InvalidAction, error, 0};
    if (action.kind == UIActionKind::Sequence || action.kind == UIActionKind::Conditional) {
        const std::vector<UIAction>* children = &action.actions;
        if (action.kind == UIActionKind::Conditional) { if (!m_predicateHandler) return {UIActionDispatchStatus::PredicateMissing, "no predicate handler registered", 0}; if (!m_predicateHandler(action.predicate)) children = &action.otherwise; }
        UIActionDispatchResult total;
        for (const UIAction& child : *children) { UIActionDispatchResult result = dispatchImpl(child, depth + 1); total.dispatchedCount += result.dispatchedCount; if (!result) return result; }
        return total;
    }
    const UIActionHandler* handler = nullptr;
    if (action.kind == UIActionKind::Custom) { const auto found = m_customHandlers.find(action.customName); if (found != m_customHandlers.end()) handler = &found->second; }
    else { const auto found = m_handlers.find(action.kind); if (found != m_handlers.end()) handler = &found->second; }
    if (!handler) return {UIActionDispatchStatus::HandlerMissing, "no handler registered for action", 0};
    UIActionDispatchResult result = (*handler)(action);
    if (result.status == UIActionDispatchStatus::Success) ++result.dispatchedCount;
    return result;
}

} // namespace appgametoolbox
