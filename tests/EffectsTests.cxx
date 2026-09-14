#include <AppGameToolbox/Effects.hpp>

#include <array>
#include <cassert>
#include <cmath>

namespace {

using namespace appgametoolbox;

bool samePoint(Point left, Point right) { return left.x == right.x && left.y == right.y; }
bool near(double left, double right) { return std::abs(left - right) < 1e-6; }

void testPresetsAndDeterministicSampling() {
    EffectGenerator generator;
    const Rect bounds = {{10.0, 20.0}, {160.0, 100.0}};
    const std::array<EffectKind, 8> kinds = {EffectKind::AmbientDrift, EffectKind::CrystalBurst, EffectKind::EmberTrail,
                                               EffectKind::NavigationMedallion, EffectKind::PressRipple, EffectKind::SonarPulse,
                                               EffectKind::SparkleOrbit, EffectKind::SignalWave};
    for (EffectKind kind : kinds) {
        EffectSpec spec = EffectGenerator::preset(kind, 3);
        assert(spec.kind == kind);
        assert(spec.seed != 0);
        assert(EffectGenerator::name(kind)[0] != '\0');
        const EffectInstance instance = {spec, 2.0};
        const EffectFrame first = generator.sample(instance, bounds, 2.4);
        const EffectFrame second = generator.sample(instance, bounds, 2.4);
        assert(!first.finished);
        assert(first.primitives.size() == second.primitives.size());
        assert(!first.primitives.empty());
        for (std::size_t index = 0; index < first.primitives.size(); ++index) {
            assert(first.primitives[index].kind == second.primitives[index].kind);
            assert(first.primitives[index].points.size() == second.primitives[index].points.size());
            for (std::size_t point = 0; point < first.primitives[index].points.size(); ++point)
                assert(samePoint(first.primitives[index].points[point], second.primitives[index].points[point]));
        }
        if (kind != EffectKind::AmbientDrift) assert(generator.sample(instance, bounds, 10.0).finished);
    }
}

void testMotionAndParticleControls() {
    EffectGenerator generator;
    EffectSpec crystal = EffectGenerator::preset(EffectKind::CrystalBurst);
    crystal.motion = false;
    const EffectInstance staticCrystal = {crystal, 0.0};
    const EffectFrame early = generator.sample(staticCrystal, {{0, 0}, {100, 100}}, 0.1);
    const EffectFrame late = generator.sample(staticCrystal, {{0, 0}, {100, 100}}, 0.8);
    assert(!early.primitives.empty());
    assert(early.primitives.size() == late.primitives.size());
    assert(samePoint(early.primitives.front().points.front(), late.primitives.front().points.front()));

    crystal.particles = false;
    assert(generator.sample({crystal, 0.0}, {{0, 0}, {100, 100}}, 0.1).primitives.empty());
}

void testSignalWaveMatchesMenuGeometry() {
    EffectGenerator generator;
    EffectSpec wave = EffectGenerator::preset(EffectKind::SignalWave, 2);
    wave.duration = 3.2;
    const EffectFrame frame = generator.sample({wave, 0.0}, {{528, 315}, {420, 197}}, 1.4);
    assert(frame.primitives.size() == 3);
    for (std::size_t band = 0; band < frame.primitives.size(); ++band) {
        const auto& primitive = frame.primitives[band];
        assert(primitive.kind == EffectPrimitiveKind::Polyline);
        assert(primitive.points.size() == 81);
        assert(near(primitive.lineWidth, 1.2));
        assert(near(primitive.opacity, .16 - band * .04));
        assert(near(primitive.points.front().x, 546.0));
        assert(near(primitive.points.front().y, 410.0));
        assert(near(primitive.points.back().x, 926.0));
        assert(near(primitive.points.back().y, 410.0));
    }
}

void testAnchoredRipple() {
    EffectGenerator generator;
    EffectSpec ripple = EffectGenerator::preset(EffectKind::PressRipple);
    ripple.duration = .55;
    ripple.anchor = {.5, .5};
    const EffectFrame frame = generator.sample({ripple, 0.0}, {{64, 230}, {352, 76}}, .275);
    assert(frame.primitives.size() == 1);
    const auto& ring = frame.primitives.front();
    assert(ring.kind == EffectPrimitiveKind::Ring);
    assert(near(ring.points.front().x, 240.0));
    assert(near(ring.points.front().y, 268.0));
    assert(near(ring.radius, 160.0));
    assert(near(ring.opacity, .25));
}

} // namespace

void runEffectsTests() {
    testPresetsAndDeterministicSampling();
    testMotionAndParticleControls();
    testSignalWaveMatchesMenuGeometry();
    testAnchoredRipple();
}
