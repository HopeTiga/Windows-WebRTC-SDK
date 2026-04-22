# Windows-WebRTC-SDK-version

```cpp
#include <iostream>
#include <boost/asio.hpp>
#include <boost/json.hpp>

#include "WindowsWebRTCManager.h"
#include "HWebRTC.h"
#include "Utils.h"

std::string peerConnectionFactoryId;
std::string peerConnectionId;
std::string videoTrackId;
std::string dataChannelId;

int main()
{
    boost::asio::io_context ioContext;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> ioContextWorkPtr = 
        std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
            boost::asio::make_work_guard(ioContext)
        );

    std::shared_ptr<hope::rtc::WindowsWebRTCManager> webrtcManager = 
        std::make_shared<hope::rtc::WindowsWebRTCManager>();
    std::weak_ptr<hope::rtc::WindowsWebRTCManager> weakMgr = webrtcManager;

    webrtcManager->setOnSignalServerConnectHandle( {
        if (auto mgr = weakMgr.lock()) {
            LOG_INFO("Signal server connected");
            boost::json::object jsonObject;
            jsonObject["requestType"] = 0;
            jsonObject["accountId"] = mgr->getAccountId();
            mgr->webrtcAsyncWrite(boost::json::serialize(jsonObject).c_str());
        }
    });

    webrtcManager->setOnOfferHandle(std::string peerConnectionId, std::string sdp {
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

    webrtcManager->setOnAnswerHandle(std::string peerConnectionId, std::string sdp {
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

    webrtcManager->setOnIceCandidateHandle([weakMgr](
        std::string peerConnectionId, 
        std::string candidate, 
        std::string mid, 
        int mlineIndex
    ) {
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

    webrtcManager->setOnDataChannelDataHandle([weakMgr](
        std::string peerConnectionId, 
        std::string dataChannelId, 
        const unsigned char* data, 
        size_t size
    ) {
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

    webrtcManager->setOnReceiveDataHandle(std::string data {
        boost::json::object json = boost::json::parse(data).as_object();
        
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
                        mgr->processIceCandidate(peerConnectionId.c_str(), candidate.c_str(), mid.c_str(), mlineIndex);
                    }
                }
            }
        } else if (json["requestType"].as_int64() == 0) {
            if (json["state"].as_int64() == 200) {
                if (auto mgr = weakMgr.lock()) {
                    peerConnectionFactoryId = mgr->createPeerConnectionFactory();
                    
                    if (!peerConnectionFactoryId.empty()) {
                        peerConnectionId = mgr->createPeerConnection(peerConnectionFactoryId.c_str());
                        
                        if (!peerConnectionId.empty()) {
                            LOG_INFO("createPeerConnection Successful");
                            dataChannelId = mgr->createDataChannel(peerConnectionId.c_str(), "dataChannel");
                            
                            if (!dataChannelId.empty()) {
                                LOG_INFO("createDataChannel Successful");
                            }
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
