#include <AppGameToolbox/UITemplate.hpp>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stack>

namespace appgametoolbox {

UITemplate::UITemplate(Size viewport) : m_viewport(viewport) {}

bool UITemplate::load(std::string html, std::string css, Size viewport,
                      std::function<Render2DResourceId(const std::string&)> imageResolver) {
    m_error.clear();
    m_viewport = viewport;
    if (!parseHTML(html)) return false;
    if (!parseCSS(css)) return false;
    // Apply CSS styles to nodes, resolve layout, etc.
    // For now just basic structure build
    m_documentVersion++;
    return true;
}

bool UITemplate::parseHTML(std::string html) {
    m_root = TemplateNode(ElementKind::Container);
    // Very simple parser: just track nested divs for now
    std::stack<TemplateNode*> nodeStack;
    nodeStack.push(&m_root);

    std::size_t pos = 0;
    while (pos < html.size()) {
        std::size_t open = html.find('<', pos);
        if (open == std::string::npos) break;

        // Check for text between tags
        if (open > pos) {
            std::string text = html.substr(pos, open - pos);
            // trim whitespace-only text
            const auto first = std::find_if_not(text.begin(), text.end(),
                [](unsigned char c) { return std::isspace(c) != 0; });
            const auto last = std::find_if_not(text.rbegin(), text.rend(),
                [](unsigned char c) { return std::isspace(c) != 0; }).base();
            if (first < last) {
                // add text to current node properties
                if (!nodeStack.empty()) {
                    nodeStack.top()->properties["storedText"] = std::string(first, last);
                } else {
                    m_root.properties["storedText"] = std::string(first, last);
                }
            }
        }

        std::size_t close = html.find('>', open + 1);
        if (close == std::string::npos) {
            m_error = "Unterminated HTML tag";
            return false;
        }

        std::string tagContent = html.substr(open + 1, close - open - 1);
        // extract just the tag name (first word before any attributes/spaces)
        const auto spacePos = tagContent.find(' ');
        std::string tagName = (spacePos != std::string::npos)
            ? tagContent.substr(0, spacePos)
            : tagContent;
        // trim
        const auto first = std::find_if_not(tagName.begin(), tagName.end(),
            [](unsigned char c) { return std::isspace(c) != 0; });
        tagName.erase(tagName.begin(), first);
        const auto last = std::find_if_not(tagName.rbegin(), tagName.rend(),
            [](unsigned char c) { return std::isspace(c) != 0; }).base();
        tagName.erase(last, tagName.end());

            // extract onclick attribute from the tag
            size_t onclickPos = tagContent.find("onclick=");
            if (onclickPos != std::string::npos) {
                size_t start = onclickPos + 8; // skip "onclick="
                size_t end = tagContent.find('"', start);
                if (end != std::string::npos) {
                    std::string onclickVal = tagContent.substr(start, end - start);
                    // store onclick in the node properties
                    if (!nodeStack.empty()) {
                        nodeStack.top()->properties["onclick"] = onclickVal;
                        nodeStack.top()->properties["eventName"] = "click";
                    } else {
                        m_root.properties["onclick"] = onclickVal;
                        m_root.properties["eventName"] = "click";
                    }
                }
            }

                if (!tagName.empty() && tagName[0] == '/') {
            // closing tag
            std::string tagName = tagContent.substr(1);
            // pop stack until we find matching opening
            if (nodeStack.size() > 1) {
                nodeStack.pop();
            }
        } else if (tagContent != "!doctype" && tagContent != "!") {
            // opening tag - create new node
            TemplateNode newNode(ElementKind::Container);

            // map tag to kind
            std::string tagLower = tagName;
            for (auto& c : tagLower) c = std::tolower(c);

            if (tagLower == "div" || tagLower == "section" || tagLower == "article") {
                newNode.kind = ElementKind::Container;
            } else if (tagLower == "p" || tagLower == "span" || tagLower == "h1" ||
                       tagLower == "h2" || tagLower == "h3" || tagLower == "h4" ||
                       tagLower == "h5" || tagLower == "h6") {
                newNode.kind = ElementKind::Text;
            } else if (tagLower == "img") {
                newNode.kind = ElementKind::Image;
            } else if (tagLower == "button") {
                newNode.kind = ElementKind::Button;
            } else if (tagLower == "ul" || tagLower == "ol") {
                newNode.kind = ElementKind::List;
            } else if (tagLower == "li") {
                newNode.kind = ElementKind::Container; // list item
            } else if (tagLower == "input") {
                newNode.kind = ElementKind::Input;
            } else {
                newNode.kind = ElementKind::Container;
            }

            // add as child of current parent
            if (!nodeStack.empty()) {
                nodeStack.top()->children.push_back(std::move(newNode));
                nodeStack.push(&nodeStack.top()->children.back());
            }
        }
        pos = close + 1;
    }
    return true;
}

bool UITemplate::parseCSS(std::string css) {
    // Very minimal CSS parsing - just extract some basic rules
    m_error.clear();
    // For now just note that CSS was seen
    // Full CSS cascade will be implemented later
    (void)css;
    return true;
}

bool UITemplate::setProperty(std::string path, Property value) {
    // Simple path-based property setter: "children.0.text" etc.
    // Split by "."
    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string part;
    while (std::getline(ss, part, '.')) {
        parts.push_back(part);
    }

    if (parts.empty()) return false;

    // Navigate to the target node
    TemplateNode* current = &m_root;
    for (std::size_t i = 0; i < parts.size() - 1; ++i) {
        const std::string& part = parts[i];
        if (part == "children" && !current->children.empty()) {
            // try to interpret as index
            int idx = 0;
            try { idx = std::stoi(part); } catch (...) { return false; }
            if (idx < 0 || (std::size_t)idx >= current->children.size()) return false;
            current = &current->children[idx];
        } else {
            m_error = "Invalid property path";
            return false;
        }
    }

    // Set the final property
    const std::string& finalPart = parts.back();
    // For simplicity, just store in the current node's properties
    current->properties[finalPart] = value;
    return true;
}

bool UITemplate::record(Render2DRecorder& recorder) {
    m_error.clear();
    // Try to record; check lastError after
    emitCommands(m_root, recorder, {0.0, 0.0});

    if (!recorder.lastError().empty()) {
        m_error = recorder.lastError();
        return false;
    }
    return true;
}



void UITemplate::emitCommands(const TemplateNode& node, Render2DRecorder& recorder, Size parentOrigin) {
    // Convert Size to Point for use in commands
    Point originPoint{parentOrigin.width, parentOrigin.height};

    switch (node.kind) {
        case ElementKind::Container: {
            // read size from properties or use default
            float w = 100.0f;
            float h = 100.0f;
            if (auto it = node.properties.find("width"); it != node.properties.end()) {
                w = std::get<float>(it->second);
            }
            if (auto it = node.properties.find("height"); it != node.properties.end()) {
                h = std::get<float>(it->second);
            }
            // default fill color
            Color c = {0.8f, 0.8f, 0.8f, 1.0f};
            if (auto it = node.properties.find("color"); it != node.properties.end()) {
                c = std::get<Color>(it->second);
            }
            // fill rect using Point origin
            recorder.fillRect({originPoint, {w, h}}, c);
            break;
        }

        case ElementKind::Text: {
            // render text
            std::string text;
            if (auto it = node.properties.find("storedText"); it != node.properties.end()) {
                text = std::get<std::string>(it->second);
            }
            float fs = 16.0f;
            if (auto it = node.properties.find("fontSize"); it != node.properties.end()) {
                fs = std::get<float>(it->second);
            }
            Color tc = {0.0f, 0.0f, 0.0f, 1.0f};
            if (auto it = node.properties.find("color"); it != node.properties.end()) {
                tc = std::get<Color>(it->second);
            }
            // default origin is parentOrigin
            recorder.drawText(0, text, originPoint, fs, tc);
            break;
        }

        case ElementKind::Image: {
            // render image if we have a resolver or ID
            Render2DResourceId imgId = 0;
            if (auto it = node.properties.find("src"); it != node.properties.end()) {
                // could be an ID or path; just use 0 for now
            }
            Size imgSize = {64.0, 64.0};
            if (auto it = node.properties.find("width"); it != node.properties.end()) {
                imgSize.width = std::get<float>(it->second);
            }
            if (auto it = node.properties.find("height"); it != node.properties.end()) {
                imgSize.height = std::get<float>(it->second);
            }
            recorder.drawImage(imgId, {originPoint, imgSize});
            break;
        }

        case ElementKind::Button: {
            // render as a colored rect
            float bw = 80.0f;
            float bh = 30.0f;
            Color bc = {0.2f, 0.6f, 0.9f, 1.0f};
            if (auto it = node.properties.find("width"); it != node.properties.end()) {
                bw = std::get<float>(it->second);
            }
            if (auto it = node.properties.find("height"); it != node.properties.end()) {
                bh = std::get<float>(it->second);
            }
            if (auto it = node.properties.find("background"); it != node.properties.end()) {
                bc = std::get<Color>(it->second);
            }
            recorder.fillRect({originPoint, {bw, bh}}, bc);
            // also render any text stored in properties
            if (auto it = node.properties.find("storedText"); it != node.properties.end()) {
                std::string text = std::get<std::string>(it->second);
                float fs = 12.0f;
                if (auto fit = node.properties.find("fontSize"); fit != node.properties.end()) {
                    fs = std::get<float>(fit->second);
                }
                Color tc = {0.0f, 0.0f, 0.0f, 1.0f};
                if (auto cit = node.properties.find("color"); cit != node.properties.end()) {
                    tc = std::get<Color>(cit->second);
                }
                recorder.drawText(0, text, originPoint, fs, tc);
            }
            // also render any text child nodes
            for (const auto& child : node.children) {
                emitCommands(child, recorder, parentOrigin);
            }
            break;
        }

        case ElementKind::Input: {
            // simple input representation
            float iw = 120.0f;
            float ih = 25.0f;
            Color ic = {1.0f, 1.0f, 1.0f, 1.0f};
            if (auto it = node.properties.find("width"); it != node.properties.end()) {
                iw = std::get<float>(it->second);
            }
            if (auto it = node.properties.find("height"); it != node.properties.end()) {
                ih = std::get<float>(it->second);
            }
            if (auto it = node.properties.find("background"); it != node.properties.end()) {
                ic = std::get<Color>(it->second);
            }
            recorder.fillRect({originPoint, {iw, ih}}, ic);
            break;
        }

        case ElementKind::List: {
            // render as vertical list of children
            for (const auto& child : node.children) {
                emitCommands(child, recorder, parentOrigin);
                // add simple spacing by shifting origin y
                originPoint.y += 20.0;
            }
            break;
        }

        case ElementKind::ScrollContainer: {
            // render as container with children
            for (const auto& child : node.children) {
                emitCommands(child, recorder, parentOrigin);
            }
            break;
        }

        default:
            // fall back to container behavior
            break;
    }

    // emit children after the node's own command
    for (const auto& child : node.children) {
        emitCommands(child, recorder, parentOrigin);
    }
}


void UITemplate::setEventCallback(EventCallbackC cb) {
    m_eventCallbackC = cb;
    m_eventCallbackStd = nullptr;
}

void UITemplate::setEventCallback(EventCallbackStd stdCb) {
    m_eventCallbackStd = stdCb;
    m_eventCallbackC = nullptr;
}

void UITemplate::fireEvent(const char* name, int eventID) {
    if (m_eventCallbackC) {
        m_eventCallbackC(name, eventID);
    }
}

void UITemplate::fireEventStd(const std::string& name, int eventID) {
    if (m_eventCallbackStd) {
        m_eventCallbackStd(name, eventID);
    }
}
} // namespace appgametoolbox