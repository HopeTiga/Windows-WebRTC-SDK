#include "VideoTrackSinkImpl.h"
#include "WebRTCManager.h"
#include "Utils.h" // 确保包含 VideoFrame 的定义

namespace hope {
namespace rtc {

    VideoTrackSinkImpl::VideoTrackSinkImpl(WebRTCManager* manager, PeerConnectionManager* peerConnectionManager, std::string videoTrackId) : manager(manager), peerConnectionManager(peerConnectionManager), videoTrackId(videoTrackId) {
}

VideoTrackSinkImpl::~VideoTrackSinkImpl() {
}

void VideoTrackSinkImpl::OnFrame(const webrtc::VideoFrame& frame) {

    if (!manager || !manager->onReceiveVideoFrameHandle) return;

    webrtc::scoped_refptr<webrtc::I420BufferInterface> buffer = frame.video_frame_buffer()->ToI420();

    if (manager->onReceiveVideoFrameHandle) {

        manager->onReceiveVideoFrameHandle(peerConnectionManager->peerConnectionId, videoTrackId, buffer->width(), buffer->height(), buffer->DataY(), buffer->DataU(), buffer->DataV(), buffer->StrideY(), buffer->StrideU(), buffer->StrideV());

    }

}


}
}
