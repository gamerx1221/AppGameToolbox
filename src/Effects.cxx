#include <AppGameToolbox/Effects.hpp>

#include <algorithm>
#include <cmath>

namespace appgametoolbox {
namespace {

constexpr double kPi = 3.14159265358979323846;

double unit(std::uint32_t seed, std::size_t index, std::uint32_t channel) {
    std::uint32_t value = seed ^ static_cast<std::uint32_t>(index * 0x9e3779b9U) ^ (channel * 0x85ebca6bU);
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    value ^= value >> 16;
    return static_cast<double>(value) / static_cast<double>(UINT32_MAX);
}

Point center(const Rect& bounds) {
    return {bounds.origin.x + bounds.size.width / 2.0, bounds.origin.y + bounds.size.height / 2.0};
}

void addCircle(EffectFrame& frame, Point point, double radius, Color color, double opacity) {
    frame.primitives.push_back({EffectPrimitiveKind::Circle, color, opacity, 1.0, radius, {point}});
}

void addRing(EffectFrame& frame, Point point, double radius, Color color, double opacity, double lineWidth = 1.0) {
    frame.primitives.push_back({EffectPrimitiveKind::Ring, color, opacity, lineWidth, radius, {point}});
}

} // namespace

const char* EffectGenerator::name(EffectKind kind) {
    switch (kind) {
        case EffectKind::AmbientDrift: return "ambient-drift";
        case EffectKind::CrystalBurst: return "crystal-burst";
        case EffectKind::EmberTrail: return "ember-trail";
        case EffectKind::SonarPulse: return "sonar-pulse";
        case EffectKind::SparkleOrbit: return "sparkle-orbit";
        case EffectKind::SignalWave: return "signal-wave";
    }
    return "unknown";
}

EffectSpec EffectGenerator::preset(EffectKind kind, std::size_t variant) {
    static constexpr Color primary[] = {
        {0.40f, 0.90f, 1.00f, 1.0f}, {1.00f, 0.48f, 0.20f, 1.0f}, {0.43f, 1.00f, 0.72f, 1.0f},
        {1.00f, 0.81f, 0.40f, 1.0f}, {0.72f, 0.50f, 1.00f, 1.0f},
    };
    static constexpr Color secondary[] = {
        {0.82f, 0.98f, 1.00f, 1.0f}, {1.00f, 0.82f, 0.35f, 1.0f}, {0.75f, 1.00f, 0.86f, 1.0f},
        {1.00f, 0.95f, 0.66f, 1.0f}, {0.90f, 0.78f, 1.00f, 1.0f},
    };
    variant %= 5;
    EffectSpec spec;
    spec.kind = kind;
    spec.palette = {primary[variant], secondary[variant]};
    spec.seed = static_cast<std::uint32_t>(0x41c64e6dU + variant * 0x9e3779b9U + static_cast<std::uint32_t>(kind) * 101U);
    spec.particleCount = 24 + variant * 6;
    spec.duration = kind == EffectKind::AmbientDrift ? 0.0 : 1.4 + variant * 0.2;
    return spec;
}

EffectFrame EffectGenerator::sample(const EffectInstance& instance, const Rect& bounds, double clock) const {
    EffectFrame frame;
    if (!std::isfinite(clock) || bounds.size.width <= 0.0 || bounds.size.height <= 0.0) return frame;
    const EffectSpec& spec = instance.spec;
    const std::size_t particleCount = std::min<std::size_t>(spec.particleCount, 1024);
    const double elapsed = std::max(0.0, clock - instance.startTime);
    if (spec.duration > 0.0 && elapsed >= spec.duration) { frame.finished = true; return frame; }
    const double time = spec.motion ? elapsed : 0.0;
    const double progress = spec.duration > 0.0 ? std::clamp(time / spec.duration, 0.0, 1.0) : 0.0;
    const double energy = spec.duration > 0.0 ? std::min(1.0, (1.0 - progress) * 4.0) : 1.0;
    const Point origin = center(bounds);
    const double scale = std::min(bounds.size.width, bounds.size.height);

    switch (spec.kind) {
        case EffectKind::AmbientDrift:
            if (!spec.particles) break;
            for (std::size_t index = 0; index < particleCount; ++index) {
                const double phase = unit(spec.seed, index, 0) * kPi * 2.0;
                const double x = bounds.origin.x + unit(spec.seed, index, 1) * bounds.size.width;
                const double y = bounds.origin.y + std::fmod(unit(spec.seed, index, 2) * bounds.size.height + time * (8.0 + unit(spec.seed, index, 3) * 12.0), bounds.size.height);
                addCircle(frame, {x + std::sin(time + phase) * 4.0, y}, 0.8 + unit(spec.seed, index, 4) * 1.7, spec.palette.primary, 0.18 + unit(spec.seed, index, 5) * 0.35);
            }
            break;
        case EffectKind::CrystalBurst:
            if (!spec.particles) break;
            for (std::size_t index = 0; index < particleCount; ++index) {
                const double angle = unit(spec.seed, index, 0) * kPi * 2.0;
                const double radius = (0.12 + progress * 0.68) * scale * (0.55 + unit(spec.seed, index, 1) * 0.45);
                const Point point = {origin.x + std::cos(angle) * radius, origin.y + std::sin(angle) * radius};
                const double size = 2.0 + unit(spec.seed, index, 2) * 4.0;
                frame.primitives.push_back({EffectPrimitiveKind::Polygon, spec.palette.primary, energy, 1.0, 0.0,
                    {{point.x, point.y - size}, {point.x + size, point.y}, {point.x, point.y + size}, {point.x - size, point.y}}});
            }
            break;
        case EffectKind::EmberTrail:
            if (!spec.particles) break;
            for (std::size_t index = 0; index < particleCount; ++index) {
                const double phase = unit(spec.seed, index, 0) * kPi * 2.0;
                const double x = bounds.origin.x + unit(spec.seed, index, 1) * bounds.size.width + std::sin(time * 2.0 + phase) * 10.0;
                const double y = bounds.origin.y + bounds.size.height * (1.0 - std::fmod(time * .32 + unit(spec.seed, index, 2), 1.0));
                const double trail = 4.0 + unit(spec.seed, index, 3) * 10.0;
                frame.primitives.push_back({EffectPrimitiveKind::Line, spec.palette.primary, energy, 1.0, 0.0, {{x, y}, {x - 3.0, y + trail}}});
                addCircle(frame, {x, y}, 1.2 + unit(spec.seed, index, 4) * 1.8, spec.palette.secondary, energy);
            }
            break;
        case EffectKind::SonarPulse:
            for (std::size_t index = 0; index < 4; ++index) {
                const double phase = std::fmod(progress + index * .25, 1.0);
                addRing(frame, origin, (0.08 + phase * .46) * scale, spec.palette.primary, (1.0 - phase) * energy, 1.5);
            }
            break;
        case EffectKind::SparkleOrbit:
            if (!spec.particles) break;
            for (std::size_t index = 0; index < particleCount; ++index) {
                const double angle = unit(spec.seed, index, 0) * kPi * 2.0 + time * .9;
                const double radius = scale * (.16 + unit(spec.seed, index, 1) * .36);
                const Point point = {origin.x + std::cos(angle) * radius, origin.y + std::sin(angle) * radius};
                const double size = 1.5 + 3.5 * std::pow(std::sin(time * 3.0 + index), 2.0);
                frame.primitives.push_back({EffectPrimitiveKind::Polygon, spec.palette.primary, energy, 1.0, 0.0,
                    {{point.x - size, point.y}, {point.x, point.y - size}, {point.x + size, point.y}, {point.x, point.y + size}}});
            }
            break;
        case EffectKind::SignalWave:
            for (std::size_t band = 0; band < 3; ++band) {
                EffectPrimitive wave;
                wave.kind = EffectPrimitiveKind::Polyline;
                wave.color = band == 1 ? spec.palette.secondary : spec.palette.primary;
                wave.opacity = energy * (.35 + band * .12);
                wave.lineWidth = 1.0 + band * .5;
                for (std::size_t sample = 0; sample <= 32; ++sample) {
                    const double x = bounds.origin.x + bounds.size.width * sample / 32.0;
                    const double phase = sample * .38 + time * (2.0 + band * .35) + band * 1.7;
                    wave.points.push_back({x, origin.y + std::sin(phase) * scale * (.035 + band * .012)});
                }
                frame.primitives.push_back(std::move(wave));
            }
            break;
    }
    return frame;
}

} // namespace appgametoolbox
