#pragma once
#include <memory>

// 使用与 WindowsWebRTCManager 相同的导出宏逻辑，但不引入其他依赖
#ifdef WEBRTCMANAGER_EXPORTS
#define WEBRTC_VIDEO_FRAME_API __declspec(dllexport)
#else
#define WEBRTC_VIDEO_FRAME_API __declspec(dllimport)
#endif

namespace webrtc {
    class I420BufferInterface;
}

namespace hope {
    namespace rtc {

        class WEBRTC_VIDEO_FRAME_API WebRTCVideoFrame {   // ← 添加导出宏
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