// WindowsWebRTCManager.cpp
#include "WindowsWebRTCManager.h"
#include "WebRTCManager.h"
#include <boost/asio.hpp>

namespace hope {
    namespace rtc {

        WindowsWebRTCManager::WindowsWebRTCManager()
        {
            static boost::asio::io_context ioContext;
            webRTCManager = std::make_shared<WebRTCManager>(ioContext);
        }

        WindowsWebRTCManager::~WindowsWebRTCManager() = default;

        void WindowsWebRTCManager::connect(const char* ip)
        {
            if (webRTCManager) webRTCManager->connect(ip);
        }

        void WindowsWebRTCManager::disConnect()
        {
            if (webRTCManager) webRTCManager->disConnect();
        }

        std::string WindowsWebRTCManager::createPeerConnectionFactory()
        {
            if (!webRTCManager) return std::string();
            return webRTCManager->createPeerConnectionFactory();
        }

        std::string WindowsWebRTCManager::createPeerConnection(const char* peerConnectionFactoryId)
        {
            if (!webRTCManager) return std::string();
            return webRTCManager->createPeerConnection(peerConnectionFactoryId);
        }

        std::string WindowsWebRTCManager::createVideoTrack(const char* peerConnectionId, WebRTCVideoCodec codec, WebRTCVideoPreference preference)
        {
            if (!webRTCManager) return std::string();
            return webRTCManager->createVideoTrack(peerConnectionId, codec, preference);
        }

        std::string WindowsWebRTCManager::createAudioTrack(const char* peerConnectionId)
        {
            if (!webRTCManager) return std::string();
            return webRTCManager->createAudioTrack(peerConnectionId);
        }

        std::string WindowsWebRTCManager::createDataChannel(const char* peerConnectionId, const char* label)
        {
            if (!webRTCManager) return std::string();
            return webRTCManager->createDataChannel(peerConnectionId, label);
        }

        void WindowsWebRTCManager::createOffer(const char* peerConnectionId)
        {
            if (webRTCManager) webRTCManager->createOffer(peerConnectionId);
        }

        void WindowsWebRTCManager::processAnswer(const char* peerConnectionId, const char* sdp)
        {
            if (webRTCManager) webRTCManager->processAnswer(peerConnectionId, sdp);
        }

        void WindowsWebRTCManager::processOffer(const char* peerConnectionId, const char* sdp)
        {
            if (webRTCManager) webRTCManager->processOffer(peerConnectionId, sdp);
        }

        void WindowsWebRTCManager::processIceCandidate(const char* peerConnectionId, const char* candidate, const char* mid, int mlineIndex)
        {
            if (webRTCManager) webRTCManager->processIceCandidate(peerConnectionId, candidate, mid, mlineIndex);
        }

        bool WindowsWebRTCManager::releasePeerConnection(const char* peerConnectionId)
        {
            if (!webRTCManager) return false;
            return webRTCManager->releaseSourcePeerConnection(peerConnectionId);
        }

        bool WindowsWebRTCManager::writeVideoFrame(std::string peerConnectionId, const char* videoTrackId, void* data, size_t size, int width, int height)
        {
            if (!webRTCManager) return false;
            return webRTCManager->writerVideoFrame(peerConnectionId,videoTrackId,
                static_cast<unsigned char*>(data), size, width, height);
        }

        bool WindowsWebRTCManager::writeAudioFrame(std::string peerConnectionId, const char* audioTrackId, void* data, size_t size)
        {
            if (!webRTCManager) return false;
            return webRTCManager->writerAudioFrame(peerConnectionId, audioTrackId,
                static_cast<unsigned char*>(data), size);
        }

        bool WindowsWebRTCManager::writeDataChannelData(std::string peerConnectionId, const char* dataChannelId, void* data, size_t size)
        {
            if (!webRTCManager) return false;
            return webRTCManager->writerDataChannelData(peerConnectionId, dataChannelId,
                static_cast<unsigned char*>(data), size);
        }

        void WindowsWebRTCManager::setAccountId(const char* accountId)
        {
            if (webRTCManager) webRTCManager->setAccountId(accountId);
        }

        std::string WindowsWebRTCManager::getAccountId() const
        {
            if (webRTCManager) return webRTCManager->getAccountId();
            return std::string();
        }

        void WindowsWebRTCManager::setTargetId(const char* targetId)
        {
            if (webRTCManager) webRTCManager->setTargetId(targetId);
        }

        std::string WindowsWebRTCManager::getTargetId() const
        {
            if (webRTCManager) return webRTCManager->getTargetId();
            return std::string();
        }

        void WindowsWebRTCManager::addStunServer(const char* host)
        {
            if (webRTCManager) webRTCManager->addStunServer(host);
        }

        void WindowsWebRTCManager::addTurnServer(const char* host, const char* username, const char* password)
        {
            if (webRTCManager) webRTCManager->addTurnServer(host, username, password);
        }

        void WindowsWebRTCManager::webrtcAsyncWrite(const char* json)
        {
            if (webRTCManager) webRTCManager->webrtcAsyncWrite(json);
        }

        // ========== 回调透传 ==========
        void WindowsWebRTCManager::setOnSignalServerConnectHandle(std::function<void()> handle)
        {
            if (webRTCManager) webRTCManager->onSignalServerConnectHandle = std::move(handle);
        }

        void WindowsWebRTCManager::setOnSignalServerDisConnectHandle(std::function<void()> handle)
        {
            if (webRTCManager) webRTCManager->onSignalServerDisConnectHandle = std::move(handle);
        }

        void WindowsWebRTCManager::setOnRemoteConnectHandle(std::function<void(std::string)> handle)
        {
            if (webRTCManager) webRTCManager->onRemoteConnectHandle = std::move(handle);
        }

        void WindowsWebRTCManager::setOnRemoteDisConnectHandle(std::function<void(std::string)> handle)
        {
            if (webRTCManager) webRTCManager->onRemoteDisConnectHandle = std::move(handle);
        }

        void WindowsWebRTCManager::setOnDataChannelDataHandle(std::function<void(std::string, std::string, const unsigned char*, size_t)> handle)
        {
            if (webRTCManager) webRTCManager->onDataChannelDataHandle = std::move(handle);
        }

        void WindowsWebRTCManager::setOnReceiveDataChannel(std::function<void(std::string, std::string)> handle)
        {
            if (webRTCManager) webRTCManager->onReceiveDataChannel = std::move(handle);
        }

        void WindowsWebRTCManager::setOnReceiveTrack(std::function<void(std::string, std::string, int)> handle)
        {
            if (webRTCManager) webRTCManager->onReceiveTrack = std::move(handle);
        }

        void WindowsWebRTCManager::setOnReceiveVideoFrameHandle(std::function<void(std::string, std::string, int, int, const uint8_t*, const uint8_t*, const uint8_t*, int, int, int)> handle)
        {
            if (webRTCManager) webRTCManager->onReceiveVideoFrameHandle = std::move(handle);
        }

        void WindowsWebRTCManager::setOnReceiveDataHandle(std::function<void(std::string)> handle)
        {
            if (webRTCManager) webRTCManager->onReceiveDataHandle = std::move(handle);
        }

        void WindowsWebRTCManager::setOnOfferHandle(std::function<void(std::string, std::string)> handle)
        {
            if (webRTCManager) webRTCManager->onOfferHandle = std::move(handle);
        }

        void WindowsWebRTCManager::setOnAnswerHandle(std::function<void(std::string, std::string)> handle)
        {
            if (webRTCManager) webRTCManager->onAnswerHandle = std::move(handle);
        }

        void WindowsWebRTCManager::setOnIceCandidateHandle(std::function<void(std::string, std::string, std::string, int)> handle)
        {
            if (webRTCManager) webRTCManager->onIceCandidateHandle = std::move(handle);
        }

    }
}