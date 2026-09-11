#pragma once

#include "Types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace appgametoolbox {

enum class EffectKind {
    AmbientDrift,
    CrystalBurst,
    EmberTrail,
    SonarPulse,
    SparkleOrbit,
    SignalWave,
};

enum class EffectPrimitiveKind { Circle, Line, Ring, Polygon, Polyline };

struct EffectPalette {
    Color primary = {1.0f, 1.0f, 1.0f, 1.0f};
    Color secondary = {1.0f, 1.0f, 1.0f, 1.0f};
};

// The description is renderer-independent and can be stored in markup data,
// game state, or an effects catalog.
struct EffectSpec {
    EffectKind kind = EffectKind::AmbientDrift;
    EffectPalette palette;
    std::uint32_t seed = 1;
    std::size_t particleCount = 36;
    double duration = 1.8;
    bool motion = true;
    bool particles = true;
};

struct EffectInstance {
    EffectSpec spec;
    double startTime = 0.0;
};

struct EffectPrimitive {
    EffectPrimitiveKind kind = EffectPrimitiveKind::Circle;
    Color color;
    // Backends multiply this by color.a when rasterizing the primitive.
    double opacity = 1.0;
    double lineWidth = 1.0;
    double radius = 0.0;
    std::vector<Point> points;
};

struct EffectFrame {
    bool finished = false;
    std::vector<EffectPrimitive> primitives;
};

// Produces deterministic geometry only. A renderer adapter translates these
// primitives to Quartz, Skia, Direct2D, Metal, or another drawing backend.
class EffectGenerator {
public:
    EffectFrame sample(const EffectInstance& instance, const Rect& bounds, double clock) const;

    // Five stable variants are available for every effect kind (0 through 4).
    static EffectSpec preset(EffectKind kind, std::size_t variant = 0);
    static const char* name(EffectKind kind);
};

} // namespace appgametoolbox
