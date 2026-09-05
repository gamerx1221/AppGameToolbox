#include <AppGameToolbox/HtmlCss.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
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
    enum class Unit { Auto, Pixels, Percent };
    Unit unit = Unit::Auto;
    double value = 0.0;
};

Length parseLength(const std::string& text) {
    const std::string value = lower(trim(text));
    if (value.empty() || value == "auto") return {};
    if (value.back() == '%') return {Length::Unit::Percent, number(value.substr(0, value.size() - 1))};
    if (value.size() > 2 && value.substr(value.size() - 2) == "px") return {Length::Unit::Pixels, number(value.substr(0, value.size() - 2))};
    return {Length::Unit::Pixels, number(value)};
}

double resolveLength(Length length, double available, double fallback = 0.0) {
    if (length.unit == Length::Unit::Pixels) return length.value;
    if (length.unit == Length::Unit::Percent) return available * length.value / 100.0;
    return fallback;
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
    if (value == "transparent") { color = {0.0f, 0.0f, 0.0f, 0.0f}; return true; }
    if (value == "black") { color = {0.0f, 0.0f, 0.0f, 1.0f}; return true; }
    if (value == "white") { color = {1.0f, 1.0f, 1.0f, 1.0f}; return true; }
    if (value == "red") { color = {1.0f, 0.0f, 0.0f, 1.0f}; return true; }
    if (value == "green") { color = {0.0f, 0.5f, 0.0f, 1.0f}; return true; }
    if (value == "blue") { color = {0.0f, 0.0f, 1.0f, 1.0f}; return true; }
    if (value == "gray" || value == "grey") { color = {0.5f, 0.5f, 0.5f, 1.0f}; return true; }
    return false;
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
        std::size_t order = 0;
    };

    struct Selector {
        std::string tag;
        std::string id;
        std::vector<std::string> classes;
        int specificity = 0;
    };

    struct Rule {
        Selector selector;
        std::vector<std::pair<std::string, std::string>> declarations;
        std::size_t order = 0;
    };

    struct Style {
        bool display = true;
        bool absolute = false;
        bool cachedLayer = false;
        Color color = {0.0f, 0.0f, 0.0f, 1.0f};
        Color background = {0.0f, 0.0f, 0.0f, 0.0f};
        float opacity = 1.0f;
        double fontSize = 16.0;
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
    ImageResolver imageResolver;
    std::string error;
    std::uint64_t documentVersion = 0;
    std::size_t nextNodeOrder = 1;

    bool load(std::string html, std::string css);
    bool parseHtml(const std::string& html);
    bool parseCss(const std::string& css);
    bool record(Render2DRecorder& recorder, const Size& viewport);

private:
    static std::unordered_map<std::string, std::string> parseAttributes(const std::string& text);
    static std::vector<std::pair<std::string, std::string>> parseDeclarations(const std::string& text);
    static bool parseSelector(const std::string& text, Selector& selector);
    static bool matches(const Node& node, const Selector& selector);
    static std::vector<double> parseBoxValues(const std::string& value);
    static void applyBoxValues(const std::vector<double>& values, double& top, double& right,
                               double& bottom, double& left);
    static const std::string* attribute(const Node& node, const char* key);

    Style resolveStyle(const Node& node, const Style& parent) const;
    void applyDeclaration(Style& style, float& localOpacity, const std::string& property,
                          const std::string& value) const;
    LayoutNode layout(const Node& node, const Rect& parentContent, double flowY,
                      const Style& parentStyle, bool document) const;
    void recordNode(const LayoutNode& node, Render2DRecorder& recorder) const;
    std::uint64_t cacheKey(const LayoutNode& node) const;
};

HtmlCssPipeline::HtmlCssPipeline() : m_impl(std::make_unique<Impl>()) {}
HtmlCssPipeline::~HtmlCssPipeline() = default;
HtmlCssPipeline::HtmlCssPipeline(HtmlCssPipeline&&) noexcept = default;
HtmlCssPipeline& HtmlCssPipeline::operator=(HtmlCssPipeline&&) noexcept = default;

bool HtmlCssPipeline::load(std::string html, std::string css) { return m_impl->load(std::move(html), std::move(css)); }
void HtmlCssPipeline::setImageResolver(ImageResolver resolver) { m_impl->imageResolver = std::move(resolver); }
bool HtmlCssPipeline::record(Render2DRecorder& recorder, const Size& viewport) { return m_impl->record(recorder, viewport); }
const std::string& HtmlCssPipeline::lastError() const { return m_impl->error; }

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

bool HtmlCssPipeline::Impl::parseSelector(const std::string& text, Selector& selector) {
    const std::string value = trim(text);
    if (value.empty() || value.find_first_of(" >+~[:") != std::string::npos) return false;
    std::size_t position = 0;
    while (position < value.size()) {
        if (value[position] == '#') {
            const std::size_t start = ++position;
            while (position < value.size() && value[position] != '.' && value[position] != '#') ++position;
            selector.id = value.substr(start, position - start);
            selector.specificity += 100;
        } else if (value[position] == '.') {
            const std::size_t start = ++position;
            while (position < value.size() && value[position] != '.' && value[position] != '#') ++position;
            selector.classes.push_back(value.substr(start, position - start));
            selector.specificity += 10;
        } else {
            const std::size_t start = position;
            while (position < value.size() && value[position] != '.' && value[position] != '#') ++position;
            selector.tag = lower(value.substr(start, position - start));
            if (selector.tag != "*") ++selector.specificity;
        }
    }
    return !selector.tag.empty() || !selector.id.empty() || !selector.classes.empty();
}

bool HtmlCssPipeline::Impl::matches(const Node& node, const Selector& selector) {
    if (!selector.tag.empty() && selector.tag != "*" && node.tag != selector.tag) return false;
    if (!selector.id.empty()) {
        const std::string* id = attribute(node, "id");
        if (id == nullptr || *id != selector.id) return false;
    }
    const std::string* classValue = attribute(node, "class");
    for (const std::string& wanted : selector.classes) {
        if (classValue == nullptr) return false;
        std::istringstream stream(*classValue);
        std::string current;
        bool found = false;
        while (stream >> current) if (current == wanted) { found = true; break; }
        if (!found) return false;
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
    if (property == "display") style.display = lower(value) != "none";
    else if (property == "position") style.absolute = lower(value) == "absolute";
    else if (property == "width") style.width = parseLength(value);
    else if (property == "height") style.height = parseLength(value);
    else if (property == "left") style.left = parseLength(value);
    else if (property == "top") style.top = parseLength(value);
    else if (property == "opacity") localOpacity = clampOpacity(number(value, 1.0));
    else if (property == "font-size") style.fontSize = std::max(0.0, resolveLength(parseLength(value), 0.0));
    else if (property == "color" && parseColor(value, color)) style.color = color;
    else if ((property == "background" || property == "background-color") && parseColor(value, color)) style.background = color;
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

HtmlCssPipeline::Impl::Style HtmlCssPipeline::Impl::resolveStyle(const Node& node, const Style& parent) const {
    Style style;
    style.color = parent.color;
    style.fontSize = parent.fontSize;
    float localOpacity = 1.0f;
    std::vector<const Rule*> matchesRules;
    for (const Rule& rule : rules) if (matches(node, rule.selector)) matchesRules.push_back(&rule);
    std::stable_sort(matchesRules.begin(), matchesRules.end(), [](const Rule* left, const Rule* right) {
        return left->selector.specificity < right->selector.specificity;
    });
    for (const Rule* rule : matchesRules) {
        for (const auto& declaration : rule->declarations) applyDeclaration(style, localOpacity, declaration.first, declaration.second);
    }
    if (const std::string* inlineStyle = attribute(node, "style")) {
        for (const auto& declaration : parseDeclarations(*inlineStyle)) applyDeclaration(style, localOpacity, declaration.first, declaration.second);
    }
    if (node.tag == "style") style.display = false;
    style.opacity = parent.opacity * localOpacity;
    return style;
}

HtmlCssPipeline::Impl::LayoutNode HtmlCssPipeline::Impl::layout(const Node& node, const Rect& parentContent,
                                                                  double flowY, const Style& parentStyle,
                                                                  bool document) const {
    LayoutNode result;
    result.node = &node;
    result.isDocument = document;
    result.style = document ? parentStyle : resolveStyle(node, parentStyle);
    if (!result.style.display) return result;

    const Style& style = result.style;
    const double parentWidth = std::max(0.0, parentContent.size.width);
    const double parentHeight = std::max(0.0, parentContent.size.height);
    const double x = document ? parentContent.origin.x : parentContent.origin.x +
        (style.absolute ? resolveLength(style.left, parentWidth) : style.marginLeft);
    const double y = document ? parentContent.origin.y : (style.absolute ? parentContent.origin.y + resolveLength(style.top, parentHeight) : flowY + style.marginTop);
    const double width = document ? parentWidth : std::max(0.0, style.width.unit == Length::Unit::Auto
        ? parentWidth - style.marginLeft - style.marginRight : resolveLength(style.width, parentWidth));
    const double contentWidth = std::max(0.0, width - style.paddingLeft - style.paddingRight);
    const double specifiedContentHeight = style.height.unit == Length::Unit::Auto ? 0.0 :
        std::max(0.0, resolveLength(style.height, parentHeight) - style.paddingTop - style.paddingBottom);
    const double contentX = x + style.paddingLeft;
    const double contentY = y + style.paddingTop;
    double childFlowY = contentY;
    const Rect childParent = {{contentX, contentY}, {contentWidth, specifiedContentHeight}};
    for (const std::unique_ptr<Node>& child : node.children) {
        LayoutNode childLayout = layout(*child, childParent, childFlowY, style, false);
        if (childLayout.style.display && !childLayout.style.absolute) {
            childFlowY = childLayout.rect.origin.y + childLayout.rect.size.height + childLayout.style.marginBottom;
        }
        result.children.push_back(std::move(childLayout));
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
    if (!node.isDocument && node.style.background.a > 0.0f) recorder.fillRect(node.rect, node.style.background);

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

bool HtmlCssPipeline::Impl::record(Render2DRecorder& recorder, const Size& viewport) {
    error.clear();
    if (root == nullptr) { error = "No HTML document has been loaded"; return false; }
    Style rootStyle;
    LayoutNode layoutRoot = layout(*root, {{0.0, 0.0}, viewport}, 0.0, rootStyle, true);
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
    std::string css = rawCss;
    std::size_t commentStart = 0;
    while ((commentStart = css.find("/*", commentStart)) != std::string::npos) {
        const std::size_t commentEnd = css.find("*/", commentStart + 2);
        css.erase(commentStart, (commentEnd == std::string::npos ? css.size() : commentEnd + 2) - commentStart);
    }
    std::size_t position = 0;
    std::size_t ruleOrder = 0;
    while (position < css.size()) {
        const std::size_t open = css.find('{', position);
        if (open == std::string::npos) break;
        const std::size_t close = css.find('}', open + 1);
        if (close == std::string::npos) { error = "Unterminated CSS rule"; return false; }
        const std::vector<std::pair<std::string, std::string>> declarations = parseDeclarations(css.substr(open + 1, close - open - 1));
        std::size_t selectorStart = position;
        while (selectorStart < open) {
            const std::size_t comma = css.find(',', selectorStart);
            const std::string selectorText = trim(css.substr(selectorStart, (comma == std::string::npos || comma > open) ? open - selectorStart : comma - selectorStart));
            Selector selector;
            if (parseSelector(selectorText, selector)) rules.push_back({std::move(selector), declarations, ruleOrder++});
            if (comma == std::string::npos || comma > open) break;
            selectorStart = comma + 1;
        }
        position = close + 1;
    }
    return true;
}

bool HtmlCssPipeline::Impl::load(std::string html, std::string css) {
    error.clear();
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

} // namespace appgametoolbox
