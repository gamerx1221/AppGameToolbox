#include <AppGameToolbox/Render2D.hpp>

#include <cassert>
#include <string>
#include <vector>

void runHtmlCssTests();
void testBasicContainerCreation();
void testTextElementCreation();
void testButtonElementCreation();
void testListElementCreation();
void testPropertyUpdates();

namespace {

using namespace appgametoolbox;

class TestPlayback final : public Render2DPlayback {
public:
    bool useCachedLayer = false;
    std::vector<std::string> events;

    void beginFrame(const Size&) override { events.push_back("begin-frame"); }
    void endFrame() override { events.push_back("end-frame"); }
    void save() override { events.push_back("save"); }
    void restore() override { events.push_back("restore"); }
    void clear(Color) override { events.push_back("clear"); }
    void setTransform(const Transform2D&) override { events.push_back("transform"); }
    void clipRect(const Rect&) override { events.push_back("clip"); }
    void setOpacity(float) override { events.push_back("opacity"); }
    void setBlendMode(BlendMode2D) override { events.push_back("blend"); }
    void fillRect(const FillRect2DCommand&) override { events.push_back("fill-rect"); }
    void strokeRect(const StrokeRect2DCommand&) override { events.push_back("stroke-rect"); }
    void drawPath(const DrawPath2DCommand&) override { events.push_back("path"); }
    void drawImage(const DrawImage2DCommand&) override { events.push_back("image"); }
    void drawText(const DrawText2DCommand&) override { events.push_back("text"); }
    CachedLayerPlayback2D beginCachedLayer(const BeginCachedLayer2DCommand&) override {
        events.push_back("begin-cache");
        return useCachedLayer ? CachedLayerPlayback2D::UseCachedLayer : CachedLayerPlayback2D::ReplayContents;
    }
    void endCachedLayer(const BeginCachedLayer2DCommand&) override { events.push_back("end-cache"); }
};

RecordedFrame2D makeFrame() {
    Render2DRecorder recorder({640.0, 480.0});
    recorder.clear({0.0f, 0.0f, 0.0f, 1.0f});
    recorder.save();
    recorder.setOpacity(0.5f);
    assert(recorder.beginCachedLayer(42, {{10.0, 10.0}, {100.0, 100.0}}, 3));
    recorder.fillRect({{0.0, 0.0}, {100.0, 100.0}}, {1.0f, 0.0f, 0.0f, 1.0f});
    assert(recorder.endCachedLayer());
    recorder.restore();

    RecordedFrame2D frame({});
    assert(recorder.finish(frame));
    return frame;
}

void testLivePlayback() {
    TestPlayback backend;
    Render2DPlayer player;
    assert(player.playback(makeFrame(), backend));
    const std::vector<std::string> expected = {
        "begin-frame", "clear", "save", "opacity", "begin-cache", "fill-rect", "end-cache", "restore", "end-frame"};
    assert(backend.events == expected);
}

void testCachedPlaybackSkipsContents() {
    TestPlayback backend;
    backend.useCachedLayer = true;
    Render2DPlayer player;
    assert(player.playback(makeFrame(), backend));
    const std::vector<std::string> expected = {
        "begin-frame", "clear", "save", "opacity", "begin-cache", "end-cache", "restore", "end-frame"};
    assert(backend.events == expected);
}

void testNestedCachedLayersReplayInOrder() {
    Render2DRecorder recorder({640.0, 480.0});
    assert(recorder.beginCachedLayer(1, {{0.0, 0.0}, {100.0, 100.0}}, 1));
    recorder.fillRect({{0.0, 0.0}, {100.0, 100.0}}, {1.0f, 1.0f, 1.0f, 1.0f});
    assert(recorder.beginCachedLayer(2, {{10.0, 10.0}, {50.0, 50.0}}, 1));
    recorder.fillRect({{10.0, 10.0}, {50.0, 50.0}}, {0.0f, 0.0f, 0.0f, 1.0f});
    assert(recorder.endCachedLayer());
    assert(recorder.endCachedLayer());

    RecordedFrame2D frame({});
    assert(recorder.finish(frame));

    TestPlayback backend;
    Render2DPlayer player;
    assert(player.playback(frame, backend));
    const std::vector<std::string> expected = {
        "begin-frame", "begin-cache", "fill-rect", "begin-cache", "fill-rect", "end-cache", "end-cache", "end-frame"};
    assert(backend.events == expected);
}

void testRecorderRejectsInvalidStructure() {
    Render2DRecorder recorder;
    recorder.restore();
    RecordedFrame2D frame({});
    assert(!recorder.finish(frame));
    assert(!recorder.lastError().empty());
}

} // namespace

int main() {
    testLivePlayback();
    testCachedPlaybackSkipsContents();
    testNestedCachedLayersReplayInOrder();
    testRecorderRejectsInvalidStructure();
    runHtmlCssTests();
    testBasicContainerCreation();
    testTextElementCreation();
    testButtonElementCreation();
    testListElementCreation();
    testPropertyUpdates();
}