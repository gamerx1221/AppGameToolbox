#include <AppGameToolbox/UIElement2D.hpp>

#include <algorithm>
#include <utility>

namespace appgametoolbox {

UIElement2D::UIElement2D(std::string name) : m_name(std::move(name)) {}
void UIElement2D::update(double dt) { (void)dt; }
const std::string& UIElement2D::name() const { return m_name; }
void UIElement2D::setName(std::string name) { m_name = std::move(name); }
const Rect& UIElement2D::frame() const { return m_frame; }
void UIElement2D::setFrame(const Rect& frame) { m_frame = frame; }
bool UIElement2D::isVisible() const { return m_visible; }
void UIElement2D::setVisible(bool visible) { m_visible = visible; }
bool UIElement2D::isEnabled() const { return m_enabled; }
void UIElement2D::setEnabled(bool enabled) { m_enabled = enabled; }
float UIElement2D::opacity() const { return m_opacity; }
void UIElement2D::setOpacity(float opacity) { m_opacity = std::clamp(opacity, 0.0f, 1.0f); }
int UIElement2D::zIndex() const { return m_zIndex; }
void UIElement2D::setZIndex(int zIndex) { m_zIndex = zIndex; }
const Color& UIElement2D::tint() const { return m_tint; }
void UIElement2D::setTint(const Color& tint) { m_tint = tint; }
bool UIElement2D::hitTest(const Point& point) const {
    return m_visible && m_enabled && point.x >= m_frame.origin.x && point.y >= m_frame.origin.y &&
           point.x <= m_frame.origin.x + m_frame.size.width && point.y <= m_frame.origin.y + m_frame.size.height;
}

} // namespace appgametoolbox
