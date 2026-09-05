#include <AppGameToolbox/Toolbox.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace appgametoolbox {

namespace {
float snap(float value, float increment) { return increment > 0.0f ? std::round(value / increment) * increment : value; }
double snap(double value, double increment) { return increment > 0.0 ? std::round(value / increment) * increment : value; }
Vector3 addAndSnap(const Vector3& value, const Vector3& delta, float increment) {
    return {snap(value.x + delta.x, increment), snap(value.y + delta.y, increment), snap(value.z + delta.z, increment)};
}
} // namespace

Toolbox::Toolbox(std::string name) : m_name(std::move(name)) {}
const std::string& Toolbox::name() const { return m_name; }
bool Toolbox::isEnabled() const { return m_enabled; }
void Toolbox::setEnabled(bool enabled) { m_enabled = enabled; }

UIElement2DToolbox::UIElement2DToolbox() : Toolbox("2D Toolbox") {}
ToolboxKind UIElement2DToolbox::kind() const { return ToolboxKind::TwoD; }
void UIElement2DToolbox::move(UIElement2D& element, Point delta) const {
    Rect frame = element.frame();
    const double increment = pixelSnapping ? gridSize : 0.0;
    frame.origin.x = snap(frame.origin.x + delta.x, increment);
    frame.origin.y = snap(frame.origin.y + delta.y, increment);
    element.setFrame(frame);
}
void UIElement2DToolbox::resize(UIElement2D& element, Size size) const {
    Rect frame = element.frame();
    const double increment = pixelSnapping ? gridSize : 0.0;
    frame.size = {std::max(0.0, snap(size.width, increment)), std::max(0.0, snap(size.height, increment))};
    element.setFrame(frame);
}

Object3DToolbox::Object3DToolbox() : Toolbox("3D Toolbox") {}
ToolboxKind Object3DToolbox::kind() const { return ToolboxKind::ThreeD; }
void Object3DToolbox::translate(Object3D& object, Vector3 delta) const { object.setPosition(addAndSnap(object.position(), delta, translationSnap)); }
void Object3DToolbox::rotate(Object3D& object, Vector3 deltaDegrees) const { object.setRotationDegrees(addAndSnap(object.rotationDegrees(), deltaDegrees, rotationSnapDegrees)); }
void Object3DToolbox::scale(Object3D& object, Vector3 multiplier) const {
    const Vector3 value = object.scale();
    object.setScale({snap(value.x * multiplier.x, scaleSnap), snap(value.y * multiplier.y, scaleSnap), snap(value.z * multiplier.z, scaleSnap)});
}

AudioFXToolbox::AudioFXToolbox() : Toolbox("Audio FX Toolbox") {}
ToolboxKind AudioFXToolbox::kind() const { return ToolboxKind::AudioFX; }
bool AudioFXToolbox::preview(AudioFX& effect) const {
    effect.setSpatialized(spatialPreview);
    effect.setGain(std::clamp(previewGain, 0.0f, 1.0f));
    return effect.play();
}
void AudioFXToolbox::setPosition(AudioFX& effect, const Vector3& position) const { effect.setPosition(position); }

} // namespace appgametoolbox
