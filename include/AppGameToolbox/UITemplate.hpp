#pragma once

#include "Render2D.hpp"
#include "Types.hpp"

#include <map>
#include <string>
#include <vector>

namespace appgametoolbox {

enum class ElementKind { Container, Panel, Text, Image, Button, Input, List, ScrollContainer };

using Event = int; // placeholder for event type IDs

using EventMap = std::map<std::string, Event>;

using Property = std::variant<
    bool,
    int,
    float,
    double,
    std::string,
    Color,
    Point,
    std::vector<float>,
    std::vector<int>
>;

using PropertyMap = std::map<std::string, Property>;

struct Style {
    Color color = {1.0f, 1.0f, 1.0f, 1.0f};
    Color background = {0.0f, 0.0f, 0.0f, 0.0f};
    float opacity = 1.0f;
    float fontSize = 16.0f;
    std::string fontFamily = "sans";
    int fontWeight = 400;
    bool fontItalic = false;
    int alignment = 0; // 0=left, 1=center, 2=right
    float paddingLeft = 0.0f;
    float paddingRight = 0.0f;
    float paddingTop = 0.0f;
    float paddingBottom = 0.0f;
    float marginLeft = 0.0f;
    float marginRight = 0.0f;
    float marginTop = 0.0f;
    float marginBottom = 0.0f;
    bool visible = true;
    bool clipped = false;
};

struct TemplateNode {
    ElementKind kind = ElementKind::Container;
    PropertyMap properties;
    Style style;
    EventMap events; // map event name -> event ID
    std::vector<TemplateNode> children;
    TemplateNode() = default;
    TemplateNode(ElementKind k) : kind(k) {}
};

class UITemplate {
public:
    explicit UITemplate(Size viewport = {800.0f, 600.0f});
    ~UITemplate() = default;

    // Build a tree from HTML/CSS/data; caller provides image resolver
    bool load(std::string html, std::string css, Size viewport,
              std::function<Render2DResourceId(const std::string&)> imageResolver);

    // Emit recorder commands for the entire resolved tree
    bool record(Render2DRecorder& recorder);

    // Update a node property by name path (e.g., "children.0.text")
    bool setProperty(std::string path, Property value);

    const std::string& lastError() const { return m_error; }

private:
    Size m_viewport;
    TemplateNode m_root;
    std::string m_error;
    std::uint64_t m_documentVersion = 0;

    bool parseHTML(std::string html);
    bool parseCSS(std::string css);
    void emitCommands(const TemplateNode& node, Render2DRecorder& recorder, Size parentOrigin);
    TemplateNode findNodeByPath(std::string path);
};

} // namespace appgametoolbox