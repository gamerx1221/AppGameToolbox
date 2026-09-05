#pragma once

#include "Types.hpp"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace appgametoolbox {

using Render2DResourceId = std::uint64_t;
using Render2DCacheKey = std::uint64_t;

struct Transform2D {
    double m11 = 1.0;
    double m12 = 0.0;
    double m21 = 0.0;
    double m22 = 1.0;
    double tx = 0.0;
    double ty = 0.0;
};

enum class BlendMode2D { SourceOver, Copy, Multiply, Screen };

struct Save2DCommand {};
struct Restore2DCommand {};
struct Clear2DCommand { Color color; };
struct SetTransform2DCommand { Transform2D transform; };
struct ClipRect2DCommand { Rect rect; };
struct SetOpacity2DCommand { float opacity = 1.0f; };
struct SetBlendMode2DCommand { BlendMode2D mode = BlendMode2D::SourceOver; };
struct FillRect2DCommand { Rect rect; Color color; };
struct StrokeRect2DCommand { Rect rect; Color color; double lineWidth = 1.0; };
struct DrawPath2DCommand { Render2DResourceId path = 0; Color color; double lineWidth = 1.0; bool stroke = false; };
struct DrawImage2DCommand {
    Render2DResourceId image = 0;
    Rect destination;
    Rect source;
    bool hasSourceRect = false;
};
struct DrawText2DCommand {
    Render2DResourceId font = 0;
    std::string text;
    Point origin;
    double fontSize = 12.0;
    Color color;
};

// Cache keys identify a subtree across frames; increment contentVersion when its
// local drawing commands or resources change.
struct BeginCachedLayer2DCommand {
    Render2DCacheKey cacheKey = 0;
    Rect bounds;
    std::uint64_t contentVersion = 0;
};
struct EndCachedLayer2DCommand {};

using Render2DCommand = std::variant<
    Save2DCommand,
    Restore2DCommand,
    Clear2DCommand,
    SetTransform2DCommand,
    ClipRect2DCommand,
    SetOpacity2DCommand,
    SetBlendMode2DCommand,
    FillRect2DCommand,
    StrokeRect2DCommand,
    DrawPath2DCommand,
    DrawImage2DCommand,
    DrawText2DCommand,
    BeginCachedLayer2DCommand,
    EndCachedLayer2DCommand>;

class RecordedFrame2D {
public:
    RecordedFrame2D() = default;

    const Size& canvasSize() const;
    const std::vector<Render2DCommand>& commands() const;

private:
    friend class Render2DRecorder;

    RecordedFrame2D(Size canvasSize, std::vector<Render2DCommand> commands);

    Size m_canvasSize;
    std::vector<Render2DCommand> m_commands;
};

// Records a backend-neutral, immutable command list that can be produced on an
// update thread and replayed later by a backend on its render thread.
class Render2DRecorder {
public:
    explicit Render2DRecorder(Size canvasSize = {});

    void reset(Size canvasSize);
    void save();
    void restore();
    void clear(Color color);
    void setTransform(const Transform2D& transform);
    void clipRect(const Rect& rect);
    void setOpacity(float opacity);
    void setBlendMode(BlendMode2D mode);
    void fillRect(const Rect& rect, Color color);
    void strokeRect(const Rect& rect, Color color, double lineWidth = 1.0);
    void drawPath(Render2DResourceId path, Color color, double lineWidth = 1.0, bool stroke = false);
    void drawImage(Render2DResourceId image, const Rect& destination);
    void drawImage(Render2DResourceId image, const Rect& source, const Rect& destination);
    void drawText(Render2DResourceId font, std::string text, Point origin, double fontSize, Color color);
    bool beginCachedLayer(Render2DCacheKey cacheKey, const Rect& bounds, std::uint64_t contentVersion);
    bool endCachedLayer();

    bool finish(RecordedFrame2D& frame);
    const std::string& lastError() const;
    bool canRecord() const { return m_error.empty() && !m_finished; }

private:
    void fail(const char* message);
    bool canRecord();

    Size m_canvasSize;
    std::vector<Render2DCommand> m_commands;
    std::string m_error;
    std::size_t m_saveDepth = 0;
    std::size_t m_layerDepth = 0;
    bool m_finished = false;
};

enum class CachedLayerPlayback2D { ReplayContents, UseCachedLayer };

// Platform adapters implement this interface. The player owns command traversal,
// including skipping cached-layer contents when a backend has a valid surface.
class Render2DPlayback {
public:
    virtual ~Render2DPlayback() = default;

    virtual void beginFrame(const Size& canvasSize) = 0;
    virtual void endFrame() = 0;
    virtual void save() = 0;
    virtual void restore() = 0;
    virtual void clear(Color color) = 0;
    virtual void setTransform(const Transform2D& transform) = 0;
    virtual void clipRect(const Rect& rect) = 0;
    virtual void setOpacity(float opacity) = 0;
    virtual void setBlendMode(BlendMode2D mode) = 0;
    virtual void fillRect(const FillRect2DCommand& command) = 0;
    virtual void strokeRect(const StrokeRect2DCommand& command) = 0;
    virtual void drawPath(const DrawPath2DCommand& command) = 0;
    virtual void drawImage(const DrawImage2DCommand& command) = 0;
    virtual void drawText(const DrawText2DCommand& command) = 0;
    virtual CachedLayerPlayback2D beginCachedLayer(const BeginCachedLayer2DCommand& command) = 0;
    virtual void endCachedLayer(const BeginCachedLayer2DCommand& command) = 0;
};

class Render2DPlayer {
public:
    // Returns false only when the recorded command structure is malformed.
    bool playback(const RecordedFrame2D& frame, Render2DPlayback& backend) const;

private:
    bool playbackRange(const std::vector<Render2DCommand>& commands, std::size_t first,
                       std::size_t last, Render2DPlayback& backend) const;
    bool findCachedLayerEnd(const std::vector<Render2DCommand>& commands, std::size_t begin,
                            std::size_t& end) const;
};

} // namespace appgametoolbox
