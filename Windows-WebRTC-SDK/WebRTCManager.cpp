#include "WebRTCManager.h"
#include <boost/locale.hpp>
#include <string>
#include <stdexcept>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/random/random_device.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <api/video/i420_buffer.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#pragma comment(lib, "ws2_32.lib")
#elif defined(__linux__)
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#endif


namespace hope {

    namespace rtc {

        static constexpr std::chrono::seconds RESOLVE_TIMEOUT{ 5 };

        static constexpr std::chrono::seconds CONNECT_TIMEOUT{ 10 };

        static constexpr std::chrono::seconds SSL_HANDSHAKE_TIMEOUT{ 10 };

        static constexpr std::chrono::seconds WS_HANDSHAKE_TIMEOUT{ 10 };


        WebRTCManager::WebRTCManager(boost::asio::io_context& ioContext)
            : ioContext(ioContext)
            , steadyTimer(ioContext)
            , asioConcurrentQueue(ioContext.get_executor())
        {

            ioContextWorkPtr = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
                boost::asio::make_work_guard(ioContext));

            ioContextThread = std::move(std::thread([this]() {
                this->ioContext.run();
                }));

            sslContext.set_options(
                boost::asio::ssl::context::default_workarounds |
                boost::asio::ssl::context::no_sslv2 |
                boost::asio::ssl::context::single_dh_use
            );

        }

        void WebRTCManager::webrtcAsyncWrite(std::string str)
        {
            asioConcurrentQueue.enqueue(std::move(str));
        }

        void WebRTCManager::closeWebSocket()
        {
            boost::system::error_code ec;

            asioConcurrentQueue.close();

            // 取消底层 TCP socket
            if (webSocket) {
                auto& tcpSocket = webSocket->next_layer().next_layer();
                tcpSocket.cancel(ec);
                if (ec) {
                    LOG_ERROR("WebRTCManager::closeSocket() can't cancel Socket: %s", ec.message().c_str());
                }
                // WebSocket 关闭帧
                if (webSocket->is_open()) {
                    try {
                        webSocket->close(boost::beast::websocket::close_code::normal, ec);
                    }
                    catch (const std::exception& e) {
                        LOG_ERROR("WebRTCManager::closeSocket() close WebSocket failed: %s", e.what());
                    }
                }

                // SSL 关闭
                if (webSocket->next_layer().next_layer().is_open()) {
                    webSocket->next_layer().shutdown(ec);
                    webSocket->next_layer().next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
                    webSocket->next_layer().next_layer().close(ec);
                    if (ec && ec != boost::asio::error::not_connected) {
                        LOG_ERROR("WebRTCManager::closeSocket() close Tcp Socket failed: %s", ec.message().c_str());
                    }
                }
            }

            webrtcSignalSocketRuns.store(false);

            LOG_INFO("WebRTCManager::WebSocket is close");
        }

        void WebRTCManager::setTcpKeepAlive(boost::asio::ip::tcp::socket& sock, int idle, int intvl, int probes)
        {
            int fd = sock.native_handle();

            /* 1. 先打开 SO_KEEPALIVE 通用开关 */
            int on = 1;
            setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE,
                reinterpret_cast<const char*>(&on), sizeof(on));

#if defined(__linux__)
            setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
            setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl));
            setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &probes, sizeof(probes));

#elif defined(_WIN32)
            /* Windows 用毫秒结构体 */
            struct tcp_keepalive kalive {};
            kalive.onoff = 1;
            kalive.keepalivetime = idle * 1000;   // ms
            kalive.keepaliveinterval = intvl * 1000;   // ms
            DWORD bytes_returned = 0;
            WSAIoctl(fd, SIO_KEEPALIVE_VALS,
                &kalive, sizeof(kalive),
                nullptr, 0, &bytes_returned, nullptr, nullptr);

#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
            /* macOS / BSD 用秒级 TCP_KEEPALIVE 等选项 */
            setsockopt(fd, IPPROTO_TCP, TCP_KEEPALIVE, &idle, sizeof(idle));   // 同 Linux 的 IDLE
            /* 间隔与次数在 BSD 上只有一个 TCP_KEEPINTVL，单位秒 */
            setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl));
            /* BSD 没有 KEEPCNT，用 TCP_KEEPALIVE 的初始值+间隔推算，效果相近 */
#else
#warning "Unsupported platform, TCP keep-alive parameters not tuned"
#endif
        }

        boost::asio::awaitable<void> WebRTCManager::webrtcReceiveCoroutine()
        {
            webrtcSignalSocketRuns.store(true);

            try {

                while (webrtcSignalSocketRuns.load()) {

                    boost::beast::flat_buffer buffer;

                    co_await webSocket->async_read(buffer, boost::asio::use_awaitable);

                    std::string str = boost::beast::buffers_to_string(buffer.data());

                    buffer.consume(buffer.size());

                    if (onReceiveDataHandle) {

                        onReceiveDataHandle(str);

                        continue;
                    }

                }

            }
            catch (std::exception& e) {

                LOG_ERROR("WebSocket Connect Error : %s", e.what());

                if (onSignalServerDisConnectHandle) {
                    onSignalServerDisConnectHandle();
                }

                releaseSource();

                co_return;

            }

        }

        boost::asio::awaitable<void> WebRTCManager::webrtcWriteCoroutine()
        {
            try {

                while (webrtcSignalSocketRuns.load()) {

                    std::optional<std::string> optional = co_await asioConcurrentQueue.dequeue();

                    if (optional.has_value()) {

                        std::string str = optional.value();

                        co_await webSocket->async_write(boost::asio::buffer(str), boost::asio::use_awaitable);

                    }
                    else {

                        break;
                    }

                    if (!webrtcSignalSocketRuns.load()) break;

                }

            }
            catch (const std::exception& e) {

                LOG_ERROR("Writer coroutine unhandled exception: %s", e.what());

            }
            catch (...) {

                LOG_ERROR("Writer coroutine unknown exception");

            }
            co_return;
        }

        std::string WebRTCManager::createPeerConnectionFactory(std::unique_ptr<webrtc::VideoEncoderFactory> videoEncoderFactory, std::unique_ptr<webrtc::VideoDecoderFactory> videoDecoderFactory, webrtc::scoped_refptr<webrtc::AudioEncoderFactory> audioEncoderFactory, webrtc::scoped_refptr<webrtc::AudioDecoderFactory> audioDecoderFactory)
        {
            webrtc::InitializeSSL();

            std::unique_ptr<webrtc::Thread> networkThread = webrtc::Thread::CreateWithSocketServer();
            if (!networkThread) {
                LOG_ERROR("Failed to create network thread");
                return std::string();
            }

            networkThread->SetName("network_thread", nullptr);
            if (!networkThread->Start()) {
                LOG_ERROR("Failed to start network thread");
                return std::string();
            }

            std::unique_ptr<webrtc::Thread>  workerThread = webrtc::Thread::Create();
            if (!workerThread) {
                LOG_ERROR("Failed to create worker thread");
                return std::string();
            }
            workerThread->SetName("worker_thread", nullptr);
            if (!workerThread->Start()) {
                LOG_ERROR("Failed to start worker thread");
                return std::string();
            }

            std::unique_ptr<webrtc::Thread> signalingThread = webrtc::Thread::Create();
            if (!signalingThread) {
                LOG_ERROR("Failed to create signaling thread");
                return std::string();
            }
            signalingThread->SetName("signaling_thread", nullptr);
            if (!signalingThread->Start()) {
                LOG_ERROR("Failed to start signaling thread");
                return std::string();
            }

            if (!audioDeviceModuleImpl) {
            
                audioDeviceModuleImpl = AudioDeviceModuleImpl::Create();

            }

            if (videoEncoderFactory == nullptr) videoEncoderFactory = webrtc::CreateBuiltinVideoEncoderFactory();

            if (videoDecoderFactory == nullptr) videoDecoderFactory = webrtc::CreateBuiltinVideoDecoderFactory();

            if (audioEncoderFactory == nullptr) audioEncoderFactory = webrtc::CreateBuiltinAudioEncoderFactory();

            if (audioDecoderFactory == nullptr) audioDecoderFactory = webrtc::CreateBuiltinAudioDecoderFactory();

            webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peerConnectionFactory = webrtc::CreatePeerConnectionFactory(
                networkThread.get(),
                workerThread.get(),
                signalingThread.get(),
                audioDeviceModuleImpl,
                audioEncoderFactory,
                audioDecoderFactory,
                std::move(videoEncoderFactory),
                std::move(videoDecoderFactory),
                nullptr,
                nullptr,
                nullptr,
                nullptr
            );

            if (!peerConnectionFactory) {
                LOG_ERROR("Failed to create PeerConnectionFactory");
                return std::string();
            }

            boost::uuids::random_generator gen;

            std::string peerConnectionFactoryId = boost::uuids::to_string(gen());

			peerConnectionFactorys[peerConnectionFactoryId] = peerConnectionFactory;

			networkThreadMaps[peerConnectionFactoryId] = std::move(networkThread);

			workerThreadMaps[peerConnectionFactoryId] = std::move(workerThread);

			signalingThreadMaps[peerConnectionFactoryId] = std::move(signalingThread);
            
            return peerConnectionFactoryId;
        }

        std::string WebRTCManager::createPeerConnection(std::string peerConnectionFactoryId)
        {

            if (peerConnectionFactorys.find(peerConnectionFactoryId)== peerConnectionFactorys.end() || !peerConnectionFactorys[peerConnectionFactoryId]) return std::string();

            webrtc::PeerConnectionInterface::RTCConfiguration config;
            config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
            config.bundle_policy = webrtc::PeerConnectionInterface::kBundlePolicyMaxBundle;
            config.rtcp_mux_policy = webrtc::PeerConnectionInterface::kRtcpMuxPolicyRequire;
            config.continual_gathering_policy = webrtc::PeerConnectionInterface::GATHER_CONTINUALLY;

            config.ice_connection_receiving_timeout = 15000;        // 5秒无数据包则认为断开
            config.ice_unwritable_timeout = 15000;                  // 3秒无响应则标记为不可写
            config.ice_inactive_timeout = 15000;                    // 5秒后标记为非活跃

            config.servers = iceServers;

            boost::uuids::random_generator gen;

            std::string peerConnectionId = boost::uuids::to_string(gen());

            std::shared_ptr<PeerConnectionManager> peerConnectionManager = std::make_shared<PeerConnectionManager>(peerConnectionFactorys[peerConnectionFactoryId], peerConnectionId, this);

            if (!peerConnectionManager->createPeerConnection()) return std::string();

			peerConnectionManagers[peerConnectionId] = peerConnectionManager;

            return peerConnectionId;
        }

        std::string WebRTCManager::createVideoTrack(std::string peerConnectionId, WebRTCVideoCodec codec, WebRTCVideoPreference preference)
        {
            if (peerConnectionManagers.find(peerConnectionId) == peerConnectionManagers.end() || !peerConnectionManagers[peerConnectionId]) return std::string();

			return peerConnectionManagers[peerConnectionId]->createVideoTrack(codec, preference);
        }

        std::string WebRTCManager::createAudioTrack(std::string peerConnectionId)
        {
            if (peerConnectionManagers.find(peerConnectionId) == peerConnectionManagers.end() || !peerConnectionManagers[peerConnectionId]) return std::string();
            
			return peerConnectionManagers[peerConnectionId]->createAudioTrack();
        }

        std::string WebRTCManager::createDataChannel(std::string peerConnectionId, std::string label)
        {
            if (peerConnectionManagers.find(peerConnectionId) == peerConnectionManagers.end() || !peerConnectionManagers[peerConnectionId]) return std::string();

			return peerConnectionManagers[peerConnectionId]->createDataChannel(label);
        }

        void WebRTCManager::createOffer(std::string peerConnectionId)
        {
			if (peerConnectionManagers.find(peerConnectionId) == peerConnectionManagers.end() || !peerConnectionManagers[peerConnectionId]) return;
            peerConnectionManagers[peerConnectionId]->createOffer();
        }

        void WebRTCManager::processAnswer(std::string peerConnectionId, const std::string& sdp)
        {
            if (peerConnectionManagers.find(peerConnectionId) == peerConnectionManagers.end() || !peerConnectionManagers[peerConnectionId]) return;

            peerConnectionManagers[peerConnectionId]->processAnswer(sdp);
        }

        void WebRTCManager::processOffer(std::string peerConnectionId, const std::string& sdp)
        {
            if (peerConnectionManagers.find(peerConnectionId) == peerConnectionManagers.end() || !peerConnectionManagers[peerConnectionId]) return;

            peerConnectionManagers[peerConnectionId]->processOffer(sdp);
        }

        void WebRTCManager::processIceCandidate(std::string peerConnectionId,std::string candidate, std::string mid, int mlineIndex)
        {
            if (peerConnectionManagers.find(peerConnectionId) == peerConnectionManagers.end() || !peerConnectionManagers[peerConnectionId]) return;

            peerConnectionManagers[peerConnectionId]->processIceCandidate(candidate,mid, mlineIndex);
        }

        bool WebRTCManager::releaseSourcePeerConnection(std::string peerConnectionId)
        {
            if (peerConnectionManagers.find(peerConnectionId) == peerConnectionManagers.end() || !peerConnectionManagers[peerConnectionId]) return false;

			peerConnectionManagers[peerConnectionId]->releaseSource();

            return true;
            
        }

        bool WebRTCManager::writerVideoFrame(std::string peerConnectionId, std::string videoTrackId, unsigned char* data, size_t size, int width, int height)
        {
            if (peerConnectionManagers.find(peerConnectionId) == peerConnectionManagers.end() || !peerConnectionManagers[peerConnectionId]) return false;

            return peerConnectionManagers[peerConnectionId]->writerVideoFrame(videoTrackId,data,size,width,height);
        }

        bool WebRTCManager::writerAudioFrame(std::string peerConnectionId, std::string audioTrackId, unsigned char* data, size_t size)
        {
            if (peerConnectionManagers.find(peerConnectionId) == peerConnectionManagers.end() || !peerConnectionManagers[peerConnectionId]) return false;

            return peerConnectionManagers[peerConnectionId]->writerAudioFrame(audioTrackId, data, size);
        }

        bool WebRTCManager::writerDataChannelData(std::string peerConnectionId, std::string dataChannelId, unsigned char* data, size_t size)
        {
            if (peerConnectionManagers.find(peerConnectionId) == peerConnectionManagers.end() || !peerConnectionManagers[peerConnectionId]) return false;

            return peerConnectionManagers[peerConnectionId]->writerDataChannelData(dataChannelId, data, size);
        }

        void WebRTCManager::setAccountId(std::string accountId)
        {
            this->accountId = accountId;
        }

        std::string WebRTCManager::getAccountId()
        {
            return this->accountId;
        }

        void WebRTCManager::setTargetId(std::string targetId)
        {
            this->targetId = targetId;
        }

        std::string WebRTCManager::getTargetId()
        {
            return this->targetId;
        }

        void WebRTCManager::addStunServer(std::string host)
        {
            webrtc::PeerConnectionInterface::IceServer stunServer;
            stunServer.uri = host;
            iceServers.push_back(stunServer);
        }

        void WebRTCManager::addTurnServer(std::string host, std::string username, std::string password)
        {
            webrtc::PeerConnectionInterface::IceServer turnServer;
            turnServer.uri = host;
            turnServer.username = username;
            turnServer.password = password;
            iceServers.emplace_back(turnServer);
        }


        void WebRTCManager::connect(std::string ip)
        {

            std::string host = ip;
            std::string port = "443"; // 默认端口，你可以根据需要修改

            // 如果 IP 包含端口号（格式：ip:port）
            size_t colonPos = ip.find(':');
            if (colonPos != std::string::npos) {
                host = ip.substr(0, colonPos);
                port = ip.substr(colonPos + 1);
            }

            boost::asio::co_spawn(ioContext, [this, host, port]()->boost::asio::awaitable<void> {

                try {

                    if (webSocket) {

                        closeWebSocket();

                        webSocket = nullptr;

                    }

                    webSocket = std::make_unique<boost::beast::websocket::stream<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>>(ioContext, sslContext);

                    boost::asio::ip::tcp::resolver resolver(ioContext);

                                std::string type(json["type"].as_string().c_str());
                                
                                if (type == "offer") {

                                    if (!initializePeerConnection()) {

                                        LOG_ERROR("Failed to initialize peer connection");

                    asioConcurrentQueue.reset();

                    if (onSignalServerConnectHandle) {
                    
                        onSignalServerConnectHandle();

                    }

                    setTcpKeepAlive(webSocket->next_layer().next_layer());

                    boost::asio::co_spawn(ioContext, webrtcReceiveCoroutine(), boost::asio::detached);

                    boost::asio::co_spawn(ioContext, webrtcWriteCoroutine(), boost::asio::detached);

                }
                catch (std::exception& e) {

                    LOG_ERROR("WebSocket Connect Error : %s", e.what());

                    if (onSignalServerDisConnectHandle) {
                        onSignalServerDisConnectHandle();
                    }

                    releaseSource();

                    co_return;
                }

                }, boost::asio::detached);

        }

        WebRTCManager::~WebRTCManager() {
            Cleanup();
        }


        void WebRTCManager::disConnect()
        {

            closeWebSocket();

            releaseSource();

            if (onSignalServerDisConnectHandle) {

                onSignalServerDisConnectHandle();

            }

        }

        // Add releaseSource implementation
        void WebRTCManager::releaseSource() {
            LOG_INFO("WebRTCManager releasing sources...");

            isRemote = false;

            // 1. 关闭信令发送队列，打断正在等待的协程
            asioConcurrentQueue.close();

            // 2. 遍历并安全释放所有的 PeerConnection 资源
            for (auto& pair : peerConnectionManagers) {
                if (pair.second) {
                    // 调用我们刚刚写好的 PeerConnectionManager::releaseSource
                    pair.second->releaseSource();
                }
            }
            // 清空容器，释放 shared_ptr 引用计数
            peerConnectionManagers.clear();

            LOG_INFO("WebRTCManager sources released.");
        }


        void WebRTCManager::Cleanup() {
            LOG_INFO("WebRTCManager starting cleanup...");

            closeWebSocket();

            releaseSource();

            peerConnectionFactorys.clear();

            // 4. 释放全局的 AudioDeviceModule
            // ADM 必须在 Factory 销毁后才能安全释放
            if (audioDeviceModuleImpl) {
                audioDeviceModuleImpl = nullptr;
            }

            for (auto& [id, signalThread] : signalingThreadMaps) {
                if (signalThread) {
                    signalThread->Quit();
                }
            }

            // 2. 退出工作线程
            for (auto& [id, workerThread] : workerThreadMaps) {
                if (workerThread) {
                    workerThread->Quit();
                }
            }

            // 3. 退出网络线程
            for (auto& [id, networkThread] : networkThreadMaps) {
                if (networkThread) {
                    networkThread->Quit();
                }
            }

            signalingThreadMaps.clear();
            workerThreadMaps.clear();
            networkThreadMaps.clear();

            // 6. 停止并回收 Boost.Asio 的 io_context 资源
            if (ioContextWorkPtr) {
                ioContextWorkPtr.reset();
            }

            if (!ioContext.stopped()) {
                ioContext.stop();
            }

            // 等待 Asio 事件循环线程安全退出
            if (ioContextThread.joinable()) {
                ioContextThread.join();
            }

            LOG_INFO("WebRTCManager cleanup finished.");
        }

    }

}