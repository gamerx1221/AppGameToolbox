#include <AppGameToolbox/HtmlCss.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

namespace appgametoolbox {

namespace {

std::string trim(std::string value) {
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char c) { return std::isspace(c) != 0; });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) { return std::isspace(c) != 0; }).base();
    return first >= last ? std::string{} : std::string(first, last);
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string collapseWhitespace(const std::string& value) {
    std::string result;
    bool pendingSpace = false;
    for (unsigned char character : value) {
        if (std::isspace(character) != 0) pendingSpace = !result.empty();
        else {
            if (pendingSpace) result += ' ';
            result += static_cast<char>(character);
            pendingSpace = false;
        }
    }
    return result;
}

std::string decodeEntities(std::string value) {
    const std::pair<const char*, const char*> entities[] = {
        {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&#39;", "'"},
    };
    for (const auto& entity : entities) {
        std::size_t position = 0;
        while ((position = value.find(entity.first, position)) != std::string::npos) {
            value.replace(position, std::char_traits<char>::length(entity.first), entity.second);
            position += std::char_traits<char>::length(entity.second);
        }
    }
    return value;
}

bool isVoidElement(const std::string& tag) {
    return tag == "img" || tag == "br" || tag == "hr" || tag == "meta" || tag == "link" || tag == "input";
}

double number(const std::string& value, double fallback = 0.0) {
    try { return std::stod(trim(value)); }
    catch (...) { return fallback; }
}

float clampOpacity(double value) {
    return static_cast<float>(std::clamp(value, 0.0, 1.0));
}

struct Length {
    enum class Unit { Auto, Linear };
    Unit unit = Unit::Auto;
    double pixels = 0.0;
    double percent = 0.0;
};

struct CalcValue {
    bool valid = false;
    bool length = false;
    double pixels = 0.0;
    double percent = 0.0;
    double scalar = 0.0;
};

class CalcParser {
public:
    explicit CalcParser(const std::string& text) : m_text(text) {}
    CalcValue parse() { CalcValue value = expression(); skipSpace(); return m_position == m_text.size() ? value : CalcValue{}; }

private:
    const std::string& m_text;
    std::size_t m_position = 0;

    void skipSpace() { while (m_position < m_text.size() && std::isspace(static_cast<unsigned char>(m_text[m_position]))) ++m_position; }
    CalcValue expression() {
        CalcValue value = term();
        while (value.valid) {
            skipSpace();
            if (m_position == m_text.size() || (m_text[m_position] != '+' && m_text[m_position] != '-')) break;
            const char operation = m_text[m_position++];
            CalcValue right = term();
            if (!right.valid || value.length != right.length) return {};
            if (value.length) { value.pixels += operation == '+' ? right.pixels : -right.pixels; value.percent += operation == '+' ? right.percent : -right.percent; }
            else value.scalar += operation == '+' ? right.scalar : -right.scalar;
        }
        return value;
    }
    CalcValue term() {
        CalcValue value = factor();
        while (value.valid) {
            skipSpace();
            if (m_position == m_text.size() || (m_text[m_position] != '*' && m_text[m_position] != '/')) break;
            const char operation = m_text[m_position++];
            CalcValue right = factor();
            if (!right.valid || right.length || (operation == '/' && right.scalar == 0.0)) return {};
            if (value.length) { value.pixels = operation == '*' ? value.pixels * right.scalar : value.pixels / right.scalar; value.percent = operation == '*' ? value.percent * right.scalar : value.percent / right.scalar; }
            else value.scalar = operation == '*' ? value.scalar * right.scalar : value.scalar / right.scalar;
        }
        return value;
    }
    CalcValue factor() {
        skipSpace();
        if (m_position < m_text.size() && m_text[m_position] == '(') {
            ++m_position; CalcValue value = expression(); skipSpace();
            if (!value.valid || m_position == m_text.size() || m_text[m_position++] != ')') return {};
            return value;
        }
        const std::size_t start = m_position;
        if (m_position < m_text.size() && (m_text[m_position] == '+' || m_text[m_position] == '-')) ++m_position;
        bool digit = false;
        while (m_position < m_text.size() && (std::isdigit(static_cast<unsigned char>(m_text[m_position])) || m_text[m_position] == '.')) { digit = true; ++m_position; }
        if (!digit) return {};
        const double value = number(m_text.substr(start, m_position - start));
        if (m_position < m_text.size() && m_text[m_position] == '%') { ++m_position; return {true, true, 0.0, value, 0.0}; }
        if (m_position + 1 < m_text.size() && lower(m_text.substr(m_position, 2)) == "px") { m_position += 2; return {true, true, value, 0.0, 0.0}; }
        return {true, false, 0.0, 0.0, value};
    }
};

Length parseLength(const std::string& text) {
    const std::string value = lower(trim(text));
    if (value.empty() || value == "auto") return {};
    if (value.size() > 6 && value.rfind("calc(", 0) == 0 && value.back() == ')') {
        CalcValue calculated = CalcParser(value.substr(5, value.size() - 6)).parse();
        return calculated.valid && calculated.length ? Length{Length::Unit::Linear, calculated.pixels, calculated.percent} : Length{};
    }
    if (value.back() == '%') return {Length::Unit::Linear, 0.0, number(value.substr(0, value.size() - 1))};
    if (value.size() > 2 && value.substr(value.size() - 2) == "px") return {Length::Unit::Linear, number(value.substr(0, value.size() - 2)), 0.0};
    return {Length::Unit::Linear, number(value), 0.0};
}

double resolveLength(Length length, double available, double fallback = 0.0) {
    if (length.unit == Length::Unit::Linear) return length.pixels + available * length.percent / 100.0;
    return fallback;
}

double parseSeconds(const std::string& text) {
    const std::string value = lower(trim(text));
    if (value.size() > 2 && value.substr(value.size() - 2) == "ms") return std::max(0.0, number(value.substr(0, value.size() - 2)) / 1000.0);
    if (!value.empty() && value.back() == 's') return std::max(0.0, number(value.substr(0, value.size() - 1)));
    return -1.0;
}

bool parseHex(const std::string& value, Color& color) {
    if (value.size() != 4 && value.size() != 7) return false;
    const auto digit = [](char character) -> int {
        if (character >= '0' && character <= '9') return character - '0';
        if (character >= 'a' && character <= 'f') return character - 'a' + 10;
        if (character >= 'A' && character <= 'F') return character - 'A' + 10;
        return -1;
    };
    const auto component = [&](std::size_t offset) -> int {
        const int high = digit(value[offset]);
        const int low = value.size() == 4 ? high : digit(value[offset + 1]);
        return high < 0 || low < 0 ? -1 : high * 16 + low;
    };
    const int red = component(1);
    const int green = component(value.size() == 4 ? 2 : 3);
    const int blue = component(value.size() == 4 ? 3 : 5);
    if (red < 0 || green < 0 || blue < 0) return false;
    color = {red / 255.0f, green / 255.0f, blue / 255.0f, 1.0f};
    return true;
}

bool parseColor(const std::string& text, Color& color) {
    const std::string value = lower(trim(text));
    if (!value.empty() && value.front() == '#') return parseHex(value, color);
    const bool rgb = value.rfind("rgb(", 0) == 0 || value.rfind("rgba(", 0) == 0;
    if (rgb && value.back() == ')') {
        const bool alpha = value.rfind("rgba(", 0) == 0;
        std::vector<double> components;
        std::size_t position = alpha ? 5 : 4;
        while (position < value.size() - 1) {
            const std::size_t comma = value.find(',', position);
            components.push_back(number(value.substr(position, (comma == std::string::npos ? value.size() - 1 : comma) - position), -1.0));
            if (comma == std::string::npos) break;
            position = comma + 1;
        }
        if (components.size() == (alpha ? 4U : 3U) && std::all_of(components.begin(), components.begin() + 3, [](double component) { return component >= 0.0 && component <= 255.0; }) &&
            (!alpha || (components[3] >= 0.0 && components[3] <= 1.0))) {
            color = {static_cast<float>(components[0] / 255.0), static_cast<float>(components[1] / 255.0), static_cast<float>(components[2] / 255.0), static_cast<float>(alpha ? components[3] : 1.0)};
            return true;
        }
    }
    if (value == "transparent") { color = {0.0f, 0.0f, 0.0f, 0.0f}; return true; }
    if (value == "black") { color = {0.0f, 0.0f, 0.0f, 1.0f}; return true; }
    if (value == "white") { color = {1.0f, 1.0f, 1.0f, 1.0f}; return true; }
    if (value == "red") { color = {1.0f, 0.0f, 0.0f, 1.0f}; return true; }
    if (value == "green") { color = {0.0f, 0.5f, 0.0f, 1.0f}; return true; }
    if (value == "blue") { color = {0.0f, 0.0f, 1.0f, 1.0f}; return true; }
    if (value == "gray" || value == "grey") { color = {0.5f, 0.5f, 0.5f, 1.0f}; return true; }
    return false;
}

struct GradientDefinition {
    Color startColor;
    Color endColor;
    bool horizontal = false;
};

bool parseLinearGradient(const std::string& text, GradientDefinition& gradient) {
    const std::string value = lower(trim(text));
    if (value.rfind("linear-gradient(", 0) != 0 || value.back() != ')') return false;
    std::vector<std::string> parts;
    std::size_t position = 16;
    while (position < value.size() - 1) {
        const std::size_t comma = value.find(',', position);
        parts.push_back(trim(value.substr(position, (comma == std::string::npos ? value.size() - 1 : comma) - position)));
        if (comma == std::string::npos) break;
        position = comma + 1;
    }
    std::size_t colors = 0;
    if (parts.size() == 3 && (parts[0] == "to right" || parts[0] == "to bottom")) {
        gradient.horizontal = parts[0] == "to right";
        colors = 1;
    }
    return parts.size() == colors + 2 && parseColor(parts[colors], gradient.startColor) && parseColor(parts[colors + 1], gradient.endColor);
}

std::optional<std::string> parseImageUrl(const std::string& text) {
    const std::string value = trim(text);
    const std::string lowered = lower(value);
    if (lowered.rfind("url(", 0) != 0 || value.size() < 6 || value.back() != ')') return std::nullopt;
    std::string source = trim(value.substr(4, value.size() - 5));
    if (source.size() >= 2 && ((source.front() == '\'' && source.back() == '\'') || (source.front() == '"' && source.back() == '"')))
        source = source.substr(1, source.size() - 2);
    return source.empty() ? std::nullopt : std::optional<std::string>(source);
}

std::uint64_t hashString(const std::string& value) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (unsigned char character : value) { hash ^= character; hash *= 1099511628211ULL; }
    return hash;
}

} // namespace

class HtmlCssPipeline::Impl {
public:
    struct Node {
        std::string tag;
        std::unordered_map<std::string, std::string> attributes;
        std::string text;
        std::vector<std::unique_ptr<Node>> children;
        Node* parent = nullptr;
        std::size_t order = 0;
    };

    struct SimpleSelector {
        std::string tag;
        std::string id;
        std::vector<std::string> classes;
        std::uint8_t pseudoStates = 0;
        int specificity = 0;
    };

    struct Selector {
        std::vector<SimpleSelector> steps;
        int specificity = 0;
    };

    struct MediaCondition {
        enum class Kind { MinWidth, MaxWidth };
        Kind kind;
        double width = 0.0;
    };

    struct Rule {
        Selector selector;
        std::vector<std::pair<std::string, std::string>> declarations;
        std::size_t order = 0;
        std::optional<MediaCondition> media;
    };

    struct BoxShadow {
        Color color;
        Point offset;
        double blur = 0.0;
        double spread = 0.0;
    };

    struct Animation {
        std::string name;
        double duration = 0.0;
        double iterations = 1.0;
    };

    struct Keyframe {
        double offset = 0.0;
        std::vector<std::pair<std::string, std::string>> declarations;
    };

    enum class LayoutMode { Block, Flex, Grid };
    enum class FlexDirection { Row, Column };
    enum class JustifyContent { Start, Center, End, SpaceBetween };

    struct Style {
        bool display = true;
        bool absolute = false;
        bool cachedLayer = false;
        LayoutMode layoutMode = LayoutMode::Block;
        FlexDirection flexDirection = FlexDirection::Row;
        JustifyContent justifyContent = JustifyContent::Start;
        Color color = {0.0f, 0.0f, 0.0f, 1.0f};
        Color background = {0.0f, 0.0f, 0.0f, 0.0f};
        Color borderColor = {0.0f, 0.0f, 0.0f, 0.0f};
        float opacity = 1.0f;
        double fontSize = 16.0;
        double borderWidth = 0.0;
        double borderRadius = 0.0;
        double flexGrow = 0.0;
        double gap = 0.0;
        std::size_t gridColumns = 1;
        double transitionDuration = 0.0;
        bool transitionOpacity = false;
        bool transitionBackground = false;
        std::optional<Animation> animation;
        Length width;
        Length height;
        Length left;
        Length top;
        double marginTop = 0.0;
        double marginRight = 0.0;
        double marginBottom = 0.0;
        double marginLeft = 0.0;
        double paddingTop = 0.0;
        double paddingRight = 0.0;
        double paddingBottom = 0.0;
        double paddingLeft = 0.0;
        std::optional<BoxShadow> boxShadow;
        std::optional<GradientDefinition> gradient;
        std::string backgroundImage;
        std::unordered_map<std::string, std::string> variables;
    };

    struct LayoutNode {
        const Node* node = nullptr;
        Style style;
        Rect rect;
        std::vector<LayoutNode> children;
        bool isDocument = false;
    };

    std::unique_ptr<Node> root = std::make_unique<Node>();
    std::vector<Rule> rules;
    std::unordered_map<std::string, std::vector<Keyframe>> keyframes;
    ImageResolver imageResolver;
    std::string error;
    std::vector<CssCompatibilityWarning> warnings;
    std::unordered_map<HtmlCssNodeId, std::uint8_t> pseudoStates;
    struct HitTarget { HtmlCssNodeId id; Rect rect; };
    std::vector<HitTarget> hitTargets;
    std::uint64_t documentVersion = 0;
    double time = 0.0;
    struct TransitionState {
        float targetOpacity = 1.0f;
        Color targetBackground = {0.0f, 0.0f, 0.0f, 0.0f};
        float fromOpacity = 1.0f;
        Color fromBackground = {0.0f, 0.0f, 0.0f, 0.0f};
        double started = 0.0;
        bool initialized = false;
    };
    mutable std::unordered_map<HtmlCssNodeId, TransitionState> transitions;
    std::size_t nextNodeOrder = 1;

    bool load(std::string html, std::string css);
    bool parseHtml(const std::string& html);
    bool parseCss(const std::string& css);
    bool record(Render2DRecorder& recorder, const Size& viewport);

private:
    static std::unordered_map<std::string, std::string> parseAttributes(const std::string& text);
    static std::vector<std::pair<std::string, std::string>> parseDeclarations(const std::string& text);
    bool parseSelector(const std::string& text, Selector& selector);
    static bool parseSimpleSelector(const std::string& text, SimpleSelector& selector);
    bool matches(const Node& node, const Selector& selector) const;
    static std::vector<double> parseBoxValues(const std::string& value);
    static std::optional<BoxShadow> parseBoxShadow(const std::string& value);
    static std::optional<std::size_t> parseGridColumns(const std::string& value);
    static std::optional<Animation> parseAnimation(const std::string& value);
    static void applyBoxValues(const std::vector<double>& values, double& top, double& right,
                               double& bottom, double& left);
    static const std::string* attribute(const Node& node, const char* key);

    Style resolveStyle(const Node& node, const Style& parent, double viewportWidth) const;
    void applyDeclaration(Style& style, float& localOpacity, const std::string& property,
                           const std::string& value) const;
    LayoutNode layout(const Node& node, const Rect& parentContent, double flowY,
                        const Style& parentStyle, bool document, double viewportWidth,
                        std::optional<Point> positionedOrigin = std::nullopt,
                        std::optional<double> allocatedWidth = std::nullopt) const;
    void recordNode(const LayoutNode& node, Render2DRecorder& recorder) const;
    void collectHitTargets(const LayoutNode& node);
    std::uint64_t cacheKey(const LayoutNode& node) const;
    bool mediaMatches(const std::optional<MediaCondition>& media, double viewportWidth) const;
    std::string resolveVariables(const std::string& value, const std::unordered_map<std::string, std::string>& variables,
                                 std::vector<std::string>& stack) const;
    void warn(std::string subject, std::string message);
    void applyKeyframes(Style& style) const;
    void applyTransition(const Node& node, Style& style) const;
};

HtmlCssPipeline::HtmlCssPipeline() : m_impl(std::make_unique<Impl>()) {}
HtmlCssPipeline::~HtmlCssPipeline() = default;
HtmlCssPipeline::HtmlCssPipeline(HtmlCssPipeline&&) noexcept = default;
HtmlCssPipeline& HtmlCssPipeline::operator=(HtmlCssPipeline&&) noexcept = default;

bool HtmlCssPipeline::load(std::string html, std::string css) { return m_impl->load(std::move(html), std::move(css)); }
void HtmlCssPipeline::setImageResolver(ImageResolver resolver) { m_impl->imageResolver = std::move(resolver); }
void HtmlCssPipeline::setTime(double seconds) { m_impl->time = std::isfinite(seconds) ? std::max(0.0, seconds) : 0.0; }
std::optional<HtmlCssNodeId> HtmlCssPipeline::nodeIdForElementId(const std::string& elementId) const {
    std::vector<const Impl::Node*> nodes = {m_impl->root.get()};
    while (!nodes.empty()) {
        const Impl::Node* node = nodes.back(); nodes.pop_back();
        const auto id = node->attributes.find("id");
        if (id != node->attributes.end() && id->second == elementId) return node->order;
        for (const auto& child : node->children) nodes.push_back(child.get());
    }
    return std::nullopt;
}
bool HtmlCssPipeline::setPseudoState(HtmlCssNodeId node, CssPseudoState state, bool enabled) {
    if (node == 0) return false;
    const std::uint8_t bit = static_cast<std::uint8_t>(state);
    std::uint8_t& states = m_impl->pseudoStates[node];
    if (enabled) states |= bit; else states &= static_cast<std::uint8_t>(~bit);
    return true;
}
std::optional<HtmlCssNodeId> HtmlCssPipeline::hitTest(Point point) const {
    for (auto target = m_impl->hitTargets.rbegin(); target != m_impl->hitTargets.rend(); ++target) {
        if (point.x >= target->rect.origin.x && point.x <= target->rect.origin.x + target->rect.size.width &&
            point.y >= target->rect.origin.y && point.y <= target->rect.origin.y + target->rect.size.height) return target->id;
    }
    return std::nullopt;
}
bool HtmlCssPipeline::record(Render2DRecorder& recorder, const Size& viewport) { return m_impl->record(recorder, viewport); }
const std::string& HtmlCssPipeline::lastError() const { return m_impl->error; }
const std::vector<CssCompatibilityWarning>& HtmlCssPipeline::compatibilityWarnings() const { return m_impl->warnings; }

const std::string* HtmlCssPipeline::Impl::attribute(const Node& node, const char* key) {
    const auto found = node.attributes.find(key);
    return found == node.attributes.end() ? nullptr : &found->second;
}

std::unordered_map<std::string, std::string> HtmlCssPipeline::Impl::parseAttributes(const std::string& text) {
    std::unordered_map<std::string, std::string> result;
    std::size_t position = 0;
    while (position < text.size()) {
        while (position < text.size() && std::isspace(static_cast<unsigned char>(text[position])) != 0) ++position;
        const std::size_t nameStart = position;
        while (position < text.size() && !std::isspace(static_cast<unsigned char>(text[position])) && text[position] != '=') ++position;
        if (nameStart == position) break;
        const std::string name = lower(text.substr(nameStart, position - nameStart));
        while (position < text.size() && std::isspace(static_cast<unsigned char>(text[position])) != 0) ++position;
        std::string value;
        if (position < text.size() && text[position] == '=') {
            ++position;
            while (position < text.size() && std::isspace(static_cast<unsigned char>(text[position])) != 0) ++position;
            if (position < text.size() && (text[position] == '\'' || text[position] == '\"')) {
                const char quote = text[position++];
                const std::size_t valueStart = position;
                while (position < text.size() && text[position] != quote) ++position;
                value = text.substr(valueStart, position - valueStart);
                if (position < text.size()) ++position;
            } else {
                const std::size_t valueStart = position;
                while (position < text.size() && std::isspace(static_cast<unsigned char>(text[position])) == 0) ++position;
                value = text.substr(valueStart, position - valueStart);
            }
        }
        result[name] = decodeEntities(value);
    }
    return result;
}

std::vector<std::pair<std::string, std::string>> HtmlCssPipeline::Impl::parseDeclarations(const std::string& text) {
    std::vector<std::pair<std::string, std::string>> result;
    std::size_t position = 0;
    while (position < text.size()) {
        const std::size_t end = text.find(';', position);
        const std::string declaration = trim(text.substr(position, end == std::string::npos ? end : end - position));
        const std::size_t colon = declaration.find(':');
        if (colon != std::string::npos) result.emplace_back(lower(trim(declaration.substr(0, colon))), trim(declaration.substr(colon + 1)));
        if (end == std::string::npos) break;
        position = end + 1;
    }
    return result;
}

bool HtmlCssPipeline::Impl::parseSimpleSelector(const std::string& value, SimpleSelector& selector) {
    std::size_t position = 0;
    while (position < value.size()) {
        if (value[position] == '#') {
            const std::size_t start = ++position;
            while (position < value.size() && (std::isalnum(static_cast<unsigned char>(value[position])) != 0 || value[position] == '-' || value[position] == '_')) ++position;
            selector.id = value.substr(start, position - start); selector.specificity += 100;
        } else if (value[position] == '.') {
            const std::size_t start = ++position;
            while (position < value.size() && (std::isalnum(static_cast<unsigned char>(value[position])) != 0 || value[position] == '-' || value[position] == '_')) ++position;
            selector.classes.push_back(value.substr(start, position - start)); selector.specificity += 10;
        } else if (value[position] == ':') {
            const std::size_t start = ++position;
            while (position < value.size() && std::isalpha(static_cast<unsigned char>(value[position])) != 0) ++position;
            const std::string pseudo = lower(value.substr(start, position - start));
            if (pseudo == "hover") selector.pseudoStates |= static_cast<std::uint8_t>(CssPseudoState::Hover);
            else if (pseudo == "active") selector.pseudoStates |= static_cast<std::uint8_t>(CssPseudoState::Active);
            else if (pseudo == "focus") selector.pseudoStates |= static_cast<std::uint8_t>(CssPseudoState::Focus);
            else return false;
            selector.specificity += 10;
        } else {
            const std::size_t start = position;
            while (position < value.size() && value[position] != '.' && value[position] != '#' && value[position] != ':') ++position;
            selector.tag = lower(value.substr(start, position - start));
            if (selector.tag != "*") ++selector.specificity;
        }
    }
    return !selector.tag.empty() || !selector.id.empty() || !selector.classes.empty() || selector.pseudoStates != 0;
}

bool HtmlCssPipeline::Impl::parseSelector(const std::string& text, Selector& selector) {
    const std::string value = trim(text);
    if (value.empty() || value.find_first_of(">+~[") != std::string::npos) return false;
    std::size_t position = 0;
    while (position < value.size()) {
        while (position < value.size() && std::isspace(static_cast<unsigned char>(value[position])) != 0) ++position;
        const std::size_t start = position;
        while (position < value.size() && std::isspace(static_cast<unsigned char>(value[position])) == 0) ++position;
        if (start == position) break;
        SimpleSelector step;
        if (!parseSimpleSelector(value.substr(start, position - start), step)) return false;
        selector.specificity += step.specificity;
        selector.steps.push_back(std::move(step));
    }
    return !selector.steps.empty();
}

bool HtmlCssPipeline::Impl::matches(const Node& node, const Selector& selector) const {
    const auto matchesSimple = [&](const Node& candidate, const SimpleSelector& simple) {
        if (!simple.tag.empty() && simple.tag != "*" && candidate.tag != simple.tag) return false;
        if (!simple.id.empty()) {
            const std::string* id = attribute(candidate, "id");
            if (id == nullptr || *id != simple.id) return false;
        }
        if (simple.pseudoStates != 0) {
            const auto found = pseudoStates.find(candidate.order);
            if (found == pseudoStates.end() || (found->second & simple.pseudoStates) != simple.pseudoStates) return false;
        }
        const std::string* classValue = attribute(candidate, "class");
        for (const std::string& wanted : simple.classes) {
            if (classValue == nullptr) return false;
            std::istringstream stream(*classValue); std::string current; bool found = false;
            while (stream >> current) if (current == wanted) { found = true; break; }
            if (!found) return false;
        }
        return true;
    };
    if (selector.steps.empty() || !matchesSimple(node, selector.steps.back())) return false;
    const Node* ancestor = node.parent;
    for (std::size_t index = selector.steps.size() - 1; index > 0; --index) {
        while (ancestor != nullptr && !matchesSimple(*ancestor, selector.steps[index - 1])) ancestor = ancestor->parent;
        if (ancestor == nullptr) return false;
        ancestor = ancestor->parent;
    }
    return true;
}

std::vector<double> HtmlCssPipeline::Impl::parseBoxValues(const std::string& value) {
    std::istringstream stream(value);
    std::vector<double> result;
    std::string part;
    while (stream >> part && result.size() < 4) result.push_back(resolveLength(parseLength(part), 0.0));
    return result;
}

std::optional<HtmlCssPipeline::Impl::BoxShadow> HtmlCssPipeline::Impl::parseBoxShadow(const std::string& value) {
    if (lower(trim(value)) == "none") return std::nullopt;
    std::istringstream stream(value);
    std::vector<double> lengths;
    std::string part;
    Color color = {0.0f, 0.0f, 0.0f, 1.0f};
    bool hasColor = false;
    while (stream >> part) {
        Color parsed;
        if (parseColor(part, parsed)) { color = parsed; hasColor = true; }
        else if (part != "inset" && lengths.size() < 4) lengths.push_back(resolveLength(parseLength(part), 0.0));
        else return std::nullopt;
    }
    if (lengths.size() < 2 || !hasColor) return std::nullopt;
    return BoxShadow{color, {lengths[0], lengths[1]}, lengths.size() > 2 ? std::max(0.0, lengths[2]) : 0.0,
                     lengths.size() > 3 ? lengths[3] : 0.0};
}

std::optional<std::size_t> HtmlCssPipeline::Impl::parseGridColumns(const std::string& value) {
    const std::string normalized = lower(trim(value));
    if (normalized.rfind("repeat(", 0) == 0 && normalized.back() == ')') {
        const std::size_t comma = normalized.find(',');
        if (comma == std::string::npos || trim(normalized.substr(comma + 1, normalized.size() - comma - 2)) != "1fr") return std::nullopt;
        const double count = number(normalized.substr(7, comma - 7), 0.0);
        if (count >= 1.0 && count == std::floor(count)) return static_cast<std::size_t>(count);
        return std::nullopt;
    }
    std::istringstream stream(normalized);
    std::string track;
    std::size_t columns = 0;
    while (stream >> track) {
        if (track != "1fr") return std::nullopt;
        ++columns;
    }
    return columns > 0 ? std::optional<std::size_t>(columns) : std::nullopt;
}

std::optional<HtmlCssPipeline::Impl::Animation> HtmlCssPipeline::Impl::parseAnimation(const std::string& value) {
    std::istringstream stream(value);
    Animation animation;
    if (!(stream >> animation.name) || animation.name == "none") return std::nullopt;
    std::string part;
    while (stream >> part) {
        const double seconds = parseSeconds(part);
        if (seconds >= 0.0 && animation.duration == 0.0) animation.duration = seconds;
        else if (lower(part) == "infinite") animation.iterations = std::numeric_limits<double>::infinity();
        else if (number(part, -1.0) >= 0.0) animation.iterations = number(part);
    }
    return animation.duration > 0.0 ? std::optional<Animation>(animation) : std::nullopt;
}

void HtmlCssPipeline::Impl::applyBoxValues(const std::vector<double>& values, double& top, double& right,
                                            double& bottom, double& left) {
    if (values.empty()) return;
    top = values[0];
    right = values.size() > 1 ? values[1] : values[0];
    bottom = values.size() > 2 ? values[2] : values[0];
    left = values.size() > 3 ? values[3] : right;
}

void HtmlCssPipeline::Impl::applyDeclaration(Style& style, float& localOpacity, const std::string& property,
                                                const std::string& value) const {
    Color color;
    if (property == "display") {
        const std::string mode = lower(value);
        style.display = mode != "none";
        if (mode == "flex") style.layoutMode = LayoutMode::Flex;
        else if (mode == "grid") style.layoutMode = LayoutMode::Grid;
        else if (mode != "none") style.layoutMode = LayoutMode::Block;
    }
    else if (property == "position") style.absolute = lower(value) == "absolute";
    else if (property == "width") style.width = parseLength(value);
    else if (property == "height") style.height = parseLength(value);
    else if (property == "left") style.left = parseLength(value);
    else if (property == "top") style.top = parseLength(value);
    else if (property == "opacity") localOpacity = clampOpacity(number(value, 1.0));
    else if (property == "font-size") style.fontSize = std::max(0.0, resolveLength(parseLength(value), 0.0));
    else if (property == "color" && parseColor(value, color)) style.color = color;
    else if ((property == "background" || property == "background-color") && parseColor(value, color)) {
        style.background = color;
        style.gradient.reset();
        style.backgroundImage.clear();
    } else if (property == "background" || property == "background-image") {
        GradientDefinition gradient;
        if (parseLinearGradient(value, gradient)) {
            style.gradient = gradient;
            style.backgroundImage.clear();
        } else if (const auto image = parseImageUrl(value)) {
            style.backgroundImage = *image;
            style.gradient.reset();
        }
    } else if (property == "border-radius") style.borderRadius = std::max(0.0, resolveLength(parseLength(value), 0.0));
    else if (property == "border-width") style.borderWidth = std::max(0.0, resolveLength(parseLength(value), 0.0));
    else if (property == "border-color" && parseColor(value, color)) style.borderColor = color;
    else if (property == "border") {
        std::istringstream stream(value);
        std::string part;
        while (stream >> part) {
            Color parsed;
            if (parseColor(part, parsed)) style.borderColor = parsed;
            else if (part != "solid") style.borderWidth = std::max(0.0, resolveLength(parseLength(part), 0.0));
        }
    } else if (property == "box-shadow") style.boxShadow = parseBoxShadow(value);
    else if (property == "flex-direction") style.flexDirection = lower(value) == "column" ? FlexDirection::Column : FlexDirection::Row;
    else if (property == "flex" || property == "flex-grow") style.flexGrow = std::max(0.0, number(value));
    else if (property == "justify-content") {
        const std::string alignment = lower(value);
        if (alignment == "center") style.justifyContent = JustifyContent::Center;
        else if (alignment == "end" || alignment == "flex-end") style.justifyContent = JustifyContent::End;
        else if (alignment == "space-between") style.justifyContent = JustifyContent::SpaceBetween;
        else style.justifyContent = JustifyContent::Start;
    } else if (property == "gap" || property == "grid-gap") style.gap = std::max(0.0, resolveLength(parseLength(value), 0.0));
    else if (property == "grid-template-columns") {
        if (const auto columns = parseGridColumns(value)) style.gridColumns = *columns;
    } else if (property == "animation") style.animation = parseAnimation(value);
    else if (property == "transition") {
        std::istringstream stream(value);
        std::string propertyName;
        std::string part;
        stream >> propertyName;
        while (stream >> part) {
            const double seconds = parseSeconds(part);
            if (seconds >= 0.0) { style.transitionDuration = seconds; break; }
        }
        propertyName = lower(propertyName);
        style.transitionOpacity = propertyName == "opacity" || propertyName == "all";
        style.transitionBackground = propertyName == "background" || propertyName == "background-color" || propertyName == "all";
    }
    else if (property == "margin") applyBoxValues(parseBoxValues(value), style.marginTop, style.marginRight, style.marginBottom, style.marginLeft);
    else if (property == "padding") applyBoxValues(parseBoxValues(value), style.paddingTop, style.paddingRight, style.paddingBottom, style.paddingLeft);
    else if (property == "margin-top") style.marginTop = resolveLength(parseLength(value), 0.0);
    else if (property == "margin-right") style.marginRight = resolveLength(parseLength(value), 0.0);
    else if (property == "margin-bottom") style.marginBottom = resolveLength(parseLength(value), 0.0);
    else if (property == "margin-left") style.marginLeft = resolveLength(parseLength(value), 0.0);
    else if (property == "padding-top") style.paddingTop = resolveLength(parseLength(value), 0.0);
    else if (property == "padding-right") style.paddingRight = resolveLength(parseLength(value), 0.0);
    else if (property == "padding-bottom") style.paddingBottom = resolveLength(parseLength(value), 0.0);
    else if (property == "padding-left") style.paddingLeft = resolveLength(parseLength(value), 0.0);
    else if (property == "render-mode") style.cachedLayer = lower(value) == "cached-layer";
}

bool HtmlCssPipeline::Impl::mediaMatches(const std::optional<MediaCondition>& media, double viewportWidth) const {
    if (!media) return true;
    return media->kind == MediaCondition::Kind::MinWidth ? viewportWidth >= media->width : viewportWidth <= media->width;
}

std::string HtmlCssPipeline::Impl::resolveVariables(const std::string& value, const std::unordered_map<std::string, std::string>& variables,
                                                    std::vector<std::string>& stack) const {
    std::string resolved = value;
    std::size_t start = 0;
    while ((start = resolved.find("var(", start)) != std::string::npos) {
        std::size_t end = start + 4;
        int depth = 1;
        while (end < resolved.size() && depth > 0) {
            if (resolved[end] == '(') ++depth;
            else if (resolved[end] == ')') --depth;
            ++end;
        }
        if (depth != 0) return value;
        const std::string inside = trim(resolved.substr(start + 4, end - start - 5));
        const std::size_t comma = inside.find(',');
        const std::string name = trim(inside.substr(0, comma));
        const std::string fallback = comma == std::string::npos ? std::string{} : trim(inside.substr(comma + 1));
        std::string replacement = fallback;
        const auto found = variables.find(name);
        if (found != variables.end() && std::find(stack.begin(), stack.end(), name) == stack.end()) {
            stack.push_back(name);
            replacement = resolveVariables(found->second, variables, stack);
            stack.pop_back();
        }
        if (replacement.empty() && found == variables.end() && fallback.empty()) return value;
        resolved.replace(start, end - start, replacement);
        start += replacement.size();
    }
    return resolved;
}

void HtmlCssPipeline::Impl::applyKeyframes(Style& style) const {
    if (!style.animation) return;
    const auto found = keyframes.find(style.animation->name);
    if (found == keyframes.end() || found->second.empty()) return;
    double progress = time / style.animation->duration;
    if (std::isfinite(style.animation->iterations)) {
        if (progress >= style.animation->iterations) progress = 1.0;
        else progress -= std::floor(progress);
    } else progress -= std::floor(progress);
    const std::vector<Keyframe>& frames = found->second;
    const Keyframe* before = &frames.front();
    const Keyframe* after = &frames.back();
    for (const Keyframe& frame : frames) {
        if (frame.offset <= progress) before = &frame;
        if (frame.offset >= progress) { after = &frame; break; }
    }
    const double amount = after->offset <= before->offset ? 0.0 : (progress - before->offset) / (after->offset - before->offset);
    const auto declaration = [](const Keyframe& frame, const char* name) -> const std::string* {
        for (auto item = frame.declarations.rbegin(); item != frame.declarations.rend(); ++item)
            if (item->first == name) return &item->second;
        return nullptr;
    };
    if (const std::string* from = declaration(*before, "opacity")) if (const std::string* to = declaration(*after, "opacity"))
        style.opacity = clampOpacity(number(*from) + (number(*to) - number(*from)) * amount);
    if (const std::string* from = declaration(*before, "background-color")) if (const std::string* to = declaration(*after, "background-color")) {
        Color start;
        Color end;
        if (parseColor(*from, start) && parseColor(*to, end)) {
            style.background = {static_cast<float>(start.r + (end.r - start.r) * amount), static_cast<float>(start.g + (end.g - start.g) * amount),
                                static_cast<float>(start.b + (end.b - start.b) * amount), static_cast<float>(start.a + (end.a - start.a) * amount)};
        }
    }
}

void HtmlCssPipeline::Impl::applyTransition(const Node& node, Style& style) const {
    if (style.animation || style.transitionDuration <= 0.0 || (!style.transitionOpacity && !style.transitionBackground)) return;
    TransitionState& state = transitions[node.order];
    if (!state.initialized) {
        state.targetOpacity = state.fromOpacity = style.opacity;
        state.targetBackground = state.fromBackground = style.background;
        state.started = time;
        state.initialized = true;
        return;
    }
    const double amount = std::clamp((time - state.started) / style.transitionDuration, 0.0, 1.0);
    const float currentOpacity = static_cast<float>(state.fromOpacity + (state.targetOpacity - state.fromOpacity) * amount);
    const Color currentBackground = {static_cast<float>(state.fromBackground.r + (state.targetBackground.r - state.fromBackground.r) * amount),
                                     static_cast<float>(state.fromBackground.g + (state.targetBackground.g - state.fromBackground.g) * amount),
                                     static_cast<float>(state.fromBackground.b + (state.targetBackground.b - state.fromBackground.b) * amount),
                                     static_cast<float>(state.fromBackground.a + (state.targetBackground.a - state.fromBackground.a) * amount)};
    const bool opacityChanged = style.opacity != state.targetOpacity;
    const bool backgroundChanged = style.background.r != state.targetBackground.r || style.background.g != state.targetBackground.g ||
        style.background.b != state.targetBackground.b || style.background.a != state.targetBackground.a;
    if (opacityChanged || backgroundChanged) {
        state.fromOpacity = currentOpacity;
        state.fromBackground = currentBackground;
        state.targetOpacity = style.opacity;
        state.targetBackground = style.background;
        state.started = time;
    }
    if (style.transitionOpacity) style.opacity = state.fromOpacity + (state.targetOpacity - state.fromOpacity) * static_cast<float>(std::clamp((time - state.started) / style.transitionDuration, 0.0, 1.0));
    if (style.transitionBackground) {
        const double t = std::clamp((time - state.started) / style.transitionDuration, 0.0, 1.0);
        style.background = {static_cast<float>(state.fromBackground.r + (state.targetBackground.r - state.fromBackground.r) * t),
                            static_cast<float>(state.fromBackground.g + (state.targetBackground.g - state.fromBackground.g) * t),
                            static_cast<float>(state.fromBackground.b + (state.targetBackground.b - state.fromBackground.b) * t),
                            static_cast<float>(state.fromBackground.a + (state.targetBackground.a - state.fromBackground.a) * t)};
    }
}

HtmlCssPipeline::Impl::Style HtmlCssPipeline::Impl::resolveStyle(const Node& node, const Style& parent, double viewportWidth) const {
    Style style;
    style.color = parent.color;
    style.fontSize = parent.fontSize;
    style.variables = parent.variables;
    float localOpacity = 1.0f;
    std::vector<const Rule*> matchesRules;
    for (const Rule& rule : rules) if (mediaMatches(rule.media, viewportWidth) && matches(node, rule.selector)) matchesRules.push_back(&rule);
    std::stable_sort(matchesRules.begin(), matchesRules.end(), [](const Rule* left, const Rule* right) {
        return left->selector.specificity == right->selector.specificity ? left->order < right->order : left->selector.specificity < right->selector.specificity;
    });
    std::vector<std::pair<std::string, std::string>> declarations;
    for (const Rule* rule : matchesRules) {
        declarations.insert(declarations.end(), rule->declarations.begin(), rule->declarations.end());
    }
    if (const std::string* inlineStyle = attribute(node, "style")) {
        const auto inlineDeclarations = parseDeclarations(*inlineStyle);
        declarations.insert(declarations.end(), inlineDeclarations.begin(), inlineDeclarations.end());
    }
    for (const auto& declaration : declarations) if (declaration.first.rfind("--", 0) == 0) style.variables[declaration.first] = declaration.second;
    for (const auto& declaration : declarations) if (declaration.first.rfind("--", 0) != 0) {
        std::vector<std::string> stack;
        applyDeclaration(style, localOpacity, declaration.first, resolveVariables(declaration.second, style.variables, stack));
    }
    if (node.tag == "style") style.display = false;
    style.opacity = parent.opacity * localOpacity;
    applyKeyframes(style);
    applyTransition(node, style);
    return style;
}

HtmlCssPipeline::Impl::LayoutNode HtmlCssPipeline::Impl::layout(const Node& node, const Rect& parentContent,
                                                                    double flowY, const Style& parentStyle,
                                                                    bool document, double viewportWidth,
                                                                    std::optional<Point> positionedOrigin,
                                                                    std::optional<double> allocatedWidth) const {
    LayoutNode result;
    result.node = &node;
    result.isDocument = document;
    result.style = document ? parentStyle : resolveStyle(node, parentStyle, viewportWidth);
    if (!result.style.display) return result;

    const Style& style = result.style;
    const double parentWidth = std::max(0.0, parentContent.size.width);
    const double parentHeight = std::max(0.0, parentContent.size.height);
    const double defaultX = document ? parentContent.origin.x : parentContent.origin.x +
        (style.absolute ? resolveLength(style.left, parentWidth) : style.marginLeft);
    const double defaultY = document ? parentContent.origin.y : (style.absolute ? parentContent.origin.y + resolveLength(style.top, parentHeight) : flowY + style.marginTop);
    const double x = positionedOrigin ? positionedOrigin->x : defaultX;
    const double y = positionedOrigin ? positionedOrigin->y : defaultY;
    const double width = document ? parentWidth : (allocatedWidth ? std::max(0.0, *allocatedWidth) : std::max(0.0, style.width.unit == Length::Unit::Auto
        ? parentWidth - style.marginLeft - style.marginRight : resolveLength(style.width, parentWidth)));
    const double contentWidth = std::max(0.0, width - style.paddingLeft - style.paddingRight);
    const double specifiedContentHeight = style.height.unit == Length::Unit::Auto ? 0.0 :
        std::max(0.0, resolveLength(style.height, parentHeight) - style.paddingTop - style.paddingBottom);
    const double contentX = x + style.paddingLeft;
    const double contentY = y + style.paddingTop;
    double childFlowY = contentY;
    const Rect childParent = {{contentX, contentY}, {contentWidth, specifiedContentHeight}};
    struct ChildInfo { const Node* node; Style style; };
    std::vector<ChildInfo> normalChildren;
    for (const std::unique_ptr<Node>& child : node.children) {
        Style childStyle = resolveStyle(*child, style, viewportWidth);
        if (childStyle.display && !childStyle.absolute) normalChildren.push_back({child.get(), std::move(childStyle)});
        else result.children.push_back(layout(*child, childParent, childFlowY, style, false, viewportWidth));
    }
    const auto appendChild = [&](const ChildInfo& child, Point origin, std::optional<double> widthOverride) {
        LayoutNode childLayout = layout(*child.node, childParent, origin.y, style, false, viewportWidth, origin, widthOverride);
        childFlowY = std::max(childFlowY, childLayout.rect.origin.y + childLayout.rect.size.height + childLayout.style.marginBottom);
        result.children.push_back(std::move(childLayout));
    };
    if (style.layoutMode == LayoutMode::Flex && style.flexDirection == FlexDirection::Row && !normalChildren.empty()) {
        const double gaps = style.gap * static_cast<double>(normalChildren.size() - 1);
        double margins = 0.0;
        double fixed = 0.0;
        double grow = 0.0;
        std::size_t automatic = 0;
        for (const ChildInfo& child : normalChildren) {
            margins += child.style.marginLeft + child.style.marginRight;
            if (child.style.width.unit == Length::Unit::Auto) ++automatic;
            else fixed += std::max(0.0, resolveLength(child.style.width, contentWidth));
            grow += child.style.flexGrow;
        }
        const double available = std::max(0.0, contentWidth - gaps - margins);
        const double extra = std::max(0.0, available - fixed);
        double used = fixed;
        std::vector<double> widths;
        widths.reserve(normalChildren.size());
        for (const ChildInfo& child : normalChildren) {
            double itemWidth = child.style.width.unit == Length::Unit::Auto ? 0.0 : std::max(0.0, resolveLength(child.style.width, contentWidth));
            if (grow > 0.0) itemWidth += extra * child.style.flexGrow / grow;
            else if (child.style.width.unit == Length::Unit::Auto && automatic > 0) itemWidth = extra / automatic;
            used += itemWidth - (child.style.width.unit == Length::Unit::Auto ? 0.0 : std::max(0.0, resolveLength(child.style.width, contentWidth)));
            widths.push_back(itemWidth);
        }
        double spacing = style.gap;
        double cursor = contentX;
        const double remaining = std::max(0.0, contentWidth - margins - used - gaps);
        if (style.justifyContent == JustifyContent::Center) cursor += remaining / 2.0;
        else if (style.justifyContent == JustifyContent::End) cursor += remaining;
        else if (style.justifyContent == JustifyContent::SpaceBetween && normalChildren.size() > 1) spacing += remaining / static_cast<double>(normalChildren.size() - 1);
        for (std::size_t index = 0; index < normalChildren.size(); ++index) {
            const ChildInfo& child = normalChildren[index];
            cursor += child.style.marginLeft;
            appendChild(child, {cursor, contentY + child.style.marginTop}, widths[index]);
            cursor += widths[index] + child.style.marginRight + spacing;
        }
    } else if (style.layoutMode == LayoutMode::Flex && !normalChildren.empty()) {
        for (std::size_t index = 0; index < normalChildren.size(); ++index) {
            const ChildInfo& child = normalChildren[index];
            appendChild(child, {contentX + child.style.marginLeft, childFlowY + child.style.marginTop}, std::nullopt);
            if (index + 1 < normalChildren.size()) childFlowY += style.gap;
        }
    } else if (style.layoutMode == LayoutMode::Grid && !normalChildren.empty()) {
        const std::size_t columns = std::max<std::size_t>(1, style.gridColumns);
        const double cellWidth = std::max(0.0, (contentWidth - style.gap * static_cast<double>(columns - 1)) / columns);
        double rowY = contentY;
        double rowBottom = rowY;
        for (std::size_t index = 0; index < normalChildren.size(); ++index) {
            const ChildInfo& child = normalChildren[index];
            const std::size_t column = index % columns;
            if (column == 0 && index > 0) { rowY = rowBottom + style.gap; rowBottom = rowY; }
            const double itemWidth = std::max(0.0, cellWidth - child.style.marginLeft - child.style.marginRight);
            const Point origin = {contentX + column * (cellWidth + style.gap) + child.style.marginLeft, rowY + child.style.marginTop};
            appendChild(child, origin, itemWidth);
            rowBottom = std::max(rowBottom, result.children.back().rect.origin.y + result.children.back().rect.size.height + child.style.marginBottom);
        }
    } else {
        for (const ChildInfo& child : normalChildren) {
            LayoutNode childLayout = layout(*child.node, childParent, childFlowY, style, false, viewportWidth);
            childFlowY = childLayout.rect.origin.y + childLayout.rect.size.height + childLayout.style.marginBottom;
            result.children.push_back(std::move(childLayout));
        }
    }
    const bool image = node.tag == "img";
    const double textHeight = node.text.empty() ? 0.0 : style.fontSize * 1.2;
    const double imageHeight = image && specifiedContentHeight == 0.0 ? 32.0 : 0.0;
    const double childHeight = std::max(0.0, childFlowY - contentY);
    const double contentHeight = document ? parentHeight : (style.height.unit == Length::Unit::Auto
        ? std::max({textHeight, imageHeight, childHeight}) : specifiedContentHeight);
    result.rect = {{x, y}, {width, contentHeight + style.paddingTop + style.paddingBottom}};
    return result;
}

std::uint64_t HtmlCssPipeline::Impl::cacheKey(const LayoutNode& node) const {
    if (const std::string* id = attribute(*node.node, "id")) return hashString(*id) | 1ULL;
    return static_cast<std::uint64_t>(node.node->order) + 1ULL;
}

void HtmlCssPipeline::Impl::recordNode(const LayoutNode& node, Render2DRecorder& recorder) const {
    if (!node.style.display) return;
    recorder.save();
    recorder.setOpacity(node.style.opacity);
    const bool cached = node.style.cachedLayer;
    if (cached && !recorder.beginCachedLayer(cacheKey(node), node.rect, documentVersion)) return;
    if (!node.isDocument) {
        if (node.style.boxShadow) {
            const BoxShadow& shadow = *node.style.boxShadow;
            recorder.drawBoxShadow(node.rect, shadow.color, shadow.offset, shadow.blur, shadow.spread, node.style.borderRadius);
        }
        if (node.style.gradient) {
            const GradientDefinition& gradient = *node.style.gradient;
            const Point start = gradient.horizontal ? Point{node.rect.origin.x, node.rect.origin.y} : Point{node.rect.origin.x, node.rect.origin.y};
            const Point end = gradient.horizontal ? Point{node.rect.origin.x + node.rect.size.width, node.rect.origin.y}
                                                  : Point{node.rect.origin.x, node.rect.origin.y + node.rect.size.height};
            recorder.fillLinearGradient(node.rect, start, end, gradient.startColor, gradient.endColor, node.style.borderRadius);
        } else if (node.style.background.a > 0.0f) {
            if (node.style.borderRadius > 0.0) recorder.fillRoundedRect(node.rect, node.style.background, node.style.borderRadius);
            else recorder.fillRect(node.rect, node.style.background);
        }
        if (!node.style.backgroundImage.empty() && imageResolver) {
            recorder.drawImage(imageResolver(node.style.backgroundImage), node.rect);
        }
        if (node.style.borderWidth > 0.0 && node.style.borderColor.a > 0.0f) {
            const double inset = node.style.borderWidth / 2.0;
            const Rect borderRect = {{node.rect.origin.x + inset, node.rect.origin.y + inset},
                                    {std::max(0.0, node.rect.size.width - node.style.borderWidth),
                                     std::max(0.0, node.rect.size.height - node.style.borderWidth)}};
            if (node.style.borderRadius > 0.0)
                recorder.strokeRoundedRect(borderRect, node.style.borderColor, std::max(0.0, node.style.borderRadius - inset), node.style.borderWidth);
            else recorder.strokeRect(borderRect, node.style.borderColor, node.style.borderWidth);
        }
    }

    const double contentX = node.rect.origin.x + node.style.paddingLeft;
    const double contentY = node.rect.origin.y + node.style.paddingTop;
    if (!node.node->text.empty() && node.node->tag != "style") {
        recorder.drawText(0, node.node->text, {contentX, contentY + node.style.fontSize}, node.style.fontSize, node.style.color);
    }
    if (node.node->tag == "img" && imageResolver) {
        if (const std::string* source = attribute(*node.node, "src")) {
            const double width = node.rect.size.width - node.style.paddingLeft - node.style.paddingRight;
            const double height = node.rect.size.height - node.style.paddingTop - node.style.paddingBottom;
            recorder.drawImage(imageResolver(*source), {{contentX, contentY}, {std::max(0.0, width), std::max(0.0, height)}});
        }
    }
    for (const LayoutNode& child : node.children) recordNode(child, recorder);
    if (cached) recorder.endCachedLayer();
    recorder.restore();
}

void HtmlCssPipeline::Impl::collectHitTargets(const LayoutNode& node) {
    if (!node.style.display) return;
    if (!node.isDocument && node.rect.size.width > 0.0 && node.rect.size.height > 0.0)
        hitTargets.push_back({node.node->order, node.rect});
    for (const LayoutNode& child : node.children) collectHitTargets(child);
}

bool HtmlCssPipeline::Impl::record(Render2DRecorder& recorder, const Size& viewport) {
    error.clear();
    if (root == nullptr) { error = "No HTML document has been loaded"; return false; }
    Style rootStyle;
    LayoutNode layoutRoot = layout(*root, {{0.0, 0.0}, viewport}, 0.0, rootStyle, true, viewport.width);
    hitTargets.clear();
    collectHitTargets(layoutRoot);
    recordNode(layoutRoot, recorder);
    if (!recorder.lastError().empty()) { error = recorder.lastError(); return false; }
    return true;
}

bool HtmlCssPipeline::Impl::parseHtml(const std::string& html) {
    root = std::make_unique<Node>();
    root->tag = "document";
    root->order = 0;
    std::vector<Node*> stack = {root.get()};
    std::size_t position = 0;
    while (position < html.size()) {
        const std::size_t open = html.find('<', position);
        const std::string text = collapseWhitespace(decodeEntities(html.substr(position, open == std::string::npos ? open : open - position)));
        if (!text.empty() && !stack.empty()) {
            if (!stack.back()->text.empty()) stack.back()->text += ' ';
            stack.back()->text += text;
        }
        if (open == std::string::npos) break;
        if (html.compare(open, 4, "<!--") == 0) {
            const std::size_t close = html.find("-->", open + 4);
            position = close == std::string::npos ? html.size() : close + 3;
            continue;
        }
        const std::size_t close = html.find('>', open + 1);
        if (close == std::string::npos) { error = "Unterminated HTML tag"; return false; }
        std::string tagText = trim(html.substr(open + 1, close - open - 1));
        if (!tagText.empty() && tagText.front() == '/') {
            const std::string closingTag = lower(trim(tagText.substr(1)));
            for (std::size_t index = stack.size(); index > 1; --index) {
                if (stack[index - 1]->tag == closingTag) { stack.resize(index - 1); break; }
            }
        } else if (!tagText.empty() && tagText.front() != '!') {
            const bool selfClosing = tagText.back() == '/';
            if (selfClosing) tagText = trim(tagText.substr(0, tagText.size() - 1));
            const std::size_t split = tagText.find_first_of(" \t\r\n");
            const std::string tag = lower(tagText.substr(0, split));
            if (tag.empty()) { error = "HTML tag is missing a name"; return false; }
            auto node = std::make_unique<Node>();
            node->tag = tag;
            node->order = nextNodeOrder++;
            node->parent = stack.back();
            node->attributes = parseAttributes(split == std::string::npos ? "" : tagText.substr(split + 1));
            Node* rawNode = node.get();
            stack.back()->children.push_back(std::move(node));
            if (!selfClosing && !isVoidElement(tag)) stack.push_back(rawNode);
        }
        position = close + 1;
    }
    return true;
}

bool HtmlCssPipeline::Impl::parseCss(const std::string& rawCss) {
    rules.clear();
    keyframes.clear();
    std::string css = rawCss;
    std::size_t commentStart = 0;
    while ((commentStart = css.find("/*", commentStart)) != std::string::npos) {
        const std::size_t commentEnd = css.find("*/", commentStart + 2);
        css.erase(commentStart, (commentEnd == std::string::npos ? css.size() : commentEnd + 2) - commentStart);
    }
    std::size_t ruleOrder = 0;
    const auto parseMedia = [&](const std::string& header) -> std::optional<MediaCondition> {
        const std::string condition = lower(trim(header));
        const bool minimum = condition.find("min-width") != std::string::npos;
        const bool maximum = condition.find("max-width") != std::string::npos;
        const std::size_t colon = condition.find(':');
        const std::size_t close = condition.rfind(')');
        if ((!minimum && !maximum) || colon == std::string::npos || close == std::string::npos || close <= colon) return std::nullopt;
        const Length width = parseLength(condition.substr(colon + 1, close - colon - 1));
        if (width.unit != Length::Unit::Linear || width.percent != 0.0) return std::nullopt;
        return MediaCondition{minimum ? MediaCondition::Kind::MinWidth : MediaCondition::Kind::MaxWidth, width.pixels};
    };
    const auto isSupportedDeclaration = [](const std::string& property) {
        return property.rfind("--", 0) == 0 || property == "display" || property == "position" || property == "width" ||
            property == "height" || property == "left" || property == "top" || property == "opacity" || property == "font-size" ||
            property == "color" || property == "background" || property == "background-color" || property == "margin" ||
            property == "padding" || property == "margin-top" || property == "margin-right" || property == "margin-bottom" ||
            property == "margin-left" || property == "padding-top" || property == "padding-right" || property == "padding-bottom" ||
             property == "padding-left" || property == "render-mode" || property == "border" || property == "border-width" ||
             property == "border-color" || property == "border-radius" || property == "box-shadow" || property == "background-image" ||
             property == "flex-direction" || property == "flex" || property == "flex-grow" || property == "justify-content" ||
             property == "gap" || property == "grid-gap" || property == "grid-template-columns" || property == "animation" || property == "transition";
    };
    const auto parseKeyframes = [&](const std::string& name, const std::string& source) {
        std::vector<Keyframe> frames;
        std::size_t position = 0;
        while (position < source.size()) {
            while (position < source.size() && std::isspace(static_cast<unsigned char>(source[position])) != 0) ++position;
            const std::size_t open = source.find('{', position);
            if (open == std::string::npos) break;
            const std::string label = lower(trim(source.substr(position, open - position)));
            const std::size_t close = source.find('}', open + 1);
            if (close == std::string::npos) { warn(name, "Unterminated @keyframes block"); return; }
            double offset = -1.0;
            if (label == "from") offset = 0.0;
            else if (label == "to") offset = 1.0;
            else if (!label.empty() && label.back() == '%') offset = number(label.substr(0, label.size() - 1), -1.0) / 100.0;
            if (offset < 0.0 || offset > 1.0) warn(label, "Unsupported @keyframes offset");
            else frames.push_back({offset, parseDeclarations(source.substr(open + 1, close - open - 1))});
            position = close + 1;
        }
        if (frames.empty()) warn(name, "@keyframes requires at least one supported frame");
        else {
            std::sort(frames.begin(), frames.end(), [](const Keyframe& left, const Keyframe& right) { return left.offset < right.offset; });
            keyframes[name] = std::move(frames);
        }
    };
    std::function<bool(const std::string&, std::optional<MediaCondition>)> parseRules;
    parseRules = [&](const std::string& source, std::optional<MediaCondition> media) {
        std::size_t position = 0;
        while (position < source.size()) {
            while (position < source.size() && std::isspace(static_cast<unsigned char>(source[position])) != 0) ++position;
            if (position == source.size()) break;
            const std::size_t open = source.find('{', position);
            if (open == std::string::npos) { warn(trim(source.substr(position)), "Ignored trailing CSS text"); break; }
            const std::string header = trim(source.substr(position, open - position));
            std::size_t close = open + 1; int depth = 1;
            while (close < source.size() && depth > 0) {
                if (source[close] == '{') ++depth;
                else if (source[close] == '}') --depth;
                ++close;
            }
            if (depth != 0) { error = "Unterminated CSS rule"; return false; }
            const std::string body = source.substr(open + 1, close - open - 2);
            if (header.rfind("@media", 0) == 0) {
                const auto condition = parseMedia(header.substr(6));
                if (!condition) warn(header, "Only @media min-width/max-width pixel conditions are supported");
                else if (!parseRules(body, condition)) return false;
            } else if (header.rfind("@keyframes", 0) == 0) {
                const std::string name = trim(header.substr(10));
                if (name.empty()) warn(header, "@keyframes requires a name");
                else parseKeyframes(name, body);
            } else if (header.empty() || header.front() == '@') {
                warn(header, "Unsupported CSS at-rule");
            } else {
                const auto declarations = parseDeclarations(body);
                for (const auto& declaration : declarations) if (!isSupportedDeclaration(declaration.first)) warn(declaration.first, "Unsupported CSS declaration was ignored");
                std::size_t selectorStart = 0;
                while (selectorStart < header.size()) {
                    const std::size_t comma = header.find(',', selectorStart);
                    const std::string selectorText = trim(header.substr(selectorStart, comma == std::string::npos ? std::string::npos : comma - selectorStart));
                    Selector selector;
                    if (parseSelector(selectorText, selector)) rules.push_back({std::move(selector), declarations, ruleOrder++, media});
                    else warn(selectorText, "Unsupported CSS selector was ignored");
                    if (comma == std::string::npos) break;
                    selectorStart = comma + 1;
                }
            }
            position = close;
        }
        return true;
    };
    return parseRules(css, std::nullopt);
}

bool HtmlCssPipeline::Impl::load(std::string html, std::string css) {
    error.clear();
    warnings.clear();
    pseudoStates.clear();
    transitions.clear();
    hitTargets.clear();
    nextNodeOrder = 1;
    if (!parseHtml(html)) return false;
    std::vector<const Node*> pending = {root.get()};
    while (!pending.empty()) {
        const Node* node = pending.back();
        pending.pop_back();
        if (node->tag == "style") css += '\n' + node->text;
        for (const std::unique_ptr<Node>& child : node->children) pending.push_back(child.get());
    }
    if (!parseCss(css)) return false;
    ++documentVersion;
    return true;
}

void HtmlCssPipeline::Impl::warn(std::string subject, std::string message) {
    warnings.push_back({std::move(subject), std::move(message)});
}

} // namespace appgametoolbox
