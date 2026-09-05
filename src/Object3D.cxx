#include <AppGameToolbox/Object3D.hpp>

#include <utility>

namespace appgametoolbox {

Object3D::Object3D(std::string name) : m_name(std::move(name)) {}
void Object3D::update(double dt) { (void)dt; }
const std::string& Object3D::name() const { return m_name; }
void Object3D::setName(std::string name) { m_name = std::move(name); }
const Vector3& Object3D::position() const { return m_position; }
void Object3D::setPosition(const Vector3& position) { m_position = position; }
const Vector3& Object3D::rotationDegrees() const { return m_rotationDegrees; }
void Object3D::setRotationDegrees(const Vector3& rotation) { m_rotationDegrees = rotation; }
const Vector3& Object3D::scale() const { return m_scale; }
void Object3D::setScale(const Vector3& scale) { m_scale = scale; }
const Color& Object3D::tint() const { return m_tint; }
void Object3D::setTint(const Color& tint) { m_tint = tint; }
bool Object3D::isVisible() const { return m_visible; }
void Object3D::setVisible(bool visible) { m_visible = visible; }
bool Object3D::isStatic() const { return m_static; }
void Object3D::setStatic(bool value) { m_static = value; }
int Object3D::renderLayer() const { return m_renderLayer; }
void Object3D::setRenderLayer(int layer) { m_renderLayer = layer; }

} // namespace appgametoolbox
