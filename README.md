# Windows-WebRTC-SDK-version

## SDK 概述

Windows WebRTC SDK 是一个高度封装的多媒体通信框架，提供了完整的 WebRTC 功能实现，包括音视频传输、数据通道和自定义信令交互。该 SDK 采用分层设计，隐藏了复杂的 WebRTC 原生接口，为开发者提供了简洁易用的 C++ API。

## 核心特性

### 1. 多实例管理
- 支持多个 PeerConnectionFactory 实例
- 支持多个 PeerConnection 连接实例
- 支持多个媒体轨道（视频/音频）
- 支持多个 DataChannel 数据通道

### 2. 媒体处理能力
- 视频轨道创建与自定义视频源注入
- 音频轨道创建与自定义音频源注入
- 支持多种视频编解码器
- 精确的媒体同步控制

### 3. 数据传输
- 可靠有序的 DataChannel 数据传输
- 二进制数据收发支持
- 实时事件传输（鼠标、键盘等）

### 4. 完整的信令交互
- SDP 协商（Offer/Answer）
- ICE Candidate 交换
- WebSocket 信令服务器连接

### 5. 丰富的回调系统
- 信令服务器连接状态通知
- 远程媒体轨道接收通知
- 视频/音频帧接收回调
- PeerConnection 状态变化通知
- ICE 连接状态变化通知
- DataChannel 数据接收回调

## 架构设计

SDK 采用清晰的分层架构设计，各层职责分明：

1. **应用层** - 用户应用程序
2. **公共API层** - WindowsWebRTCManager 提供统一接口
3. **核心管理层** - WebRTCManager 协调管理所有实例
4. **连接管理层** - PeerConnectionManager 管理单个连接
5. **原生接口层** - Google WebRTC Native API

## 主要组件

### WindowsWebRTCManager（公共API）
这是 SDK 的主要入口点，提供所有对外接口：
- 连接管理（connect, disconnect）
- 资源创建（createPeerConnectionFactory, createPeerConnection）
- 媒体轨道管理（createVideoTrack, createAudioTrack）
- 数据通道管理（createDataChannel）
- SDP 协商（createOffer, processAnswer, processOffer）
- ICE 处理（processIceCandidate）
- 媒体帧写入（writeVideoFrame, writeAudioFrame）
- 数据通道写入（writeDataChannelData）

### WebRTCManager（核心管理）
负责协调管理所有 WebRTC 实例和回调处理。

### PeerConnectionManager（连接管理）
管理单个 PeerConnection 实例及其相关资源。

### WebRTC SDK 架构图

```mermaid
graph TB
    subgraph "应用层"
        APP[应用程序<br/>Application]
    end
    
    subgraph "公共API层"
        WAPI[WindowsWebRTCManager<br/>公有接口类]
        
        subgraph "API功能模块"
            APIConnect[连接管理<br/>connect/disConnect]
            APIFactory[工厂创建<br/>createPeerConnectionFactory]
            APIPC[连接实例<br/>createPeerConnection]
            APITrack[媒体轨道<br/>createVideoTrack/createAudioTrack]
            APIData[数据通道<br/>createDataChannel]
            APISDP[SDP协商<br/>createOffer/processAnswer/processOffer]
            APIIce[ICE处理<br/>processIceCandidate]
            APIWrite[媒体写入<br/>writeVideoFrame/writeAudioFrame<br/>writeDataChannelData]
        end
    end
    
    subgraph "核心管理层"
        CORE[WebRTCManager<br/>核心管理器]
        
        subgraph "管理功能"
            MThread[线程管理<br/>network/worker/signaling]
            MFactory[工厂管理<br/>peerConnectionFactorys]
            MPC[连接管理器<br/>peerConnectionManagers]
            MCallback[回调管理<br/>setOn*Handle]
            MSignal[信令处理<br/>webrtcAsyncWrite/onReceiveDataHandle]
        end
    end
    
    subgraph "连接管理层"
        PCM[PeerConnectionManager<br/>连接实例管理器]
        
        subgraph "实例功能"
            PCObserver[PeerConnectionObserver<br/>连接观察者]
            DCObserver[DataChannelObserver<br/>数据通道观察者]
            PCTrack[媒体轨道管理<br/>createVideoTrack/createAudioTrack]
            PCWrite[媒体数据写入<br/>writerVideoFrame/writerAudioFrame]
        end
    end
    
    subgraph "原生接口层"
        NATIVE[Google WebRTC Native API]
        
        subgraph "WebRTC组件"
            WEBRTC[WebRTC Core<br/>核心库]
            MEDIA[音视频编解码<br/>Builtin Codecs]
            NETWORK[网络传输<br/>RTP/ICE/STUN/TURN]
        end
    end
    
    subgraph "回调系统"
        CB[回调处理器]
        
        subgraph "回调类型"
            CBConnect[连接状态<br/>onSignalServerConnectHandle]
            CBP2P[P2P状态<br/>onPeerConnectionStateChangeHandle<br/>onIceConnectionStateChangeHandle]
            CBSDP[SDP事件<br/>onOfferHandle/onAnswerHandle<br/>onIceCandidateHandle]
            CBTrack[媒体接收<br/>onReceiveTrack<br/>onReceiveVideoFrameHandle<br/>onReceiveAudioFrameHandle]
            CBData[数据通道<br/>onDataChannelDataHandle<br/>onReceiveDataChannel]
            CBMsg[消息处理<br/>onReceiveDataHandle]
        end
    end
    
    subgraph "外部组件"
        SIGNAL[信令服务器]
        STUNTURN[ICE服务器<br/>STUN/TURN]
    end
    
    %% 主流程
    APP --> WAPI
    WAPI --> CORE
    CORE --> PCM
    PCM --> NATIVE
    
    %% 外部连接
    CORE --> SIGNAL
    NATIVE --> STUNTURN
    
    %% 回调连接
    PCM --> CB
    CORE --> CB
    
    %% 内部关联
    APIConnect --> MCallback
    APIFactory --> MFactory
    APIPC --> MPC
    APITrack --> PCTrack
    APIData --> DCObserver
    APISDP --> PCObserver
    APIIce --> PCObserver
    APIWrite --> PCWrite
    
    %% 功能关联
    MThread --> WEBRTC
    MFactory --> WEBRTC
    MPC --> PCObserver
    MCallback --> CBTrack
    MSignal --> CBMsg
    
    %% 样式优化
    classDef app fill:#c9daf8,stroke:#1155cc
    classDef api fill:#d9ead3,stroke:#38761d
    classDef core fill:#f4cccc,stroke:#990000
    classDef pc fill:#ead1dc,stroke:#741b47
    classDef native fill:#fce5cd,stroke:#e69138
    classDef callback fill:#d9d2e9,stroke:#351c75
    classDef external fill:#fff2cc,stroke:#bf9000
    
    class APP app
    class WAPI,APIConnect,APIFactory,APIPC,APITrack,APIData,APISDP,APIIce,APIWrite api
    class CORE,MThread,MFactory,MPC,MCallback,MSignal core
    class PCM,PCObserver,DCObserver,PCTrack,PCWrite pc
    class NATIVE,WEBRTC,MEDIA,NETWORK native
    class CB,CBConnect,CBP2P,CBSDP,CBTrack,CBData,CBMsg callback
    class SIGNAL,STUNTURN external
```

### Example Code

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

                timeBeginPeriod(1);

                while (true) {
                    mp3AudioReader->Initialize(L"E:\\cppPro\\WindowsCaptureDemo-version\\qsx.mp3");

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
                                16,  
                                48000, 
                                2,  
                                frame.sampleCount
                            );
                        }

                        next_frame_time += std::chrono::milliseconds(10);
                        std::this_thread::sleep_until(next_frame_time);

                        if (std::chrono::steady_clock::now() > next_frame_time + std::chrono::milliseconds(50)) {
                            next_frame_time = std::chrono::steady_clock::now();
                        }
                    }

                    LOG_INFO("Audio track finished, restarting...");
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }

                timeEndPeriod(1); 
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
