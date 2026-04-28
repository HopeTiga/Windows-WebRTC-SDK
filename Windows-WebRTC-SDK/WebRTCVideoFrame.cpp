#include "WebRTCVideoFrame.h"
#include <api/scoped_refptr.h>          // 这里才引入 scoped_refptr
#include <api/video/i420_buffer.h>      // I420BufferInterface 的完整定义

namespace hope {
    namespace rtc {

        struct WebRTCVideoFrame::Impl {
            webrtc::scoped_refptr<webrtc::I420BufferInterface> buffer;

            explicit Impl(webrtc::I420BufferInterface* buf)
                : buffer(buf) {  
            }
        };

        WebRTCVideoFrame::WebRTCVideoFrame(const webrtc::I420BufferInterface* buffer)
            : pImpl(std::make_unique<Impl>(const_cast<webrtc::I420BufferInterface*>(buffer))) {
        }

        WebRTCVideoFrame::~WebRTCVideoFrame()
        {
        }

        WebRTCVideoFrame::WebRTCVideoFrame(WebRTCVideoFrame&&) noexcept = default;
        WebRTCVideoFrame& WebRTCVideoFrame::operator=(WebRTCVideoFrame&&) noexcept = default;

        // 转发各个访问成员
        const uint8_t* WebRTCVideoFrame::dataY() const {
            return pImpl->buffer->DataY();
        }

        const uint8_t* WebRTCVideoFrame::dataU() const {
            return pImpl->buffer->DataU();
        }

        const uint8_t* WebRTCVideoFrame::dataV() const {
            return pImpl->buffer->DataV();
        }

        int WebRTCVideoFrame::width() const {
            return pImpl->buffer->width();
        }

        int WebRTCVideoFrame::height() const {
            return pImpl->buffer->height();
        }

        int WebRTCVideoFrame::strideY() const {
            return pImpl->buffer->StrideY();
        }

        int WebRTCVideoFrame::strideU() const {
            return pImpl->buffer->StrideU();
        }

        int WebRTCVideoFrame::strideV() const {
            return pImpl->buffer->StrideV();
        }

    } // namespace rtc
} // namespace hope