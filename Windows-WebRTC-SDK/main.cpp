#include <iostream>
#include <boost/asio.hpp>
#include <boost/json.hpp>

#include "WindowsWebRTCManager.h"

#include "LibWebRTC.h"

#include "Utils.h"

int main()
{

    boost::asio::io_context ioContext;

    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> ioContextWorkPtr = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(boost::asio::make_work_guard(ioContext));

    std::shared_ptr<hope::rtc::WindowsWebRTCManager> webrtcManager = std::make_shared<hope::rtc::WindowsWebRTCManager>();

    std::weak_ptr<hope::rtc::WindowsWebRTCManager> weakMgr = webrtcManager;

    webrtcManager->addStunServer("stun:150.158.173.80:3478");

    webrtcManager->addTurnServer("turn:150.158.173.80:3478", "HopeTiga", "dy913140924");

    webrtcManager->setAccountId("147718387@qq.com");

    webrtcManager->setTargetId("396887208@qq.com");

	boost::json::object registerJson;

	registerJson["requestType"] = static_cast<int64_t>(WebRTCRequestState::REGISTER);

	registerJson["accountId"] = webrtcManager->getAccountId();

    webrtcManager->setOnDataChannelDataHandle([weakMgr](const unsigned char* data, size_t size, int dataChannelId) {
        
        if (auto mgr = weakMgr.lock()) {
            LOG_INFO("webrtcManager->onDataChannelDataHandle :%d",dataChannelId);
            // 处理接收到的数据
        }
        
		});

    webrtcManager->setOnReceiveTrack([weakMgr](int trackId, int trackType) {
        
        if (trackType == 0) {

            LOG_INFO("webrtcManager->onReceiveVideoTrack :%d",trackId);

        }
        else if (trackType == 1) {
        
            LOG_INFO("webrtcManager->onReceiveAudioTrack :%d", trackId);

        }

        });

    webrtcManager->setOnReceiveVideoFrameHandle([weakMgr](int videoTrackId,int width,int height,const uint8_t * dataY, const uint8_t* dataU, const uint8_t* dataV, int strideY, int strideU, int strideV) {
        
        LOG_INFO("webrtcManager->onReceiveVideoFrameHandle :%d, width: %d, height: %d", videoTrackId, width, height);
        // 处理接收到的视频帧

        });

    webrtcManager->setOnSignalServerConnectHandle([weakMgr]() {

        if (auto mgr = weakMgr.lock()) {
            LOG_INFO("SignalServerConnectHandle Register Successful!");
            // 处理接收到的数据

            boost::json::object request;

			request["requestType"] = static_cast<int64_t>(WebRTCRequestState::REQUEST);

			request["accountId"] = mgr->getAccountId();

			request["targetId"] = mgr->getTargetId();

			request["webRTCRemoteState"] = static_cast<int64_t>(WebRTCRemoteState::masterRemote);

            mgr->writerAsync(boost::json::serialize(request).c_str());

        }

        });

    webrtcManager->connect("121.5.37.53:8088",boost::json::serialize(registerJson).c_str());

    ioContext.run();

    return 0;
}


