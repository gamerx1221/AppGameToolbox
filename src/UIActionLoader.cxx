#include <AppGameToolbox/UIActionLoader.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace appgametoolbox {
namespace {

std::optional<double> milliseconds(const std::string& value) {
    try {
        std::size_t consumed = 0;
        double result = 0.0;
        if (value.size() > 2 && value.substr(value.size() - 2) == "ms")
            result = std::stod(value.substr(0, value.size() - 2), &consumed), consumed += 2;
        else if (!value.empty() && value.back() == 's')
            result = std::stod(value.substr(0, value.size() - 1), &consumed), consumed += 1, result *= 1000.0;
        else return std::nullopt;
        return consumed == value.size() && std::isfinite(result) && result >= 0.0 ? std::optional<double>(result) : std::nullopt;
    } catch (const std::exception&) { return std::nullopt; }
}

std::optional<EffectKind> effectKind(const std::string& value) {
    static const std::pair<const char*, EffectKind> kinds[] = {{"ambient-drift", EffectKind::AmbientDrift}, {"crystal-burst", EffectKind::CrystalBurst}, {"ember-trail", EffectKind::EmberTrail}, {"navigation-medallion", EffectKind::NavigationMedallion}, {"press-ripple", EffectKind::PressRipple}, {"sonar-pulse", EffectKind::SonarPulse}, {"sparkle-orbit", EffectKind::SparkleOrbit}, {"signal-wave", EffectKind::SignalWave}};
    for (const auto& kind : kinds) if (value == kind.first) return kind.second;
    return std::nullopt;
}

bool boolean(const std::string& value) { return value == "true"; }

std::string replaceAll(std::string text, const std::string& from, const std::string& to) {
    for (std::size_t position = 0; (position = text.find(from, position)) != std::string::npos; position += to.size())
        text.replace(position, from.size(), to);
    return text;
}

std::string tagName(const std::string& tag) {
    std::size_t begin = tag.find_first_not_of(" \t\r\n", 1);
    if (begin != std::string::npos && tag[begin] == '/') begin = tag.find_first_not_of(" \t\r\n", begin + 1);
    if (begin == std::string::npos) return {};
    const std::size_t end = tag.find_first_of(" \t\r\n>/", begin);
    return tag.substr(begin, end - begin);
}

std::optional<std::size_t> matchingClose(const std::string& html, std::size_t content, const std::string& name) {
    int depth = 1;
    for (std::size_t position = content; (position = html.find('<', position)) != std::string::npos;) {
        const std::size_t end = html.find('>', position);
        if (end == std::string::npos) return std::nullopt;
        const std::string tag = html.substr(position, end - position + 1);
        const std::string current = tagName(tag);
        if (current == name && tag.size() > 1 && tag[1] == '/') --depth;
        else if (current == name && tag.size() > 1 && tag[1] != '/' && tag.size() > 2 && tag[tag.size() - 2] != '/') ++depth;
        if (depth == 0) return position;
        position = end + 1;
    }
    return std::nullopt;
}

} // namespace

UIActionLoader::UIActionLoader() {
    m_stateSubscription = m_state.subscribe([this](const std::string& key, const UIStateValue& value) { applyState(key, value); });
    m_collectionSubscription = m_state.subscribeCollections([this](const std::string&, const UIStateCollection&) {
        if (m_html.empty()) return;
        cancelEffect();
        clearInteraction();
        if (rebuildDocument()) focusFirst();
    });
}

UIActionLoader::~UIActionLoader() {
    m_state.unsubscribe(m_stateSubscription);
    m_state.unsubscribe(m_collectionSubscription);
}

bool UIActionLoader::load(std::string html, std::string css) {
    cancelEffect();
    clearInteraction();
    m_error.clear();
    m_source = nullptr;
    m_route.clear();
    m_routeHistory.clear();
    m_routeCache.clear();
    m_manifest = {};
    m_html = std::move(html);
    m_css = std::move(css);
    m_routeTime = 0.0;
    if (!rebuildDocument()) return false;
    focusFirst();
    return true;
}

bool UIActionLoader::load(UIActionPackageSource& source, const std::string& packageId) {
    m_source = &source;
    m_route.clear();
    m_routeHistory.clear();
    m_routeCache.clear();
    m_manifest = {};
    std::string manifestText;
    if (!source.loadManifestText(packageId, manifestText, m_error) ||
        !parseUIActionPackageManifest(manifestText, m_manifest, m_error)) return false;
    if (m_manifest.packageId != packageId) { m_error = "manifest package id does not match requested package"; return false; }
    for (const auto& route : m_manifest.routes)
        if (route.preload && !loadCurrentRoute(route.id)) return false;
    return loadCurrentRoute(m_manifest.entryRoute);
}

bool UIActionLoader::loadCurrentRoute(const std::string& routeId) {
    const auto declaration = std::find_if(m_manifest.routes.begin(), m_manifest.routes.end(),
        [&routeId](const UIActionPackageRoute& route) { return route.id == routeId; });
    if (declaration == m_manifest.routes.end()) { m_error = "route is not declared by package manifest"; return false; }
    std::pair<std::string, std::string> package;
    const auto cached = m_routeCache.find(routeId);
    if (cached != m_routeCache.end()) {
        package = cached->second;
    } else {
        std::string error;
        if (m_source == nullptr || !m_source->loadResourceText(m_manifest.packageId, declaration->html, package.first, error) ||
            !m_source->loadResourceText(m_manifest.packageId, declaration->css, package.second, error)) {
            m_error = std::move(error);
            return false;
        }
        m_routeCache.emplace(routeId, package);
    }
    cancelEffect();
    clearInteraction();
    m_error.clear();
    m_html = std::move(package.first);
    m_css = std::move(package.second);
    m_routeTime = 0.0;
    if (!rebuildDocument()) return false;
    focusFirst();
    m_route = routeId;
    m_state.set("route", routeId);
    return true;
}

bool UIActionLoader::rebuildDocument() {
    std::string expanded = m_html;
    m_repeatBindings.clear();
    for (std::size_t start = 0; (start = expanded.find("data-repeat=", start)) != std::string::npos;) {
        const std::size_t open = expanded.rfind('<', start);
        const std::size_t openEnd = expanded.find('>', start);
        if (open == std::string::npos || openEnd == std::string::npos) { m_error = "invalid data-repeat template"; return false; }
        const std::string opening = expanded.substr(open, openEnd - open + 1);
        const std::string name = tagName(opening);
        const auto close = matchingClose(expanded, openEnd + 1, name);
        if (name.empty() || !close) { m_error = "data-repeat template requires a closing element"; return false; }
        const std::size_t closeEnd = expanded.find('>', *close);
        const auto attribute = [&opening](const char* key) -> std::optional<std::string> {
            const std::string needle = std::string(key) + "=";
            const std::size_t at = opening.find(needle);
            if (at == std::string::npos || at + needle.size() >= opening.size()) return std::nullopt;
            const char quote = opening[at + needle.size()];
            const std::size_t value = at + needle.size() + 1;
            const std::size_t end = opening.find(quote, value);
            return end == std::string::npos ? std::nullopt : std::optional<std::string>(opening.substr(value, end - value));
        };
        const auto collectionName = attribute("data-repeat");
        const auto itemName = attribute("data-repeat-item");
        if (!collectionName || !itemName || itemName->empty()) { m_error = "data-repeat requires data-repeat-item"; return false; }
        const UIStateCollection* collection = m_state.collection(*collectionName);
        std::string replacement;
        if (collection) for (const UIStateItem& item : *collection) {
            const auto id = item.find("id");
            if (id == item.end()) { m_error = "repeated items require an id"; return false; }
            std::string instance = expanded.substr(open, closeEnd - open + 1);
            instance = replaceAll(instance, "data-repeat=\"" + *collectionName + "\"", "");
            instance = replaceAll(instance, "data-repeat-item=\"" + *itemName + "\"", "");
            const std::string stableId = UIStateStore::toString(id->second);
            const std::string scope = "__repeat." + *collectionName + "." + stableId + ".";
            for (const auto& field : item) {
                const std::string value = UIStateStore::toString(field.second);
                instance = replaceAll(instance, "{{" + *itemName + "." + field.first + "}}", value);
                const std::string scoped = *itemName + "." + field.first;
                instance = replaceAll(instance, "data-bind-text=\"" + scoped + "\"", "data-bind-text=\"" + scope + field.first + "\"");
                instance = replaceAll(instance, "data-bind-visible=\"" + scoped + "\"", "data-bind-visible=\"" + scope + field.first + "\"");
                for (std::size_t binding = instance.find("data-bind-style-"); binding != std::string::npos;) {
                    const std::size_t equals = instance.find('=', binding);
                    if (equals == std::string::npos || equals + 2 >= instance.size()) break;
                    const char quote = instance[equals + 1];
                    const std::size_t value = equals + 2;
                    const std::size_t end = instance.find(quote, value);
                    if (end == std::string::npos) break;
                    if (instance.substr(value, end - value) == scoped) {
                        instance.replace(value, end - value, scope + field.first);
                        binding = value + scope.size() + field.first.size();
                    } else binding = end + 1;
                }
                m_repeatBindings.push_back({scope + field.first, field.second});
            }
            // Missing optional fields intentionally expand to an empty attribute value.
            for (std::size_t unresolved = instance.find("{{" + *itemName + "."); unresolved != std::string::npos;) {
                const std::size_t end = instance.find("}}", unresolved);
                if (end == std::string::npos) break;
                instance.erase(unresolved, end + 2 - unresolved);
                unresolved = instance.find("{{" + *itemName + ".");
            }
            if (instance.find(" id=") == std::string::npos) instance.insert(instance.find('>'), " id=\"" + *collectionName + "-" + stableId + "\"");
            replacement += instance;
        }
        expanded.replace(open, closeEnd - open + 1, replacement);
        start = open + replacement.size();
    }
    if (!m_document.load(std::move(expanded), m_css)) { m_error = m_document.lastError(); return false; }
    m_document.setTime(m_routeTime);
    if (!compileActions()) return false;
    for (const auto& binding : m_repeatBindings) applyState(binding.first, binding.second);
    return true;
}

bool UIActionLoader::compileActions() {
    m_actionsByNode.clear();
    m_focusOrder.clear();
    m_focusMetadata.clear();
    m_focusTargets.clear();
    m_effectsByNode.clear();
    m_activeEffect.reset();
    m_effectAudioCues.clear();
    for (const HtmlCssNodeId node : m_document.nodesWithDataAttributeKey("data-action")) {
        const auto expression = m_document.dataAttribute(node, "data-action");
        if (!expression || expression->empty()) continue;
        const UIActionParseResult parsed = parseUIActionExpression(*expression);
        if (!parsed) {
            m_error = "invalid data-action: " + parsed.error;
            m_actionsByNode.clear();
            return false;
        }
        if (node >= m_actionsByNode.size()) m_actionsByNode.resize(static_cast<std::size_t>(node) + 1);
        m_actionsByNode[static_cast<std::size_t>(node)] = parsed.action;
    }
    if (!compileFocusGraph()) return false;
    if (!compileEffects()) return false;
    compileBindings();
    return compileRouteTransition();
}

bool UIActionLoader::compileEffects() {
    for (const HtmlCssNodeId node : m_document.nodesWithDataAttributeKey("data-effect-kind")) {
        if (!actionForNode(node)) { m_error = "data-effect-kind requires data-action"; return false; }
        const auto kindText = m_document.dataAttribute(node, "data-effect-kind");
        if (!kindText || kindText->empty()) continue;
        const auto kind = kindText ? effectKind(*kindText) : std::nullopt;
        if (!kind) { m_error = "unknown data-effect-kind"; return false; }
        EffectDescriptor descriptor;
        descriptor.kind = *kind;
        const auto value = [this, node](const char* key) { return m_document.dataAttribute(node, key).value_or(""); };
        descriptor.anchorId = value("data-effect-anchor");
        const std::string variant = value("data-effect-variant");
        if (!variant.empty()) {
            try {
                std::size_t consumed = 0;
                descriptor.variant = std::stoull(variant, &consumed);
                if (consumed != variant.size()) throw std::invalid_argument("effect variant");
            } catch (const std::exception&) { m_error = "data-effect-variant must be an unsigned integer"; return false; }
        }
        const std::string clipRadius = value("data-effect-clip-radius");
        if (!clipRadius.empty()) {
            try {
                std::size_t consumed = 0;
                descriptor.clipRadius = std::stod(clipRadius, &consumed);
                if (consumed != clipRadius.size() || !std::isfinite(descriptor.clipRadius) || descriptor.clipRadius < 0.0)
                    throw std::invalid_argument("effect clip radius");
            } catch (const std::exception&) { m_error = "data-effect-clip-radius must be a non-negative number"; return false; }
        }
        descriptor.task = boolean(value("data-effect-task"));
        descriptor.titleKey = value("data-effect-title-key");
        descriptor.descriptionKey = value("data-effect-description-key");
        descriptor.progressKey = value("data-effect-progress-key");
        descriptor.completionKey = value("data-effect-completion-key");
        descriptor.visibilityKey = value("data-effect-visibility-key");
        descriptor.accentColor = value("data-effect-accent");
        descriptor.startCue = value("data-effect-start-sound");
        descriptor.progressCue = value("data-effect-progress-sound");
        descriptor.completionCue = value("data-effect-completion-sound");
        const std::string duration = value("data-effect-duration");
        if (!duration.empty()) {
            const auto parsed = milliseconds(duration);
            if (!parsed) { m_error = "data-effect-duration must use ms or s"; return false; }
            descriptor.duration = *parsed / 1000.0;
        }
        if (descriptor.task && (descriptor.progressKey.empty() || descriptor.completionKey.empty())) {
            m_error = "task effects require data-effect-progress-key and data-effect-completion-key"; return false;
        }
        m_effectsByNode.emplace(node, std::move(descriptor));
    }
    return true;
}

bool UIActionLoader::compileFocusGraph() {
    for (const HtmlCssNodeId node : m_document.nodesWithDataAttributeKey("data-action")) {
        const auto orderText = m_document.dataAttribute(node, "data-focus-order");
        if (!orderText) continue;
        try {
            std::size_t consumed = 0;
            const long long parsed = std::stoll(*orderText, &consumed);
            if (consumed != orderText->size() || parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max())
                throw std::out_of_range("focus order");
            FocusMetadata metadata;
            metadata.order = static_cast<int>(parsed);
            if (const auto group = m_document.dataAttribute(node, "data-focus-group")) metadata.group = *group;
            constexpr std::array<const char*, 4> targetAttributes = {
                "data-nav-up", "data-nav-down", "data-nav-left", "data-nav-right"};
            for (std::size_t direction = 0; direction < targetAttributes.size(); ++direction)
                if (const auto target = m_document.dataAttribute(node, targetAttributes[direction])) metadata.targetIds[direction] = *target;
            m_focusMetadata.emplace(node, std::move(metadata));
            m_focusOrder.push_back(node);
        } catch (const std::exception&) {
            m_error = "data-focus-order must be an integer";
            return false;
        }
    }
    std::stable_sort(m_focusOrder.begin(), m_focusOrder.end(), [this](HtmlCssNodeId left, HtmlCssNodeId right) {
        return m_focusMetadata.at(left).order < m_focusMetadata.at(right).order;
    });
    for (const HtmlCssNodeId node : m_focusOrder) {
        std::array<std::optional<HtmlCssNodeId>, 4> targets;
        const auto& metadata = m_focusMetadata.at(node);
        for (std::size_t direction = 0; direction < metadata.targetIds.size(); ++direction) {
            if (metadata.targetIds[direction].empty()) continue;
            const auto target = m_document.nodeIdForElementId(metadata.targetIds[direction]);
            if (!target || m_focusMetadata.find(*target) == m_focusMetadata.end()) {
                m_error = "navigation target must identify a focusable data-action node";
                return false;
            }
            targets[direction] = *target;
        }
        m_focusTargets.emplace(node, std::move(targets));
    }
    return true;
}

void UIActionLoader::compileBindings() {
    m_textBindings.clear();
    m_styleBindings.clear();
    m_visibleBindings.clear();
    for (const HtmlCssNodeId node : m_document.nodesWithDataAttributeKey("data-bind-text")) {
        const auto binding = m_document.dataAttribute(node, "data-bind-text");
        if (binding && !binding->empty()) m_textBindings[*binding].push_back(node);
    }
    for (const HtmlCssDataAttribute& binding : m_document.dataAttributesWithPrefix("data-bind-style-")) {
        const std::string property = binding.key.substr(std::string("data-bind-style-").size());
        if (!property.empty() && !binding.value.empty()) m_styleBindings[binding.value].push_back({binding.node, property});
    }
    for (const HtmlCssNodeId node : m_document.nodesWithDataAttributeKey("data-bind-visible")) {
        const auto binding = m_document.dataAttribute(node, "data-bind-visible");
        if (binding && !binding->empty()) m_visibleBindings[*binding].push_back(node);
    }
    for (const auto& state : m_state.values()) applyState(state.first, state.second);
}

void UIActionLoader::setTime(double seconds) {
    if (!std::isfinite(seconds)) return;
    m_routeTime = std::max(m_routeTime, std::max(0.0, seconds));
    m_document.setTime(m_routeTime);
    if (m_activeEffect && m_activeEffect->lifecycle == EffectLifecycle::Running && !m_activeEffect->descriptor.task &&
        m_activeEffect->descriptor.duration > 0.0 && m_routeTime - m_activeEffect->startedAt >= m_activeEffect->descriptor.duration)
        m_activeEffect->lifecycle = EffectLifecycle::Completed;
}

void UIActionLoader::advance(double seconds) {
    if (!std::isfinite(seconds) || seconds <= 0.0) return;
    setTime(m_routeTime + seconds);
}

void UIActionLoader::setReducedMotion(bool enabled) {
    if (m_reducedMotion == enabled) return;
    m_reducedMotion = enabled;
    applyRouteTransition();
}

bool UIActionLoader::compileRouteTransition() {
    m_routeTransition = {};
    auto roots = m_document.nodesWithDataAttributeKey("data-route-enter-duration");
    if (roots.empty()) return true;
    std::sort(roots.begin(), roots.end());
    const HtmlCssNodeId root = roots.front();
    const auto duration = m_document.dataAttribute(root, "data-route-enter-duration");
    const auto easing = m_document.dataAttribute(root, "data-route-enter-easing");
    const auto stagger = m_document.dataAttribute(root, "data-route-enter-stagger");
    if (!duration || !easing || !stagger || duration->empty() || easing->empty() || stagger->empty()) {
        m_error = "route enter metadata requires duration, easing, and stagger";
        return false;
    }
    if (!milliseconds(*duration) || !milliseconds(*stagger)) {
        m_error = "route enter duration and stagger must use ms or s";
        return false;
    }
    m_routeTransition.duration = *duration;
    m_routeTransition.easing = *easing;
    m_routeTransition.stagger = *stagger;
    if (const auto reduced = m_document.dataAttribute(root, "data-route-enter-reduced-duration")) {
        if (!milliseconds(*reduced)) { m_error = "route enter reduced duration must use ms or s"; return false; }
        m_routeTransition.reducedDuration = *reduced;
    }
    m_routeTransition.nodes = m_document.nodesWithDataAttributeKey("data-route-enter");
    std::sort(m_routeTransition.nodes.begin(), m_routeTransition.nodes.end());
    applyRouteTransition();
    return true;
}

void UIActionLoader::applyRouteTransition() {
    const double stagger = m_routeTransition.stagger.empty() ? 0.0 : *milliseconds(m_routeTransition.stagger);
    for (std::size_t index = 0; index < m_routeTransition.nodes.size(); ++index) {
        const HtmlCssNodeId node = m_routeTransition.nodes[index];
        if (m_reducedMotion && m_routeTransition.reducedDuration.empty()) {
            m_document.setStyleProperty(node, "animation", "none");
            continue;
        }
        const std::string& duration = m_reducedMotion ? m_routeTransition.reducedDuration : m_routeTransition.duration;
        const double delay = m_reducedMotion ? 0.0 : stagger * static_cast<double>(index);
        m_document.setStyleProperty(node, "animation", "route-enter " + duration + " " + m_routeTransition.easing + " " +
            std::to_string(delay) + "ms");
    }
}

bool UIActionLoader::setTextBinding(const std::string& key, std::string value) {
    return m_state.set(key, std::move(value));
}

bool UIActionLoader::setStyleBinding(const std::string& key, const std::string& property, std::string value) {
    bool changed = false;
    const auto bindings = m_styleBindings.find(key);
    if (bindings == m_styleBindings.end()) return false;
    for (const auto& binding : bindings->second)
        if (binding.second == property) changed = m_document.setStyleProperty(binding.first, property, value) || changed;
    return changed;
}

void UIActionLoader::applyState(const std::string& key, const UIStateValue& value) {
    const std::string text = UIStateStore::toString(value);
    const auto textBinding = m_textBindings.find(key);
    if (textBinding != m_textBindings.end())
        for (const HtmlCssNodeId node : textBinding->second) m_document.setText(node, text);
    const auto styleBinding = m_styleBindings.find(key);
    if (styleBinding != m_styleBindings.end())
        for (const auto& binding : styleBinding->second) m_document.setStyleProperty(binding.first, binding.second, text);
    const auto visibleBinding = m_visibleBindings.find(key);
    if (visibleBinding != m_visibleBindings.end()) {
        const bool visible = std::get_if<bool>(&value) != nullptr && std::get<bool>(value);
        for (const HtmlCssNodeId node : visibleBinding->second)
            m_document.setStyleProperty(node, "display", visible ? "block" : "none");
    }
    if (m_activeEffect && m_activeEffect->lifecycle == EffectLifecycle::Running) {
        auto& active = *m_activeEffect;
        if (key == active.descriptor.progressKey) {
            double progress = 0.0;
            if (const auto* number = std::get_if<double>(&value)) progress = *number;
            else if (const auto* integer = std::get_if<std::int64_t>(&value)) progress = static_cast<double>(*integer);
            progress = std::clamp(progress, 0.0, 1.0);
            if (progress != active.progress) { active.progress = progress; if (!active.descriptor.progressCue.empty()) m_effectAudioCues.push_back(active.descriptor.progressCue); }
        }
        if (key == active.descriptor.completionKey && std::get_if<bool>(&value) != nullptr && std::get<bool>(value)) {
            active.lifecycle = EffectLifecycle::Completed;
            if (!active.descriptor.completionCue.empty()) m_effectAudioCues.push_back(active.descriptor.completionCue);
        }
    }
}

void UIActionLoader::startEffect(HtmlCssNodeId node) {
    const auto found = m_effectsByNode.find(node);
    if (found == m_effectsByNode.end()) return;
    if (m_activeEffect && m_activeEffect->lifecycle == EffectLifecycle::Running &&
        !m_activeEffect->descriptor.visibilityKey.empty())
        m_state.set(m_activeEffect->descriptor.visibilityKey, false);
    m_activeEffect = ActiveEffect{found->second, EffectLifecycle::Running, m_routeTime, 0.0};
    if (!found->second.visibilityKey.empty()) m_state.set(found->second.visibilityKey, true);
    if (const auto progress = m_state.value(found->second.progressKey)) {
        if (const auto* number = std::get_if<double>(&*progress)) m_activeEffect->progress = std::clamp(*number, 0.0, 1.0);
        else if (const auto* integer = std::get_if<std::int64_t>(&*progress)) m_activeEffect->progress = std::clamp(static_cast<double>(*integer), 0.0, 1.0);
    }
    if (!found->second.startCue.empty()) m_effectAudioCues.push_back(found->second.startCue);
}

void UIActionLoader::cancelEffect() {
    if (!m_activeEffect || m_activeEffect->lifecycle != EffectLifecycle::Running) return;
    m_activeEffect->lifecycle = EffectLifecycle::Cancelled;
    if (!m_activeEffect->descriptor.visibilityKey.empty()) m_state.set(m_activeEffect->descriptor.visibilityKey, false);
}

bool UIActionLoader::triggerEffectForElement(const std::string& elementId) {
    const auto node = m_document.nodeIdForElementId(elementId);
    if (!node || m_effectsByNode.find(*node) == m_effectsByNode.end()) return false;
    startEffect(*node);
    return true;
}

void UIActionLoader::cancelActiveEffect() { cancelEffect(); }

EffectFrame UIActionLoader::sampleEffectFrame() const {
    if (!m_activeEffect || m_activeEffect->lifecycle != EffectLifecycle::Running) return {};
    const auto& active = *m_activeEffect;
    Rect bounds = {{0.0, 0.0}, {1.0, 1.0}};
    if (!active.descriptor.anchorId.empty()) {
        const auto node = m_document.nodeIdForElementId(active.descriptor.anchorId);
        const auto resolved = node ? m_document.bounds(*node) : std::nullopt;
        if (!resolved) return {};
        bounds = *resolved;
    }
    EffectSpec spec = EffectGenerator::preset(active.descriptor.kind, active.descriptor.variant);
    // Task lifetime is controlled by game state, not the visual duration.
    // Keeping this duration lets task effects animate within their panel.
    spec.duration = active.descriptor.duration;
    spec.motion = !m_reducedMotion;
    spec.particles = m_effectParticles;
    EffectFrame frame = EffectGenerator().sample({spec, active.startedAt}, bounds, m_routeTime);
    frame.clip = bounds;
    frame.clipRadius = active.descriptor.clipRadius;
    return frame;
}

std::vector<std::string> UIActionLoader::takeEffectAudioCues() { return std::exchange(m_effectAudioCues, {}); }

std::optional<UIAction> UIActionLoader::actionForNode(HtmlCssNodeId node) const {
    if (node >= m_actionsByNode.size()) return std::nullopt;
    return m_actionsByNode[static_cast<std::size_t>(node)];
}

std::optional<UIAction> UIActionLoader::actionAt(Point point) const {
    const auto node = m_document.attributeNodeAt(point, "data-action");
    return node ? actionForNode(*node) : std::nullopt;
}

void UIActionLoader::setHover(std::optional<HtmlCssNodeId> node) {
    if (m_hover == node) return;
    if (m_hover) m_document.setPseudoState(*m_hover, CssPseudoState::Hover, false);
    m_hover = node;
    if (m_hover) m_document.setPseudoState(*m_hover, CssPseudoState::Hover, true);
}

void UIActionLoader::setPressed(std::optional<HtmlCssNodeId> node) {
    if (m_pressed == node) return;
    if (m_pressed) m_document.setPseudoState(*m_pressed, CssPseudoState::Active, false);
    m_pressed = node;
    if (m_pressed) m_document.setPseudoState(*m_pressed, CssPseudoState::Active, true);
}

void UIActionLoader::setFocus(std::optional<HtmlCssNodeId> node) {
    if (m_focus == node) return;
    if (m_focus) m_document.setPseudoState(*m_focus, CssPseudoState::Focus, false);
    m_focus = node;
    if (m_focus) m_document.setPseudoState(*m_focus, CssPseudoState::Focus, true);
}

void UIActionLoader::pointerMove(Point point) {
    const auto node = m_document.attributeNodeAt(point, "data-action");
    setHover(node);
    if (node) setFocus(node);
}

void UIActionLoader::pointerPress(Point point) {
    pointerMove(point);
    setPressed(m_hover);
}

UIActionDispatchResult UIActionLoader::pointerRelease(Point point, const ActionDispatcher& dispatcher) {
    pointerMove(point);
    const auto pressed = m_pressed;
    setPressed(std::nullopt);
    if (!pressed || pressed != m_hover) return {UIActionDispatchStatus::NotFound, "pointer release did not target an action", 0};
    return dispatchNode(*pressed, dispatcher);
}

bool UIActionLoader::focusFirst() {
    if (m_focusOrder.empty()) return false;
    setFocus(m_focusOrder.front());
    return true;
}

bool UIActionLoader::focusNext() {
    if (m_focusOrder.empty()) return false;
    if (!m_focus || m_focusMetadata.find(*m_focus) == m_focusMetadata.end()) return focusFirst();
    const std::string& group = m_focusMetadata.at(*m_focus).group;
    const auto current = std::find(m_focusOrder.begin(), m_focusOrder.end(), *m_focus);
    for (auto node = std::next(current); node != m_focusOrder.end(); ++node)
        if (m_focusMetadata.at(*node).group == group) { setFocus(*node); return true; }
    for (const HtmlCssNodeId node : m_focusOrder)
        if (m_focusMetadata.at(node).group == group) { setFocus(node); return true; }
    return false;
}

bool UIActionLoader::focusPrevious() {
    if (m_focusOrder.empty()) return false;
    if (!m_focus || m_focusMetadata.find(*m_focus) == m_focusMetadata.end()) {
        setFocus(m_focusOrder.back());
        return true;
    }
    const std::string& group = m_focusMetadata.at(*m_focus).group;
    const auto current = std::find(m_focusOrder.begin(), m_focusOrder.end(), *m_focus);
    for (auto node = current; node != m_focusOrder.begin();) {
        --node;
        if (m_focusMetadata.at(*node).group == group) { setFocus(*node); return true; }
    }
    for (auto node = m_focusOrder.rbegin(); node != m_focusOrder.rend(); ++node)
        if (m_focusMetadata.at(*node).group == group) { setFocus(*node); return true; }
    return false;
}

bool UIActionLoader::moveFocus(FocusDirection direction) {
    const std::size_t index = static_cast<std::size_t>(direction);
    if (m_focus) {
        const auto targets = m_focusTargets.find(*m_focus);
        if (targets != m_focusTargets.end() && targets->second[index]) {
            setFocus(*targets->second[index]);
            return true;
        }
    }
    return direction == FocusDirection::Up || direction == FocusDirection::Left ? focusPrevious() : focusNext();
}

UIActionDispatchResult UIActionLoader::activateFocused(const ActionDispatcher& dispatcher) {
    return m_focus ? dispatchNode(*m_focus, dispatcher) : UIActionDispatchResult{UIActionDispatchStatus::NotFound, "no focused action", 0};
}

UIActionDispatchResult UIActionLoader::dispatchNode(HtmlCssNodeId node, const ActionDispatcher& dispatcher) {
    const auto action = actionForNode(node);
    if (!action) return {UIActionDispatchStatus::InvalidAction, m_error, 0};
    startEffect(node);
    return dispatch(*action, dispatcher);
}

UIActionDispatchResult UIActionLoader::dispatchBack(const ActionDispatcher& dispatcher) {
    return dispatch(UIAction::make(UIActionKind::Back), dispatcher);
}

UIActionDispatchResult UIActionLoader::dispatch(const UIAction& action, const ActionDispatcher& dispatcher) {
    if (action.kind == UIActionKind::Sequence) {
        UIActionDispatchResult total;
        for (const UIAction& child : action.actions) {
            const UIActionDispatchResult result = dispatch(child, dispatcher);
            total.dispatchedCount += result.dispatchedCount;
            if (!result) return result;
        }
        return total;
    }
    if (m_source != nullptr && action.kind == UIActionKind::Navigate) {
        const auto route = action.arguments.find("route");
        if (route == action.arguments.end() || !std::holds_alternative<std::string>(route->second))
            return {UIActionDispatchStatus::InvalidAction, "navigate action is missing its route", 0};
        const std::string& nextRoute = std::get<std::string>(route->second);
        const std::string previousRoute = m_route;
        if (!loadCurrentRoute(nextRoute)) return {UIActionDispatchStatus::HandlerFailed, m_error, 0};
        if (!previousRoute.empty() && previousRoute != nextRoute) m_routeHistory.push_back(previousRoute);
        return {UIActionDispatchStatus::Success, {}, 1};
    }
    if (m_source != nullptr && action.kind == UIActionKind::Back && !m_routeHistory.empty()) {
        const std::string previousRoute = m_routeHistory.back();
        m_routeHistory.pop_back();
        if (!loadCurrentRoute(previousRoute)) return {UIActionDispatchStatus::HandlerFailed, m_error, 0};
        return {UIActionDispatchStatus::Success, {}, 1};
    }
    return dispatcher.dispatch(action);
}

void UIActionLoader::clearInteraction() {
    setHover(std::nullopt);
    setPressed(std::nullopt);
    setFocus(std::nullopt);
}

} // namespace appgametoolbox
