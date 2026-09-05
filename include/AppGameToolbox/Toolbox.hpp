#pragma once

#include "AudioFX.hpp"
#include "Object3D.hpp"
#include "UIElement2D.hpp"

#include <string>

namespace appgametoolbox {

enum class ToolboxKind { TwoD, ThreeD, AudioFX };

class Toolbox {
public:
    explicit Toolbox(std::string name);
    virtual ~Toolbox() = default;
    virtual ToolboxKind kind() const = 0;
    const std::string& name() const;
    bool isEnabled() const;
    void setEnabled(bool enabled);

private:
    std::string m_name;
    bool m_enabled = true;
};

class UIElement2DToolbox final : public Toolbox {
public:
    UIElement2DToolbox();
    ToolboxKind kind() const override;
    bool pixelSnapping = true;
    double gridSize = 1.0;
    void move(UIElement2D& element, Point delta) const;
    void resize(UIElement2D& element, Size size) const;
};

class Object3DToolbox final : public Toolbox {
public:
    Object3DToolbox();
    ToolboxKind kind() const override;
    float translationSnap = 0.0f;
    float rotationSnapDegrees = 0.0f;
    float scaleSnap = 0.0f;
    void translate(Object3D& object, Vector3 delta) const;
    void rotate(Object3D& object, Vector3 deltaDegrees) const;
    void scale(Object3D& object, Vector3 multiplier) const;
};

class AudioFXToolbox final : public Toolbox {
public:
    AudioFXToolbox();
    ToolboxKind kind() const override;
    float previewGain = 1.0f;
    bool spatialPreview = false;
    bool preview(AudioFX& effect, AudioPlayback& standardPlayback,
                 SpatialAudioPlayback* spatialPlayback = nullptr) const;
    void setPosition(AudioFX& effect, const Vector3& position) const;
};

} // namespace appgametoolbox
