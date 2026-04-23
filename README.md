# Windows-WebRTC-SDK-version

```cpp
#define HOPE_RTC_CONTROLLER
#undef WIN32_LEAN_AND_MEAN

#include "WindowsWebRTCManager.h"
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

#include <iostream>
#include <boost/asio.hpp>
#include <boost/json.hpp>


#include "HWebRTC.h"

#include "Utils.h"

#include "E:\cppPro\WindowsCaptureDemo-version\WindowsCaptureDemo\MP3AudioReader.h"

std::string peerConnectionFactoryId;

std::string peerConnectionId;

std::string videoTrackId;

std::string audioTrackId;

std::string dataChannelId;

auto mp3AudioReader = std::make_shared<MP3AudioReader>();

int main()
{

    boost::asio::io_context ioContext;

    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> ioContextWorkPtr = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(boost::asio::make_work_guard(ioContext));

    std::shared_ptr<hope::rtc::WindowsWebRTCManager> webrtcManager = std::make_shared<hope::rtc::WindowsWebRTCManager>();

    std::weak_ptr<hope::rtc::WindowsWebRTCManager> weakMgr = webrtcManager;

    webrtcManager->setOnSignalServerConnectHandle([weakMgr]() {
        
        if (auto mgr = weakMgr.lock()) {

            LOG_INFO("Signal server connected");

            boost::json::object jsonObject;

            jsonObject["requestType"] = 0;

			jsonObject["accountId"] = mgr->getAccountId();

			mgr->webrtcAsyncWrite(boost::json::serialize(jsonObject).c_str());
        }

        });

    webrtcManager->setOnOfferHandle([weakMgr](std::string peerConnectionId, std::string sdp) {
        
        if (auto mgr = weakMgr.lock()) {
        
            boost::json::object jsonObject;

            jsonObject["requestType"] = 1;

            jsonObject["accountId"] = mgr->getAccountId();

            jsonObject["targetId"] = mgr->getTargetId();

            jsonObject["sdp"] = sdp;

            jsonObject["type"] = "offer";

            mgr->webrtcAsyncWrite(boost::json::serialize(jsonObject).c_str());

        }

        });

    webrtcManager->setOnAnswerHandle([weakMgr](std::string peerConnectionId, std::string sdp) {

        if (auto mgr = weakMgr.lock()) {

            boost::json::object jsonObject;

            jsonObject["requestType"] = 1;

            jsonObject["accountId"] = mgr->getAccountId();

            jsonObject["targetId"] = mgr->getTargetId();

            jsonObject["sdp"] = sdp;

            jsonObject["type"] = "answer";

            mgr->webrtcAsyncWrite(boost::json::serialize(jsonObject).c_str());

        }

        });

    webrtcManager->setOnIceCandidateHandle([weakMgr](std::string peerConnectionId,std::string candidate,std::string mid,int mlineIndex) {
        
        if (auto mgr = weakMgr.lock()) {

            boost::json::object jsonObject;

            jsonObject["requestType"] = 1;

            jsonObject["accountId"] = mgr->getAccountId();

            jsonObject["targetId"] = mgr->getTargetId();

            jsonObject["type"] = "candidate";

            jsonObject["candidate"] = candidate;

            jsonObject["mid"] = mid;

            jsonObject["mlineIndex"] = mlineIndex;

            mgr->webrtcAsyncWrite(boost::json::serialize(jsonObject).c_str());
        }

		});

    webrtcManager->setOnReceiveTrack([weakMgr](std::string peerConnectionId, std::string trackId, int trackType) {
        LOG_INFO("Received remote track: PeerConnectionId=%s, TrackId=%s, TrackType=%d",
            peerConnectionId.c_str(), trackId.c_str(), trackType);
		});

    webrtcManager->setOnReceiveVideoFrameHandle([weakMgr](std::string peerConnectionId,std::string videoTrackId,int width,int height
        , const uint8_t* dataY, const uint8_t* dataU, const uint8_t* dataV, int widthY, int widthU, int widthV) {
            
			static bool firstFrame = true;

            if (firstFrame) {

                LOG_INFO("PeerConnection[%s] received VideoTrack[%s] VideoFrame", peerConnectionId.c_str(), videoTrackId.c_str());

				firstFrame = false;

            }

        });

    webrtcManager->setOnReceiveAudioFrameHandle([weakMgr](std::string peerConnectionId, std::string audioTrackId, const void* pcmData, int bitsPerSample, int sampleRate, size_t numberOfChannels, size_t numberOfFrames) {
        static bool firstFrame = true;
        if (firstFrame) {
            LOG_INFO("PeerConnection[%s] received AudioTrack[%s] AudioFrame: %dHz, %d channels, %d frames",
                peerConnectionId.c_str(), audioTrackId.c_str(), sampleRate, numberOfChannels, numberOfFrames);
            firstFrame = false;
        }
		});

    webrtcManager->setOnPeerConnectionStateChangeHandle([weakMgr](std::string peerConnectionId, int type) {
        static const char* stateNames[] = {
            "New", "Connecting", "Connected", "Disconnected", "Failed", "Closed"
        };

        const char* name = (type >= 0 && type < 6) ? stateNames[type] : "Unknown";
        LOG_INFO("PeerConnection[%s] PeerConnection state: %s", peerConnectionId.c_str(), name);
        });

    webrtcManager->setOnIceConnectionStateChangeHandle([weakMgr](std::string peerConnectionId, int type) {
        static const char* stateNames[] = {
            "New",
            "Checking",
            "Connected",
            "Completed",
            "Failed",
            "Disconnected",
            "Closed",
            "Max"
        };

        const char* stateName = (type >= 0 && type < 8) ? stateNames[type] : "Unknown";
        LOG_INFO("PeerConnection[%s] ICE state changed to: %s", peerConnectionId.c_str(), stateName);

        if (type == IceConnectionState::kIceConnectionDisconnected) {
        
            LOG_INFO("PeerConnection[%s] ICE disconnected, releasing PeerConnection", peerConnectionId.c_str());
            if (auto mgr = weakMgr.lock()) {
                mgr->releasePeerConnection(peerConnectionId.c_str());
			}

        }
        else if (type == IceConnectionState::kIceConnectionConnected) {
        
            LOG_INFO("ICE Connected! Starting Audio Broadcast Thread...");

            std::thread audioThread([weakMgr, peerConnectionId]() {
                auto mgr = weakMgr.lock();
                if (!mgr) return;

                // 【核心修复】：强制 Windows 开启 1ms 定时器精度！
                // 没有这句，下面的 sleep_until 照样不准。
                timeBeginPeriod(1);

                while (true) {
                    mp3AudioReader->Initialize(L"E:\\cppPro\\WindowsCaptureDemo-version\\qsx.mp3");

                    // 记录发送的绝对基准时间
                    auto next_frame_time = std::chrono::steady_clock::now();

                    while (!mp3AudioReader->IsEndOfFile()) {
                        MP3AudioReader::AudioFrame frame;

                        // 严格读取 10ms
                        HRESULT hr = mp3AudioReader->ReadOpusFrame(frame, MP3AudioReader::OPUS_FRAME_10MS);
                        if (hr == S_FALSE) break;

                        if (SUCCEEDED(hr)) {
                            mgr->writeAudioFrame(peerConnectionId.c_str(),
                                audioTrackId.c_str(),
                                reinterpret_cast<unsigned char*>(frame.data.data()),
                                16,           // bitsPerSample
                                48000,        // sampleRate 
                                2,            // 【保持为 1】：因为底层确实配置成了单声道！
                                frame.sampleCount // 480 
                            );
                        }

                        // 【核心修复】：基于绝对时间的精准等待，彻底消除累积误差
                        next_frame_time += std::chrono::milliseconds(10);
                        std::this_thread::sleep_until(next_frame_time);

                        // 防御性重置：如果系统卡死导致落后太多，不要疯狂快进补发
                        if (std::chrono::steady_clock::now() > next_frame_time + std::chrono::milliseconds(50)) {
                            next_frame_time = std::chrono::steady_clock::now();
                        }
                    }

                    LOG_INFO("Audio track finished, restarting...");
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }

                timeEndPeriod(1); // 退出时恢复系统默认时钟精度
                LOG_INFO("Audio thread exited.");
                });

            audioThread.detach();

        }

        });

    webrtcManager->setOnDataChannelDataHandle([weakMgr](std::string peerConnectionId, std::string dataChannelId, const unsigned char* data, size_t size) {
        if (size < sizeof(short)) return;

        // 获取事件类型
        short eventType = *reinterpret_cast<const short*>(data);

        switch (eventType) {
        case 0: { // 鼠标移动
            if (size >= 10) { // short(2) + uint32(4) + uint32(4)
                uint32_t x = *reinterpret_cast<const uint32_t*>(data + 2);
                uint32_t y = *reinterpret_cast<const uint32_t*>(data + 6);
                LOG_INFO("[Mouse Move] X: %u, Y: %u", x, y);
            }
            break;
        }
        case 1:   // 鼠标按下
        case 2: { // 鼠标抬起
            if (size >= 12) { // short*2 + int*2
                short mouseButton = *reinterpret_cast<const short*>(data + 2);
                int rawX = *reinterpret_cast<const int*>(data + 4);
                int rawY = *reinterpret_cast<const int*>(data + 8);
                LOG_INFO("[Mouse %s] Button: %d, RawPos: (%d, %d)",
                    (eventType == 1 ? "Down" : "Up"), mouseButton, rawX, rawY);
            }
            break;
        }
        case 3:   // 键盘按下
        case 4: { // 键盘抬起
            if (size >= 4) { // short(2) + byte(1) + byte(1)
                unsigned char keyCode = data[2];
                unsigned char keyFlags = data[3];
                LOG_INFO("[Key %s] Code: %u, Flags: %u",
                    (eventType == 3 ? "Down" : "Up"), keyCode, keyFlags);
            }
            break;
        }
        case 5: { // 鼠标滚轮
            if (size >= 6) { // short(2) + int(4)
                int delta = *reinterpret_cast<const int*>(data + 2);
                LOG_INFO("[Mouse Wheel] Delta: %d", delta);
            }
            break;
        }
        default:
            LOG_INFO("[Unknown Event] Type: %d, Size: %zu", eventType, size);
            break;
        }
        });

    webrtcManager->setOnReceiveDataHandle([weakMgr](std::string data) {
       
        boost::json::object json =  boost::json::parse(data).as_object();

        if (json["requestType"].as_int64() == 1) {
        
            if (auto mgr = weakMgr.lock()) {

                if (json.contains("webRTCRemoteState")) {

                    mgr->setTargetId(json["accountId"].as_string().c_str());
                
                    mgr->createOffer(peerConnectionId.c_str());

                }
                else if (json.contains("type")) {
                
                    if (json["type"].as_string() == "offer") {
                    
                        mgr->processOffer(peerConnectionId.c_str(), json["sdp"].as_string().c_str());

                    }
                    else if (json["type"].as_string() == "answer") {
                    
                        mgr->processAnswer(peerConnectionId.c_str(), json["sdp"].as_string().c_str());

                    }
                    else if (json["type"].as_string() == "candidate") {

                        std::string candidate(json["candidate"].as_string().c_str());
                        std::string mid = json.contains("mid") ? std::string(json["mid"].as_string().c_str()) : "";
                        int mlineIndex = json.contains("mlineIndex") ? static_cast<int>(json["mlineIndex"].as_int64()) : 0;
                    
                        mgr->processIceCandidate(peerConnectionId.c_str(), candidate.c_str(), mid.c_str(),mlineIndex);

                    }

                }

            }

        }else if (json["requestType"].as_int64() == 0) {

            if (json["state"].as_int64() == 200) {
            
                if (auto mgr = weakMgr.lock()) {
                
                    peerConnectionFactoryId =   mgr->createPeerConnectionFactory(false);

                    if (!peerConnectionFactoryId.empty()) {
                    
                        peerConnectionId = mgr->createPeerConnection(peerConnectionFactoryId.c_str());

                        if (!peerConnectionId.empty()) {
                        
                            LOG_INFO("createPeerConnection Successful");

#ifdef HOPE_RTC_CONTROLLER  

							dataChannelId = mgr->createDataChannel(peerConnectionId.c_str(), "dataChannel");

                            if (!dataChannelId.empty()) {
                            
								LOG_INFO("createDataChannel Successful");

                            }


							audioTrackId = mgr->createAudioTrack(peerConnectionId.c_str(), "audioTrack");

                            if (!audioTrackId.empty()) {

                                LOG_INFO("createAudioTrack Successful");

                            }

#else

                            boost::json::object json;

                            json["requestType"] = 1;

                            json["webRTCRemoteState"] = 1;

                            json["accountId"] = mgr->getAccountId();

                            json["targetId"] = mgr->getTargetId();

                            json["webrtcAudioEnable"] = 1;

                            mgr->webrtcAsyncWrite(boost::json::serialize(json).c_str());
#endif

                        }
                    
                    }

                }

            }

        }

        });

    webrtcManager->addStunServer("stun:");

    webrtcManager->addTurnServer("turn:", "", "");

    webrtcManager->setAccountId("");

    webrtcManager->setTargetId("");

    webrtcManager->connect("");

    ioContext.run();

    return 0;
}
```
