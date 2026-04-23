#pragma once

#include "api/audio_options.h"
#include "api/media_stream_interface.h"
#include "api/notifier.h"
#include "api/scoped_refptr.h"
#include "rtc_base/synchronization/mutex.h"
#include <set>

namespace hope {

    namespace rtc {

        class AudioTrackSourceImpl : public webrtc::Notifier<webrtc::AudioSourceInterface>
        {
        public:
            // 创建实例的工厂方法
            static webrtc::scoped_refptr<AudioTrackSourceImpl> create(
                const webrtc::AudioOptions* audioOptions);

            // MediaSourceInterface 实现
            SourceState state() const override;
            bool remote() const override;

            // AudioSourceInterface 实现
            const webrtc::AudioOptions options() const override;
            void AddSink(webrtc::AudioTrackSinkInterface* sink)override;
            void RemoveSink(webrtc::AudioTrackSinkInterface* sink)override;

            // 外部注入数据的接口（混音后调用此函数）
            void onData(const void* audioData,
                int bitsPerSample,
                int sampleRate,
                size_t numberOfChannels,
                size_t numberOfFrames);

            AudioTrackSourceImpl();
            ~AudioTrackSourceImpl() override;

        private:
            void initialize(const webrtc::AudioOptions* audioOptions);

            webrtc::AudioOptions audioOptions;
            webrtc::Mutex sinkMutex;
            std::set<webrtc::AudioTrackSinkInterface*> sinks;
        };

    } // namespace rtc

} // namespace hope