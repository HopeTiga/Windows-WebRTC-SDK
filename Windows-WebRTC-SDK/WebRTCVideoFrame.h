#pragma once
#include <memory>

namespace webrtc {
    class I420BufferInterface;
}

namespace hope {
    namespace rtc {

        class WebRTCVideoFrame {
        public:
            explicit WebRTCVideoFrame(const class webrtc::I420BufferInterface* buffer);
            ~WebRTCVideoFrame();

            WebRTCVideoFrame(const WebRTCVideoFrame&) = delete;
            WebRTCVideoFrame& operator=(const WebRTCVideoFrame&) = delete;
            WebRTCVideoFrame(WebRTCVideoFrame&&) noexcept;
            WebRTCVideoFrame& operator=(WebRTCVideoFrame&&) noexcept;

            const uint8_t* dataY() const;
            const uint8_t* dataU() const;
            const uint8_t* dataV() const;
            int width() const;
            int height() const;
            int strideY() const;
            int strideU() const;
            int strideV() const;

        private:
            struct Impl;
            std::unique_ptr<Impl> pImpl;
        };

    } // namespace rtc
} // namespace hope