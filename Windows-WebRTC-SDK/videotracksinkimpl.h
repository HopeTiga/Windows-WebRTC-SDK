#pragma once
#include <api/video/video_frame.h>
#include <api/video/video_sink_interface.h>

namespace hope {
namespace rtc {

class WebRTCManager;

class PeerConnectionManager;

class VideoTrackSinkImpl : public webrtc::VideoSinkInterface<webrtc::VideoFrame> {
public:
    VideoTrackSinkImpl(WebRTCManager* manager, PeerConnectionManager * peerConnectionManager,std::string videoTrackId);
    ~VideoTrackSinkImpl() override;

    void OnFrame(const webrtc::VideoFrame& frame) override;

public:
    std::string videoTrackId;// 用于标识视频轨道

private:

    WebRTCManager* manager;

    PeerConnectionManager* peerConnectionManager;

};

}
}
