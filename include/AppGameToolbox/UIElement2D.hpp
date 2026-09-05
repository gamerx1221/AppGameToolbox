#pragma once

#include "Types.hpp"

#include <string>

namespace appgametoolbox {

class UIElement2D {
public:
    explicit UIElement2D(std::string name = {});
    virtual ~UIElement2D() = default;

    virtual void update(double dt);
    const std::string& name() const;
    void setName(std::string name);
    const Rect& frame() const;
    void setFrame(const Rect& frame);
    bool isVisible() const;
    void setVisible(bool visible);
    bool isEnabled() const;
    void setEnabled(bool enabled);
    float opacity() const;
    void setOpacity(float opacity);
    int zIndex() const;
    void setZIndex(int zIndex);
    const Color& tint() const;
    void setTint(const Color& tint);
    bool hitTest(const Point& point) const;

private:
    std::string m_name;
    Rect m_frame;
    Color m_tint;
    bool m_visible = true;
    bool m_enabled = true;
    float m_opacity = 1.0f;
    int m_zIndex = 0;
};

} // namespace appgametoolbox
