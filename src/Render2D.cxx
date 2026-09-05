#include <AppGameToolbox/Render2D.hpp>

#include <algorithm>
#include <type_traits>
#include <utility>

namespace appgametoolbox {

RecordedFrame2D::RecordedFrame2D(Size canvasSize, std::vector<Render2DCommand> commands)
    : m_canvasSize(canvasSize), m_commands(std::move(commands)) {}

const Size& RecordedFrame2D::canvasSize() const { return m_canvasSize; }
const std::vector<Render2DCommand>& RecordedFrame2D::commands() const { return m_commands; }

Render2DRecorder::Render2DRecorder(Size canvasSize) : m_canvasSize(canvasSize) {}

void Render2DRecorder::reset(Size canvasSize) {
    m_canvasSize = canvasSize;
    m_commands.clear();
    m_error.clear();
    m_saveDepth = 0;
    m_layerDepth = 0;
    m_finished = false;
}

void Render2DRecorder::fail(const char* message) {
    if (m_error.empty()) m_error = message;
}

bool Render2DRecorder::canRecord() {
    if (m_finished) fail("Recorder has already finished; call reset before recording another frame");
    return m_error.empty();
}

void Render2DRecorder::save() {
    if (!canRecord()) return;
    m_commands.emplace_back(Save2DCommand{});
    ++m_saveDepth;
}

void Render2DRecorder::restore() {
    if (!canRecord()) return;
    if (m_saveDepth == 0) { fail("Restore called without a matching save"); return; }
    m_commands.emplace_back(Restore2DCommand{});
    --m_saveDepth;
}

void Render2DRecorder::clear(Color color) {
    if (canRecord()) m_commands.emplace_back(Clear2DCommand{color});
}

void Render2DRecorder::setTransform(const Transform2D& transform) {
    if (canRecord()) m_commands.emplace_back(SetTransform2DCommand{transform});
}

void Render2DRecorder::clipRect(const Rect& rect) {
    if (canRecord()) m_commands.emplace_back(ClipRect2DCommand{rect});
}

void Render2DRecorder::setOpacity(float opacity) {
    if (canRecord()) m_commands.emplace_back(SetOpacity2DCommand{std::clamp(opacity, 0.0f, 1.0f)});
}

void Render2DRecorder::setBlendMode(BlendMode2D mode) {
    if (canRecord()) m_commands.emplace_back(SetBlendMode2DCommand{mode});
}

void Render2DRecorder::fillRect(const Rect& rect, Color color) {
    if (canRecord()) m_commands.emplace_back(FillRect2DCommand{rect, color});
}

void Render2DRecorder::strokeRect(const Rect& rect, Color color, double lineWidth) {
    if (canRecord()) m_commands.emplace_back(StrokeRect2DCommand{rect, color, std::max(0.0, lineWidth)});
}

void Render2DRecorder::drawPath(Render2DResourceId path, Color color, double lineWidth, bool stroke) {
    if (canRecord()) m_commands.emplace_back(DrawPath2DCommand{path, color, std::max(0.0, lineWidth), stroke});
}

void Render2DRecorder::drawImage(Render2DResourceId image, const Rect& destination) {
    if (canRecord()) m_commands.emplace_back(DrawImage2DCommand{image, destination, {}, false});
}

void Render2DRecorder::drawImage(Render2DResourceId image, const Rect& source, const Rect& destination) {
    if (canRecord()) m_commands.emplace_back(DrawImage2DCommand{image, destination, source, true});
}

void Render2DRecorder::drawText(Render2DResourceId font, std::string text, Point origin, double fontSize, Color color) {
    if (canRecord()) m_commands.emplace_back(DrawText2DCommand{font, std::move(text), origin, std::max(0.0, fontSize), color});
}

bool Render2DRecorder::beginCachedLayer(Render2DCacheKey cacheKey, const Rect& bounds, std::uint64_t contentVersion) {
    if (!canRecord()) return false;
    if (cacheKey == 0) { fail("Cached layers require a non-zero cache key"); return false; }
    m_commands.emplace_back(BeginCachedLayer2DCommand{cacheKey, bounds, contentVersion});
    ++m_layerDepth;
    return true;
}

bool Render2DRecorder::endCachedLayer() {
    if (!canRecord()) return false;
    if (m_layerDepth == 0) { fail("End cached layer called without a matching begin cached layer"); return false; }
    m_commands.emplace_back(EndCachedLayer2DCommand{});
    --m_layerDepth;
    return true;
}

bool Render2DRecorder::finish(RecordedFrame2D& frame) {
    if (m_finished) { fail("Recorder has already finished; call reset before recording another frame"); return false; }
    if (m_saveDepth != 0) fail("Frame finished with unmatched save commands");
    if (m_layerDepth != 0) fail("Frame finished with unmatched cached layers");
    if (!m_error.empty()) return false;
    frame = RecordedFrame2D(m_canvasSize, std::move(m_commands));
    m_finished = true;
    return true;
}

const std::string& Render2DRecorder::lastError() const { return m_error; }

bool Render2DPlayer::findCachedLayerEnd(const std::vector<Render2DCommand>& commands,
                                         std::size_t begin, std::size_t& end) const {
    std::size_t depth = 0;
    for (std::size_t index = begin; index < commands.size(); ++index) {
        if (std::holds_alternative<BeginCachedLayer2DCommand>(commands[index])) ++depth;
        else if (std::holds_alternative<EndCachedLayer2DCommand>(commands[index]) && --depth == 0) {
            end = index;
            return true;
        }
    }
    return false;
}

bool Render2DPlayer::playbackRange(const std::vector<Render2DCommand>& commands, std::size_t first,
                                    std::size_t last, Render2DPlayback& backend) const {
    for (std::size_t index = first; index < last; ++index) {
        if (const auto* layer = std::get_if<BeginCachedLayer2DCommand>(&commands[index])) {
            std::size_t end = 0;
            if (!findCachedLayerEnd(commands, index, end) || end >= last) return false;
            if (backend.beginCachedLayer(*layer) == CachedLayerPlayback2D::ReplayContents &&
                !playbackRange(commands, index + 1, end, backend)) return false;
            backend.endCachedLayer(*layer);
            index = end;
            continue;
        }
        if (std::holds_alternative<EndCachedLayer2DCommand>(commands[index])) return false;

        std::visit([&backend](const auto& command) {
            using Command = std::decay_t<decltype(command)>;
            if constexpr (std::is_same_v<Command, Save2DCommand>) backend.save();
            else if constexpr (std::is_same_v<Command, Restore2DCommand>) backend.restore();
            else if constexpr (std::is_same_v<Command, Clear2DCommand>) backend.clear(command.color);
            else if constexpr (std::is_same_v<Command, SetTransform2DCommand>) backend.setTransform(command.transform);
            else if constexpr (std::is_same_v<Command, ClipRect2DCommand>) backend.clipRect(command.rect);
            else if constexpr (std::is_same_v<Command, SetOpacity2DCommand>) backend.setOpacity(command.opacity);
            else if constexpr (std::is_same_v<Command, SetBlendMode2DCommand>) backend.setBlendMode(command.mode);
            else if constexpr (std::is_same_v<Command, FillRect2DCommand>) backend.fillRect(command);
            else if constexpr (std::is_same_v<Command, StrokeRect2DCommand>) backend.strokeRect(command);
            else if constexpr (std::is_same_v<Command, DrawPath2DCommand>) backend.drawPath(command);
            else if constexpr (std::is_same_v<Command, DrawImage2DCommand>) backend.drawImage(command);
            else if constexpr (std::is_same_v<Command, DrawText2DCommand>) backend.drawText(command);
        }, commands[index]);
    }
    return true;
}

bool Render2DPlayer::playback(const RecordedFrame2D& frame, Render2DPlayback& backend) const {
    backend.beginFrame(frame.canvasSize());
    const bool completed = playbackRange(frame.commands(), 0, frame.commands().size(), backend);
    backend.endFrame();
    return completed;
}

} // namespace appgametoolbox
