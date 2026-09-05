#include <AppGameToolbox/AudioTransport.hpp>

#import <AVFoundation/AVFoundation.h>

#include <algorithm>
#include <limits>

namespace appgametoolbox {

class AudioTransport::Impl {
public:
    enum class State { Stopped, Playing, Paused };
    AVAudioEngine* engine = [[AVAudioEngine alloc] init];
    AVAudioPlayerNode* player = [[AVAudioPlayerNode alloc] init];
    AVAudioFile* file = nil;
    State state = State::Stopped;
    double scheduledStart = 0.0;
    double pausedTime = 0.0;
    std::string error;
    Impl() { [engine attachNode:player]; [engine connect:player to:engine.mainMixerNode format:nil]; }
    double duration() const;
    double timelineTime() const;
    bool startEngine();
    bool scheduleFromCurrentPosition();
};

namespace {
NSURL* audioUrl(const char* pathOrUrl) {
    if (pathOrUrl == nullptr || pathOrUrl[0] == '\0') return nil;
    NSString* value = [NSString stringWithUTF8String:pathOrUrl];
    NSURL* url = [NSURL URLWithString:value];
    return url.scheme == nil ? [NSURL fileURLWithPath:value] : url;
}
std::string errorMessage(NSError* error) {
    const char* text = error == nil ? nullptr : error.localizedDescription.UTF8String;
    return text == nullptr ? "Unknown audio transport error" : text;
}
bool activateAudioSession(std::string& destination) {
#if TARGET_OS_IPHONE
    NSError* error = nil;
    AVAudioSession* session = AVAudioSession.sharedInstance;
    if (![session setCategory:AVAudioSessionCategoryPlayback error:&error] || ![session setActive:YES error:&error]) {
        destination = errorMessage(error); return false;
    }
#else
    (void)destination;
#endif
    return true;
}
} // namespace

double AudioTransport::Impl::duration() const {
    return file == nil || file.processingFormat.sampleRate <= 0.0 ? 0.0 : static_cast<double>(file.length) / file.processingFormat.sampleRate;
}
double AudioTransport::Impl::timelineTime() const {
    if (file == nil || state == State::Stopped) return scheduledStart;
    if (state == State::Paused) return pausedTime;
    AVAudioTime* renderTime = player.lastRenderTime;
    AVAudioTime* playerTime = renderTime == nil ? nil : [player playerTimeForNodeTime:renderTime];
    if (playerTime == nil || playerTime.sampleRate <= 0.0) return scheduledStart;
    return std::min(scheduledStart + static_cast<double>(playerTime.sampleTime) / playerTime.sampleRate, duration());
}
bool AudioTransport::Impl::startEngine() {
    if (engine.isRunning) return true;
    NSError* error = nil;
    if (![engine startAndReturnError:&error]) { this->error = errorMessage(error); return false; }
    return true;
}
bool AudioTransport::Impl::scheduleFromCurrentPosition() {
    if (file == nil || scheduledStart >= duration()) return false;
    const AVAudioFramePosition startFrame = static_cast<AVAudioFramePosition>(scheduledStart * file.processingFormat.sampleRate);
    const AVAudioFramePosition availableFrames = file.length - startFrame;
    const AVAudioFrameCount frameCount = static_cast<AVAudioFrameCount>(std::min(availableFrames, static_cast<AVAudioFramePosition>(std::numeric_limits<AVAudioFrameCount>::max())));
    [player scheduleSegment:file startingFrame:startFrame frameCount:frameCount atTime:nil completionHandler:nil];
    return true;
}

AudioTransport::AudioTransport() : m_impl(std::make_unique<Impl>()) {}
AudioTransport::~AudioTransport() { stop(); [m_impl->engine detachNode:m_impl->player]; }
bool AudioTransport::load(const char* pathOrUrl) {
    NSURL* url = audioUrl(pathOrUrl);
    if (url == nil || !url.isFileURL) { m_impl->file = nil; m_impl->error = "AudioTransport only supports local file paths or file URLs"; return false; }
    NSError* error = nil;
    AVAudioFile* file = [[AVAudioFile alloc] initForReading:url error:&error];
    if (file == nil) { m_impl->file = nil; m_impl->error = errorMessage(error); return false; }
    stop(); m_impl->file = file; m_impl->scheduledStart = 0.0; m_impl->pausedTime = 0.0; m_impl->error.clear(); return true;
}
bool AudioTransport::play() {
    if (m_impl->file == nil || !activateAudioSession(m_impl->error)) return false;
    if (m_impl->state == Impl::State::Playing && m_impl->player.isPlaying) return true;
    if (!m_impl->startEngine()) return false;
    if (m_impl->state == Impl::State::Paused) { [m_impl->player play]; m_impl->state = Impl::State::Playing; return true; }
    if (!m_impl->scheduleFromCurrentPosition()) return false;
    [m_impl->player play]; m_impl->state = Impl::State::Playing; return true;
}
void AudioTransport::pause() { if (m_impl->state == Impl::State::Playing) { m_impl->pausedTime = m_impl->timelineTime(); [m_impl->player pause]; m_impl->state = Impl::State::Paused; } }
void AudioTransport::stop() { [m_impl->player stop]; [m_impl->engine stop]; m_impl->state = Impl::State::Stopped; m_impl->scheduledStart = 0.0; m_impl->pausedTime = 0.0; }
bool AudioTransport::seek(double seconds) {
    if (m_impl->file == nil) return false;
    const bool resume = m_impl->state == Impl::State::Playing;
    [m_impl->player stop]; m_impl->scheduledStart = std::clamp(seconds, 0.0, duration()); m_impl->pausedTime = m_impl->scheduledStart; m_impl->state = Impl::State::Stopped;
    return !resume || play();
}
bool AudioTransport::isLoaded() const { return m_impl->file != nil; }
bool AudioTransport::isPlaying() const { return m_impl->state == Impl::State::Playing && m_impl->player.isPlaying; }
double AudioTransport::currentTime() const { return m_impl->timelineTime(); }
double AudioTransport::duration() const { return m_impl->duration(); }
void AudioTransport::setVolume(float volume) { m_impl->player.volume = std::clamp(volume, 0.0f, 1.0f); }
float AudioTransport::volume() const { return m_impl->player.volume; }
const std::string& AudioTransport::lastError() const { return m_impl->error; }

} // namespace appgametoolbox
