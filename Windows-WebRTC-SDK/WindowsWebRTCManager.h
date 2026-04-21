// WindowsWebRTCManager.h
#pragma once

#ifndef WINVER
#define WINVER 0x0601
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_WIN7
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <memory>
#include <string>
#include <functional>
#include "HWebRTC.h"

#ifdef WEBRTCMANAGER_EXPORTS
#define WEBRTCMANAGER_API __declspec(dllexport)
#else
#define WEBRTCMANAGER_API __declspec(dllimport)
#endif

namespace hope {
    namespace rtc {
        class WebRTCManager;

        /**
         * @brief Windows平台WebRTC SDK对外封装层
         * @note 纯透传包装，所有逻辑在WebRTCManager中实现。生命周期由shared_ptr管理。
         */
        class WEBRTCMANAGER_API WindowsWebRTCManager
        {
        public:
            WindowsWebRTCManager();
            ~WindowsWebRTCManager();

            // ==================== 连接管理 ====================

            /**
             * @brief 连接信令服务器（WebSocket over TLS）
             * @param ip 服务器地址，支持"host"或"host:port"格式，默认端口443
             * @note 异步操作，连接成功通过onSignalServerConnectHandle回调通知
             * @see setOnSignalServerConnectHandle, setOnSignalServerDisConnectHandle
             */
            void connect(const char* ip);

            /**
             * @brief 断开所有连接并释放全部资源
             * @note 包括：信令WebSocket、所有PeerConnection、Factory、线程
             * @warning 调用后所有string ID失效，不可继续使用
             */
            void disConnect();

            // ==================== Factory / PeerConnection ====================

            /**
             * @brief 创建PeerConnectionFactory（含网络/工作/信令三线程）
             * @param videoEncoderFactory 自定义视频编码器工厂，nullptr使用内置
             * @param videoDecoderFactory 自定义视频解码器工厂，nullptr使用内置
             * @param audioEncoderFactory 自定义音频编码器工厂，nullptr使用内置
             * @param audioDecoderFactory 自定义音频解码器工厂，nullptr使用内置
             * @return Factory ID（UUID字符串），失败返回空串
             * @note 一个Factory可创建多个PeerConnection，通常一个进程只需一个Factory
             */
            std::string createPeerConnectionFactory();

            /**
             * @brief 创建PeerConnection（P2P连接实例）
             * @param peerConnectionFactoryId createPeerConnectionFactory返回的ID
             * @return PeerConnection ID（UUID字符串），失败返回空串
             * @note 创建后需通过信令交换SDP和ICE Candidate才能连通
             */
            std::string createPeerConnection(const char* peerConnectionFactoryId);

            // ==================== 轨道 / DataChannel ====================

            /**
             * @brief 创建本地视频发送轨道
             * @param peerConnectionId PeerConnection ID
             * @param codec 优先编码器（VP8/VP9/H264/H265/AV1），最终由协商决定
             * @param preference 降级策略（MAINTAIN_FRAMERATE/MAINTAIN_RESOLUTION/BALANCED/DISABLED）
             * @return VideoTrack ID（UUID字符串），失败返回空串
             * @note 创建后通过writeVideoFrame推送I420原始帧
             * @see writeVideoFrame
             */
            std::string createVideoTrack(const char* peerConnectionId, WebRTCVideoCodec codec, WebRTCVideoPreference preference = WebRTCVideoPreference::DISABLED);

            /**
             * @brief 创建本地音频发送轨道
             * @param peerConnectionId PeerConnection ID
             * @return AudioTrack ID（UUID字符串），失败返回空串
             * @note 每个PeerConnection目前仅支持一个音频轨道
             * @note 音频数据通过AudioDeviceModuleImpl推送，不通过本类write接口
             */
            std::string createAudioTrack(const char* peerConnectionId);

            /**
             * @brief 创建DataChannel（可靠有序传输）
             * @param peerConnectionId PeerConnection ID
             * @param label 通道标识名，如"control"、"file"
             * @return DataChannel ID（UUID字符串），失败返回空串
             * @note 必须在ICE连通前创建，连通后创建会失败（WebRTC限制）
             * @see writeDataChannelData, setOnDataChannelDataHandle
             */
            std::string createDataChannel(const char* peerConnectionId, const char* label);

            // ==================== 信令交互 ====================

            /**
             * @brief 创建并发送Offer（主动发起协商）
             * @param peerConnectionId PeerConnection ID
             * @note 异步操作，SDP通过setOnOfferHandle回调返回，需发给远端
             */
            void createOffer(const char* peerConnectionId);

            /**
             * @brief 处理远端Answer（作为Offerer时调用）
             * @param peerConnectionId PeerConnection ID
             * @param sdp 远端返回的Answer SDP文本
             */
            void processAnswer(const char* peerConnectionId, const char* sdp);

            /**
             * @brief 处理远端Offer（作为Answerer时调用）
             * @param peerConnectionId PeerConnection ID
             * @param sdp 远端发来的Offer SDP文本
             * @note 内部自动创建Answer并通过setOnAnswerHandle回调返回
             */
            void processOffer(const char* peerConnectionId, const char* sdp);

            /**
             * @brief 添加远端ICE Candidate
             * @param peerConnectionId PeerConnection ID
             * @param candidate ICE candidate字符串（如"a=candidate:..."）
             * @param mid 媒体标识（如"0"、"1"）
             * @param mlineIndex SDP m-line索引
             * @note 需按顺序在SetRemoteDescription之后添加
             */
            void processIceCandidate(const char* peerConnectionId, const char* candidate, const char* mid, int mlineIndex);

            /**
             * @brief 释放单个PeerConnection及其所有轨道/DataChannel
             * @param peerConnectionId PeerConnection ID
             * @return true成功，false未找到或已释放
             * @note 不会释放Factory，可重新createPeerConnection复用Factory
             */
            bool releasePeerConnection(const char* peerConnectionId);

            // ==================== 媒体写入 ====================

            /**
             * @brief 向视频轨道推送原始帧
             * @param videoTrackId createVideoTrack返回的ID
             * @param data I420格式原始数据指针（Y+U+V连续存储）
             * @param size 数据总字节数，必须等于width*height*3/2
             * @param width 帧宽（像素）
             * @param height 帧高（像素）
             * @return true成功，false参数错误或轨道不存在
             * @note 时间戳由内部自动生成（steady_clock微秒），无需调用方处理
             */
            bool writeVideoFrame(std::string peerConnectionId,const char* videoTrackId, void* data, size_t size, int width, int height);

            /**
             * @brief 向音频轨道推送PCM数据
             * @param audioTrackId createAudioTrack返回的ID
             * @param data PCM音频数据指针
             * @param size 数据字节数
             * @return true成功，false参数错误或轨道不存在
             * @note 实际通过AudioDeviceModuleImpl->PushAudioData转发，采样率/通道数由ADM配置决定
             */
            bool writeAudioFrame(std::string peerConnectionId, const char* audioTrackId, void* data, size_t size);

            /**
             * @brief 通过DataChannel发送数据
             * @param peerConnectionId createPeerConnection返回的ID
             * @param dataChannelId createDataChannel返回的ID
             * @param data 待发送数据指针
             * @param size 数据字节数
             * @return true成功入队（异步发送），false通道不存在或已关闭
             * @note 内部使用SendAsync非阻塞发送，发送失败仅打日志不抛异常
             * @warning 调用方负责data生命周期，SDK内部做CopyOnWriteBuffer拷贝
             */
            bool writeDataChannelData(std::string peerConnectionId, const char* dataChannelId, void* data, size_t size);

            // ==================== 账号/配置 ====================

            /** @brief 设置本地账号ID（透传，不参与RTC逻辑） */
            void setAccountId(const char* accountId);
            /** @brief 获取本地账号ID */
            std::string getAccountId() const;

            /** @brief 设置对端账号ID（透传，不参与RTC逻辑） */
            void setTargetId(const char* targetId);
            /** @brief 获取对端账号ID */
            std::string getTargetId() const;

            /** @brief 添加STUN服务器（用于NAT穿透） */
            void addStunServer(const char* host);
            /**
             * @brief 添加TURN服务器（用于中继转发）
             * @param host TURN服务器地址
             * @param username 认证用户名
             * @param password 认证密码
             */
            void addTurnServer(const char* host, const char* username, const char* password);

            /**
             * @brief 向信令服务器发送原始JSON
             * @param json 待发送JSON字符串
             * @note 异步写入，通过内部队列缓冲。连接断开时自动丢弃
             */
            void webrtcAsyncWrite(const char* json);

            // ==================== 回调设置 ====================
            // 所有回调均在WebRTC内部线程触发，如需更新UI请自行post到主线程

            /**
             * @brief WebSocket连接成功回调
             * @param handle 无参回调
             * @note 在SSL+WebSocket握手完成后触发，此时可开始发送注册信令
             */
            void setOnSignalServerConnectHandle(std::function<void()> handle);

            /**
             * @brief WebSocket连接断开回调
             * @param handle 无参回调
             * @note 网络异常、对端关闭、本地disConnect都会触发
             */
            void setOnSignalServerDisConnectHandle(std::function<void()> handle);

            /**
             * @brief P2P远端连接成功回调
             * @param handle 参数：peerConnectionId
             * @note ICE状态变为Connected时触发，此时媒体/DataChannel可开始传输
             */
            void setOnRemoteConnectHandle(std::function<void(std::string)> handle);

            /**
             * @brief P2P远端连接断开回调
             * @param handle 参数：peerConnectionId
             * @note ICE状态变为Disconnected时触发，可能自动恢复或最终失败
             */
            void setOnRemoteDisConnectHandle(std::function<void(std::string)> handle);

            /**
             * @brief DataChannel收到数据回调
             * @param handle 参数：(peerConnectionId, dataChannelId, data指针, data大小)
             * @note data指针仅在回调执行期间有效，异步处理需先拷贝
             * @warning 回调在WebRTC工作线程执行，禁止阻塞
             */
            void setOnDataChannelDataHandle(std::function<void(std::string, std::string, const unsigned char*, size_t)> handle);

            /**
             * @brief 远端创建DataChannel通知
             * @param handle 参数：(peerConnectionId, dataChannelId)
             * @note 远端主动创建DataChannel时触发，本地需保存dataChannelId用于后续收发
             */
            void setOnReceiveDataChannel(std::function<void(std::string, std::string)> handle);

            /**
             * @brief 远端添加轨道通知
             * @param handle 参数：(peerConnectionId, trackId, trackType)
             * @param trackType 0=video, 1=audio（对应WebRTCTrackType枚举）
             * @note 收到远端SDP中的track后触发，video track会自动注册VideoSink
             * @see setOnReceiveVideoFrameHandle 视频帧通过该回调输出
             */
            void setOnReceiveTrack(std::function<void(std::string, std::string, int)> handle);

            /**
             * @brief 收到远端视频帧回调
             * @param handle 参数：(peerConnectionId, videoTrackId, width, height, Y指针, U指针, V指针, Y步长, U步长, V步长)
             * @note 格式为I420平面，可直接用于渲染或编码
             * @warning 回调在WebRTC解码线程执行，禁止阻塞。如需异步处理，拷贝帧数据
             */
            void setOnReceiveVideoFrameHandle(std::function<void(std::string, std::string, int, int, const uint8_t*, const uint8_t*, const uint8_t*, int, int, int)> handle);

            /**
             * @brief WebSocket收到原始数据回调
             * @param handle 参数：原始字符串
             * @note 设置此回调会覆盖内置JSON解析，所有信令需自行处理
             * @warning 一般不需要设置，由SDK内部解析Offer/Answer/ICE等信令
             */
            void setOnReceiveDataHandle(std::function<void(std::string)> handle);

            /**
             * @brief 本地Offer创建成功回调
             * @param handle 参数：(peerConnectionId, sdp字符串)
             * @note 需通过信令服务器将sdp发送给远端
             */
            void setOnOfferHandle(std::function<void(std::string, std::string)> handle);

            /**
             * @brief 本地Answer创建成功回调
             * @param handle 参数：(peerConnectionId, sdp字符串)
             * @note 需通过信令服务器将sdp发送给远端
             */
            void setOnAnswerHandle(std::function<void(std::string, std::string)> handle);

            /**
             * @brief 本地ICE Candidate生成回调
             * @param handle 参数：(peerConnectionId, candidate字符串, mid, mlineIndex)
             * @note 每生成一个candidate触发一次，需逐个发给远端。也可等gathering complete后批量发送
             * @see PeerConnectionObserverImpl::OnIceGatheringChange 可监听gathering状态
             */
            void setOnIceCandidateHandle(std::function<void(std::string, std::string, std::string, int)> handle);

        private:
            std::shared_ptr<WebRTCManager> webRTCManager;

            WindowsWebRTCManager(const WindowsWebRTCManager&) = delete;
            WindowsWebRTCManager& operator=(const WindowsWebRTCManager&) = delete;
        };
    }
}