
#include "WindowsWebRTCManager.h"
#include "WebRTCManager.h"
#include <boost/asio.hpp>

namespace hope {

	namespace rtc {

        webrtc::RtpEncodingParameters toWebRTCEncodingConfig(RtpEncodingConfig config) {

            webrtc::RtpEncodingParameters encoding;

            // 基本参数 - 始终设置
            encoding.active = config.active;

            // 最大比特率 (bps) - 根据标记决定是否设置
            if (config.has_max_bitrate_bps) {
                encoding.max_bitrate_bps = config.max_bitrate_bps;
            }

            // 最小比特率 (bps) - 根据标记决定是否设置
            if (config.has_min_bitrate_bps) {
                encoding.min_bitrate_bps = config.min_bitrate_bps;
            }

            // 最大帧率 (fps) - 根据标记决定是否设置
            if (config.has_max_framerate) {
                encoding.max_framerate = config.max_framerate;
            }

            // 分辨率缩放比例 - 根据标记决定是否设置
            if (config.has_scale_resolution_down_by) {
                encoding.scale_resolution_down_by = config.scale_resolution_down_by;
            }

            // 可扩展性模式 (如 "L1T1", "L2T2" 等) - 根据标记决定是否设置
            if (config.has_scalability_mode) {
                encoding.scalability_mode = config.scalability_mode;
            }

            // 网络优先级 - 始终设置
            encoding.network_priority = webrtc::Priority(config.network_priority);

            // 比特率优先级 - 始终设置
            encoding.bitrate_priority = config.bitrate_priority;

            // 时域层数 - 根据标记决定是否设置
            if (config.has_num_temporal_layers) {
                encoding.num_temporal_layers = config.num_temporal_layers;
            }

            // RID（限制标识符）- 只有非空时才设置
            if (!config.rid.empty()) {
                encoding.rid = config.rid;
            }

            // 注意：以下参数在标准 webrtc::RtpEncodingParameters 中可能没有直接对应
            // - request_key_frame: 通常通过其他机制（如 PLI/FIR）请求关键帧
            // - adaptive_ptime: 可能需要在其他地方配置或不直接支持

            return encoding;
        }

        // Constructor
        WindowsWebRTCManager::WindowsWebRTCManager()
        {
            static boost::asio::io_context ioContext;

            webRTCManager = std::make_shared<WebRTCManager>(ioContext);
        }

        // Destructor
        WindowsWebRTCManager::~WindowsWebRTCManager()
        {
            // webRTCManager will be automatically cleaned up due to unique_ptr
        }

        // Connection management
        void WindowsWebRTCManager::connect(const char* ip,const char * registerJson)
        {
            if (webRTCManager) {
                webRTCManager->connect(ip,registerJson);
            }
        }

        void WindowsWebRTCManager::disConnect()
        {
            if (webRTCManager) {
                webRTCManager->disConnect();
            }
        }

        void WindowsWebRTCManager::disConnectRemote()
        {
            if (webRTCManager) {
                webRTCManager->disConnectRemote();
            }
        }

        int WindowsWebRTCManager::createVideoTrack(WebRTCVideoCodec codec, WebRTCVideoPreference preference, RtpEncodingConfig rtpEncodingConfig)
        {
            if (webRTCManager) {
                return webRTCManager->createVideoTrack(codec, preference, toWebRTCEncodingConfig(rtpEncodingConfig));
            }

            return -1;
        }

        int WindowsWebRTCManager::createAudioTrack()
        {
            if (webRTCManager) {
                return webRTCManager->createAudioTrack();
            }

            return -1;
        }

        int WindowsWebRTCManager::createDataChannel()
        {
            if (webRTCManager) {
                return webRTCManager->createDataChannel();
            }

            return -1;
        }

        void WindowsWebRTCManager::setOnSignalServerConnectHandle(std::function<void()> handle)
        {
            if (webRTCManager) {
                webRTCManager->onSignalServerConnectHandle = handle;
            }
        }

        void WindowsWebRTCManager::setOnSignalServerDisConnectHandle (std::function<void()> handle)
        {
            if (webRTCManager) {
                webRTCManager->onSignalServerDisConnectHandle = handle;
            }
        }

        void WindowsWebRTCManager::setOnRemoteConnectHandle(std::function<void()> handle)
        {
            if (webRTCManager) {
                webRTCManager->onRemoteConnectHandle = handle;
            }
        }

        void WindowsWebRTCManager::setOnRemoteDisConnectHandle(std::function<void()> handle)
        {
            if (webRTCManager) {
                webRTCManager->onRemoteDisConnectHandle = handle;
            }
        }

        void WindowsWebRTCManager::setOnDataChannelDataHandle(std::function<void(const unsigned char*, size_t,int)> handle) {
            if (webRTCManager) {
                webRTCManager->onDataChannelDataHandle = handle;
            }
        }


        void WindowsWebRTCManager::setOnReInitHandle(std::function<void()> func) {

            if (webRTCManager) {
                webRTCManager->onReInitHandle = func;
            }

        }


        void WindowsWebRTCManager::setOnReceiveDataChannel(std::function<void(int)> handle) {
            if (webRTCManager) {
                webRTCManager->onReceiveDataChannel = handle;
            }

        }

        void WindowsWebRTCManager::setOnReceiveTrack(std::function<void(int, int)> handle) {

            if (webRTCManager) {
                webRTCManager->onReceiveTrack = handle;
            }

        }

        void WindowsWebRTCManager::setOnReceiveVideoFrameHandle(std::function<void(int, int, int, const uint8_t*, const uint8_t*, const uint8_t*, int, int, int)> handle) {
            if (webRTCManager) {
                webRTCManager->onReceiveVideoFrameHandle = handle;
            }

        }

        void WindowsWebRTCManager::setOnCreateOfferBeforeHandle(std::function<void()> handle)
        {
            if (webRTCManager) {
                webRTCManager->onCreateOfferBeforeHandle = handle;
            }
        }

        void WindowsWebRTCManager::setOnReceiveOfferBeforeHandle(std::function<void()> handle)
        {
            if (webRTCManager) {
                webRTCManager->onReceiveOfferBeforeHandle = handle;
            }
        }

        // Account management
        void WindowsWebRTCManager::setAccountId(const char* accountId)
        {
            if (webRTCManager) {
                webRTCManager->setAccountId(accountId);
            }
        }

        std::string WindowsWebRTCManager::getAccountId() const
        {
            if (webRTCManager) {
                return webRTCManager->getAccountId();
            }
            return nullptr;
        }

        void WindowsWebRTCManager::setTargetId(const char* targetId)
        {
            if (webRTCManager) {
                webRTCManager->setTargetId(targetId);
            }
        }

        std::string WindowsWebRTCManager::getTargetId() const
        {
            if (webRTCManager) {
                return webRTCManager->getTargetId();
            }
            return nullptr;
        }

        // Media streaming
        bool WindowsWebRTCManager::writeVideoFrame(int videoTrackId,void* data, size_t size, int width, int height)
        {
            if (webRTCManager && data) {
                return webRTCManager->writerVideoFrame(videoTrackId,static_cast<unsigned char*>(data), size, width, height);
            }
            return false;
        }

        bool WindowsWebRTCManager::writeAudioFrame(int audioTrackId,void* data, size_t size)
        {
            if (webRTCManager && data) {
                return webRTCManager->writerAudioFrame(audioTrackId,static_cast<unsigned char*>(data), size);
            }

            return false;
        }

        bool WindowsWebRTCManager::writeDataChannelData(int dataChannelId,void* data, size_t size)
        {
            if (webRTCManager && data) {

                return webRTCManager->writerDataChannelData(dataChannelId,static_cast<unsigned char*>(data), size);

            }
            
            return false;
        }

        WebRTCConnetState WindowsWebRTCManager::getConnectionState() const
        {
            if (webRTCManager) {
                // You'll need to add a getter method to WebRTCManager for the connetState
                // For now, returning a default value
                // You can add: WebRTCConnetState getConnectionState() const { return connetState.load(); }
                // to WebRTCManager class
                return WebRTCConnetState::none;
            }
            return WebRTCConnetState::none;
        }


        void WindowsWebRTCManager::WindowsWebRTCManager::addStunServer(const char* host) {

            if (webRTCManager) {
                webRTCManager->addStunServer(host);
            }
        }

        void WindowsWebRTCManager::addTurnServer(const char* host, const char* username, const char* password) {

            if (webRTCManager) {
                webRTCManager->addTurnServer(host, username, password);
            }

        }

        void WindowsWebRTCManager::writerAsync(const char* json)
        {
            if (webRTCManager) {
                webRTCManager->writerAsync(std::string(json));
            }
        }


	}

}