#include <AppGameToolbox/SpatialAudioPlayer.hpp>

#import <AVFoundation/AVFoundation.h>

#include <algorithm>

namespace appgametoolbox {

class SpatialAudioPlayer::Impl {
public:
    AVAudioEngine* engine = [[AVAudioEngine alloc] init];
    AVAudioEnvironmentNode* environment = [[AVAudioEnvironmentNode alloc] init];
    AVAudioPlayerNode* player = [[AVAudioPlayerNode alloc] init];
    AVAudioFile* file = nil;
    bool paused = false;
    std::string error;
    Impl() {
        [engine attachNode:player]; [engine attachNode:environment];
        [engine connect:environment to:engine.mainMixerNode format:nil];
        environment.renderingAlgorithm = AVAudio3DMixingRenderingAlgorithmHRTF;
    }
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
    return text == nullptr ? "Unknown spatial audio error" : text;
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
AVAudio3DPoint point(float x, float y, float z) { return {x, y, z}; }
} // namespace

SpatialAudioPlayer::SpatialAudioPlayer() : m_impl(std::make_unique<Impl>()) {}
SpatialAudioPlayer::~SpatialAudioPlayer() { stop(); [m_impl->engine detachNode:m_impl->player]; [m_impl->engine detachNode:m_impl->environment]; }
bool SpatialAudioPlayer::load(const char* pathOrUrl) {
    NSURL* url = audioUrl(pathOrUrl);
    if (url == nil || !url.isFileURL) { m_impl->file = nil; m_impl->error = "SpatialAudioPlayer only supports local file paths or file URLs"; return false; }
    NSError* error = nil;
    AVAudioFile* file = [[AVAudioFile alloc] initForReading:url error:&error];
    if (file == nil) { m_impl->file = nil; m_impl->error = errorMessage(error); return false; }
    [m_impl->player stop];
    [m_impl->engine disconnectNodeOutput:m_impl->player];
    [m_impl->engine connect:m_impl->player to:m_impl->environment format:file.processingFormat];
    m_impl->file = file; m_impl->paused = false; m_impl->error.clear();
    return true;
}
bool SpatialAudioPlayer::play() {
    if (m_impl->file == nil || !activateAudioSession(m_impl->error)) return false;
    NSError* error = nil;
    if (!m_impl->engine.isRunning && ![m_impl->engine startAndReturnError:&error]) { m_impl->error = errorMessage(error); return false; }
    if (m_impl->paused) { [m_impl->player play]; m_impl->paused = false; return true; }
    [m_impl->player scheduleFile:m_impl->file atTime:nil completionHandler:nil]; [m_impl->player play]; return true;
}
void SpatialAudioPlayer::pause() { if (m_impl->player.isPlaying) { [m_impl->player pause]; m_impl->paused = true; } }
void SpatialAudioPlayer::stop() { [m_impl->player stop]; [m_impl->engine stop]; m_impl->paused = false; }
bool SpatialAudioPlayer::isLoaded() const { return m_impl->file != nil; }
bool SpatialAudioPlayer::isPlaying() const { return m_impl->player.isPlaying; }
void SpatialAudioPlayer::setVolume(float volume) { m_impl->player.volume = std::clamp(volume, 0.0f, 1.0f); }
float SpatialAudioPlayer::volume() const { return m_impl->player.volume; }
void SpatialAudioPlayer::setPosition(float x, float y, float z) { m_impl->player.position = point(x, y, z); }
void SpatialAudioPlayer::setListenerPosition(float x, float y, float z) { m_impl->environment.listenerPosition = point(x, y, z); }
void SpatialAudioPlayer::setDistanceAttenuation(float referenceDistance, float rolloffFactor) {
    AVAudioEnvironmentDistanceAttenuationParameters* parameters = m_impl->environment.distanceAttenuationParameters;
    parameters.referenceDistance = std::max(0.0f, referenceDistance);
    parameters.rolloffFactor = std::max(0.0f, rolloffFactor);
}
const std::string& SpatialAudioPlayer::lastError() const { return m_impl->error; }

} // namespace appgametoolbox
