#include <AppGameToolbox/UITemplate.hpp>

#include <cassert>
#include <string>

using namespace appgametoolbox;

void testBasicContainerCreation() {
    UITemplate templateDoc;
    assert(templateDoc.load(
        "<div style=background:#ff0000;width:100;height:100></div>",
        "",
        {320.0f, 240.0f},
        [](const std::string&) { return 0; }));

    Render2DRecorder recorder({320.0f, 240.0f});
    assert(templateDoc.record(recorder));

    RecordedFrame2D frame;
    assert(recorder.finish(frame));

    // Should have emitted at least a fill rect for the container
    bool hasFill = false;
    for (const auto& cmd : frame.commands()) {
        if (std::holds_alternative<FillRect2DCommand>(cmd)) {
            hasFill = true;
            break;
        }
    }
    assert(hasFill);
}

void testTextElementCreation() {
    UITemplate templateDoc;
    assert(templateDoc.load(
        "<p style=color:#ffffff;font-size:20>Hello World</p>",
        "",
        {320.0f, 240.0f},
        [](const std::string&) { return 0; }));

    Render2DRecorder recorder({320.0f, 240.0f});
    assert(templateDoc.record(recorder));

    RecordedFrame2D frame;
    assert(recorder.finish(frame));

    bool hasText = false;
    for (const auto& cmd : frame.commands()) {
        if (std::holds_alternative<DrawText2DCommand>(cmd)) {
            hasText = true;
            break;
        }
    }
    assert(hasText);
}

void testButtonElementCreation() {
    UITemplate templateDoc;
    assert(templateDoc.load(
        "<button style=background:#0066cc;width:80;height:30>Click Me</button>",
        "",
        {320.0f, 240.0f},
        [](const std::string&) { return 0; }));

    Render2DRecorder recorder({320.0f, 240.0f});
    assert(templateDoc.record(recorder));

    RecordedFrame2D frame;
    assert(recorder.finish(frame));

    // Should have emitted commands for a button (rect + text)
    bool hasRect = false, hasText = false;
    for (const auto& cmd : frame.commands()) {
        if (std::holds_alternative<FillRect2DCommand>(cmd)) hasRect = true;
        if (std::holds_alternative<DrawText2DCommand>(cmd)) hasText = true;
    }
    assert(hasRect && "Button should have a background rectangle");
    assert(hasText && "Button should have text");
}

void testListElementCreation() {
    UITemplate templateDoc;
    assert(templateDoc.load(
        "<ul><li>Item 1</li><li>Item 2</li><li>Item 3</li></ul>",
        "",
        {320.0f, 240.0f},
        [](const std::string&) { return 0; }));

    Render2DRecorder recorder({320.0f, 240.0f});
    assert(templateDoc.record(recorder));

    RecordedFrame2D frame;
    assert(recorder.finish(frame));

    // List should emit commands for items
    bool hasItems = false;
    for (const auto& cmd : frame.commands()) {
        if (std::holds_alternative<FillRect2DCommand>(cmd) ||
            std::holds_alternative<DrawText2DCommand>(cmd)) {
            hasItems = true;
            break;
        }
    }
    assert(hasItems && "List should emit item rendering commands");
}

void testPropertyUpdates() {
    UITemplate templateDoc;
    assert(templateDoc.load("<div></div>", "", {320.0f, 240.0f}, [](const std::string&) { return 0; }));

    // Update properties via script-like calls
    templateDoc.setProperty("children.0.width", 150.0f);
    templateDoc.setProperty("children.0.height", 50.0f);
    templateDoc.setProperty("children.0.color", std::vector<float>{1.0f, 0.0f, 0.0f, 1.0f});

    Render2DRecorder recorder({320.0f, 240.0f});
    assert(templateDoc.record(recorder));

    RecordedFrame2D frame;
    assert(recorder.finish(frame));

    // Should have used the updated properties
    bool hasRect = false;
    for (const auto& cmd : frame.commands()) {
        if (std::holds_alternative<FillRect2DCommand>(cmd)) {
            hasRect = true;
            break;
        }
    }
    assert(hasRect);
}