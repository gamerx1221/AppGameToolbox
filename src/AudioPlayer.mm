#include <AppGameToolbox/AudioPlayer.hpp>

#import <AVFoundation/AVFoundation.h>

#include <algorithm>

namespace appgametoolbox {

class AudioPlayer::Impl { public: AVAudioPlayer* player = nil; std::string error; };

namespace {
NSURL* audioUrl(const char* pathOrUrl) {
    if (pathOrUrl == nullptr || pathOrUrl[0] == '\0') return nil;
    NSString* value = [NSString stringWithUTF8String:pathOrUrl];
    NSURL* url = [NSURL URLWithString:value];
    return url.scheme == nil ? [NSURL fileURLWithPath:value] : url;
}
std::string errorMessage(NSError* error) {
    const char* text = error == nil ? nullptr : error.localizedDescription.UTF8String;
    return text == nullptr ? "Unknown audio error" : text;
}
bool activateAudioSession(std::string& destination) {
#if TARGET_OS_IPHONE
    NSError* error = nil;
    AVAudioSession* session = AVAudioSession.sharedInstance;
    if (![session setCategory:AVAudioSessionCategoryPlayback error:&error] || ![session setActive:YES error:&error]) {
        destination = errorMessage(error);
        return false;
    }
#else
    (void)destination;
#endif
    return true;
}
} // namespace

AudioPlayer::AudioPlayer() : m_impl(std::make_unique<Impl>()) {}
AudioPlayer::~AudioPlayer() = default;
bool AudioPlayer::load(const char* pathOrUrl) {
    NSURL* url = audioUrl(pathOrUrl);
    if (url == nil || !url.isFileURL) { m_impl->player = nil; m_impl->error = "AudioPlayer only supports local file paths or file URLs"; return false; }
    NSError* error = nil;
    AVAudioPlayer* player = [[AVAudioPlayer alloc] initWithContentsOfURL:url error:&error];
    if (player == nil) { m_impl->player = nil; m_impl->error = errorMessage(error); return false; }
    m_impl->player = player;
    m_impl->error.clear();
    return [m_impl->player prepareToPlay];
}
bool AudioPlayer::play() { return m_impl->player != nil && activateAudioSession(m_impl->error) && [m_impl->player play]; }
void AudioPlayer::pause() { [m_impl->player pause]; }
void AudioPlayer::stop() { [m_impl->player stop]; m_impl->player.currentTime = 0.0; }
bool AudioPlayer::isLoaded() const { return m_impl->player != nil; }
bool AudioPlayer::isPlaying() const { return m_impl->player != nil && m_impl->player.isPlaying; }
void AudioPlayer::setVolume(float volume) { m_impl->player.volume = std::clamp(volume, 0.0f, 1.0f); }
float AudioPlayer::volume() const { return m_impl->player == nil ? 0.0f : m_impl->player.volume; }
double AudioPlayer::duration() const { return m_impl->player == nil ? 0.0 : m_impl->player.duration; }
double AudioPlayer::currentTime() const { return m_impl->player == nil ? 0.0 : m_impl->player.currentTime; }
void AudioPlayer::setCurrentTime(double seconds) { if (m_impl->player != nil) m_impl->player.currentTime = std::max(0.0, seconds); }
const std::string& AudioPlayer::lastError() const { return m_impl->error; }

} // namespace appgametoolbox
