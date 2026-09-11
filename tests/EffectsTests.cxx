#include <AppGameToolbox/Effects.hpp>

#include <array>
#include <cassert>

namespace {

using namespace appgametoolbox;

bool samePoint(Point left, Point right) { return left.x == right.x && left.y == right.y; }

void testPresetsAndDeterministicSampling() {
    EffectGenerator generator;
    const Rect bounds = {{10.0, 20.0}, {160.0, 100.0}};
    const std::array<EffectKind, 6> kinds = {EffectKind::AmbientDrift, EffectKind::CrystalBurst, EffectKind::EmberTrail,
                                               EffectKind::SonarPulse, EffectKind::SparkleOrbit, EffectKind::SignalWave};
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

} // namespace

void runEffectsTests() {
    testPresetsAndDeterministicSampling();
    testMotionAndParticleControls();
}
