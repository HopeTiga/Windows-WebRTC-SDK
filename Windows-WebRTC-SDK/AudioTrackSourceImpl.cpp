#include "AudioTrackSourceImpl.h"

#include "Utils.h"

namespace hope {

    namespace rtc {

        webrtc::scoped_refptr<AudioTrackSourceImpl> AudioTrackSourceImpl::create(
            const webrtc::AudioOptions* audioOptions) {
            // 使用 webrtc 内部的引用计数构造方式
            auto source = webrtc::make_ref_counted<AudioTrackSourceImpl>();
            source->initialize(audioOptions);
            return source;
        }

        AudioTrackSourceImpl::AudioTrackSourceImpl() {
        }

        AudioTrackSourceImpl::~AudioTrackSourceImpl() {
            webrtc::MutexLock lock(&sinkMutex);
            sinks.clear();
        }

        void AudioTrackSourceImpl::initialize(const webrtc::AudioOptions* options) {
            if (options) {
                audioOptions = *options;
            }
        }

        // 状态必须返回 kLive，否则 Track 会认为源已失效而不发送数据
        webrtc::MediaSourceInterface::SourceState AudioTrackSourceImpl::state() const {
            return kLive;
        }

        // 返回 false，表示这是本地生成的源
        bool AudioTrackSourceImpl::remote() const {
            return false;
        }

        const webrtc::AudioOptions AudioTrackSourceImpl::options() const {
            return audioOptions;
        }

        // 当 AudioTrack 被创建并关联此 Source 时，WebRTC 会自动调用此方法
        void AudioTrackSourceImpl::AddSink(webrtc::AudioTrackSinkInterface* sink) {
            if (!sink) return;
            webrtc::MutexLock lock(&sinkMutex);
            LOG_INFO("AudioTrackSourceImpl::addSink");
            sinks.insert(sink);
        }

        // 当 AudioTrack 销毁时调用
        void AudioTrackSourceImpl::RemoveSink(webrtc::AudioTrackSinkInterface* sink) {
            webrtc::MutexLock lock(&sinkMutex);
            sinks.erase(sink);
        }

        /**
         * 核心分发逻辑：
         * 当你完成了外部混音（PCM数据），调用此函数。
         * 它会遍历所有已连接的 Sink（即所有的 AudioTrack），并将数据分发出去。
         */
        void AudioTrackSourceImpl::onData(const void* audioData,
            int bitsPerSample,
            int sampleRate,
            size_t numberOfChannels,
            size_t numberOfFrames) {
            webrtc::MutexLock lock(&sinkMutex);
            if (sinks.empty()) {
                return;
            }
            for (auto* sink : sinks) {
                sink->OnData(audioData, bitsPerSample, sampleRate, numberOfChannels, numberOfFrames);
            }
        }

    } // namespace rtc

} // namespace hope