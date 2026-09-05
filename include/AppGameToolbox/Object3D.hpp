#pragma once

#include "Types.hpp"

#include <string>

namespace appgametoolbox {

class Object3D {
public:
    explicit Object3D(std::string name = {});
    virtual ~Object3D() = default;

    virtual void update(double dt);
    const std::string& name() const;
    void setName(std::string name);
    const Vector3& position() const;
    void setPosition(const Vector3& position);
    const Vector3& rotationDegrees() const;
    void setRotationDegrees(const Vector3& rotation);
    const Vector3& scale() const;
    void setScale(const Vector3& scale);
    const Color& tint() const;
    void setTint(const Color& tint);
    bool isVisible() const;
    void setVisible(bool visible);
    bool isStatic() const;
    void setStatic(bool value);
    int renderLayer() const;
    void setRenderLayer(int layer);

private:
    std::string m_name;
    Vector3 m_position;
    Vector3 m_rotationDegrees;
    Vector3 m_scale = {1.0f, 1.0f, 1.0f};
    Color m_tint;
    bool m_visible = true;
    bool m_static = false;
    int m_renderLayer = 0;
};

} // namespace appgametoolbox
