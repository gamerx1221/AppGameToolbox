#pragma once

#include "HtmlCss.hpp"
#include "Effects.hpp"
#include "UIActions.hpp"
#include "UIActionPackageManifest.hpp"
#include "UIStateStore.hpp"

#include <optional>
#include <array>
#include <string>
#include <unordered_map>
#include <vector>

namespace appgametoolbox {

// Platform adapters resolve manifests and named resources from files, archives, bundles, or network caches.
class UIActionPackageSource {
public:
    virtual ~UIActionPackageSource() = default;
    virtual bool loadManifestText(const std::string& packageId, std::string& manifest,
                                  std::string& error) = 0;
    virtual bool loadResourceText(const std::string& packageId, const std::string& resource,
                                  std::string& text, std::string& error) = 0;
};

// Retained markup host. Backends supply file I/O, Render2D playback, and action handlers.
// data-action uses the restricted UIAction expression language; no script is evaluated.
class UIActionLoader {
public:
    enum class FocusDirection { Up, Down, Left, Right };

    UIActionLoader();
    ~UIActionLoader();

    bool load(std::string html, std::string css = {});
    // Opens the manifest entry route and enables declarative navigate()/back() actions.
    bool load(UIActionPackageSource& source, const std::string& packageId);
    // Sets the non-decreasing route-local animation clock. Loading a route resets it.
    void setTime(double seconds);
    void advance(double seconds);
    double routeTime() const { return m_routeTime; }
    // Reduced motion uses data-route-enter-reduced-duration when supplied, or no route enter animation.
    void setReducedMotion(bool enabled);
    bool reducedMotion() const { return m_reducedMotion; }
    // Hosts can disable particle primitives independently of motion preferences.
    void setEffectParticles(bool enabled) { m_effectParticles = enabled; }
    bool effectParticles() const { return m_effectParticles; }

    struct EffectDescriptor {
        EffectKind kind = EffectKind::PressRipple;
        std::string anchorId;
        std::size_t variant = 0;
        double clipRadius = 0.0;
        double duration = 0.0;
        bool task = false;
        std::string titleKey;
        std::string descriptionKey;
        std::string progressKey;
        std::string completionKey;
        std::string visibilityKey;
        std::string accentColor;
        std::string startCue;
        std::string progressCue;
        std::string completionCue;
    };
    enum class EffectLifecycle { Inactive, Running, Completed, Cancelled };
    struct ActiveEffect {
        EffectDescriptor descriptor;
        EffectLifecycle lifecycle = EffectLifecycle::Inactive;
        double startedAt = 0.0;
        double progress = 0.0;
    };
    const std::optional<ActiveEffect>& activeEffect() const { return m_activeEffect; }
    // Starts an effect declared on a named element without dispatching its action.
    bool triggerEffectForElement(const std::string& elementId);
    void cancelActiveEffect();
    // Samples portable renderer-independent geometry. Anchor bounds resolve after document record().
    EffectFrame sampleEffectFrame() const;
    // Cue IDs are emitted at lifecycle edges; hosts dispatch them through their PlaySound adapter.
    std::vector<std::string> takeEffectAudioCues();

    // State bindings are data-bind-text="key", data-bind-style-<property>="key",
    // and data-bind-visible="key". These compatibility helpers set string state.
    bool setTextBinding(const std::string& key, std::string value);
    bool setStyleBinding(const std::string& key, const std::string& property, std::string value);
    UIStateStore& state() { return m_state; }
    const UIStateStore& state() const { return m_state; }

    void pointerMove(Point point);
    void pointerPress(Point point);
    UIActionDispatchResult pointerRelease(Point point, const ActionDispatcher& dispatcher);
    bool focusFirst();
    bool focusNext();
    bool focusPrevious();
    bool moveFocus(FocusDirection direction);
    UIActionDispatchResult activateFocused(const ActionDispatcher& dispatcher);
    UIActionDispatchResult dispatchBack(const ActionDispatcher& dispatcher);
    UIActionDispatchResult dispatch(const UIAction& action, const ActionDispatcher& dispatcher);
    void clearInteraction();

    std::optional<UIAction> actionAt(Point point) const;
    HtmlCssPipeline& document() { return m_document; }
    const HtmlCssPipeline& document() const { return m_document; }
    const std::string& lastError() const { return m_error; }
    const std::string& route() const { return m_route; }

private:
    std::optional<UIAction> actionForNode(HtmlCssNodeId node) const;
    bool loadCurrentRoute(const std::string& routeId);
    bool compileActions();
    bool compileEffects();
    bool compileFocusGraph();
    void compileBindings();
    bool compileRouteTransition();
    bool rebuildDocument();
    void applyRouteTransition();
    void applyState(const std::string& key, const UIStateValue& value);
    UIActionDispatchResult dispatchNode(HtmlCssNodeId node, const ActionDispatcher& dispatcher);
    void startEffect(HtmlCssNodeId node);
    void cancelEffect();
    void setHover(std::optional<HtmlCssNodeId> node);
    void setPressed(std::optional<HtmlCssNodeId> node);
    void setFocus(std::optional<HtmlCssNodeId> node);

    struct FocusMetadata {
        int order = 0;
        std::string group;
        std::array<std::string, 4> targetIds;
    };

    HtmlCssPipeline m_document;
    std::optional<HtmlCssNodeId> m_hover;
    std::optional<HtmlCssNodeId> m_pressed;
    std::optional<HtmlCssNodeId> m_focus;
    // Node IDs are sequential within a retained document, so action lookup is direct.
    std::vector<std::optional<UIAction>> m_actionsByNode;
    std::unordered_map<HtmlCssNodeId, EffectDescriptor> m_effectsByNode;
    std::optional<ActiveEffect> m_activeEffect;
    std::vector<std::string> m_effectAudioCues;
    std::vector<HtmlCssNodeId> m_focusOrder;
    std::unordered_map<HtmlCssNodeId, FocusMetadata> m_focusMetadata;
    std::unordered_map<HtmlCssNodeId, std::array<std::optional<HtmlCssNodeId>, 4>> m_focusTargets;
    std::unordered_map<std::string, std::vector<HtmlCssNodeId>> m_textBindings;
    std::unordered_map<std::string, std::vector<std::pair<HtmlCssNodeId, std::string>>> m_styleBindings;
    std::unordered_map<std::string, std::vector<HtmlCssNodeId>> m_visibleBindings;
    struct RouteTransition {
        std::string duration;
        std::string reducedDuration;
        std::string easing;
        std::string stagger;
        std::vector<HtmlCssNodeId> nodes;
    };
    RouteTransition m_routeTransition;
    double m_routeTime = 0.0;
    bool m_reducedMotion = false;
    bool m_effectParticles = true;
    UIStateStore m_state;
    UIStateStore::SubscriptionId m_stateSubscription = 0;
    UIStateStore::SubscriptionId m_collectionSubscription = 0;
    std::string m_html;
    std::string m_css;
    std::vector<std::pair<std::string, UIStateValue>> m_repeatBindings;
    std::unordered_map<std::string, std::pair<std::string, std::string>> m_routeCache;
    UIActionPackageSource* m_source = nullptr;
    UIActionPackageManifest m_manifest;
    std::string m_route;
    std::vector<std::string> m_routeHistory;
    mutable std::string m_error;
};

} // namespace appgametoolbox
