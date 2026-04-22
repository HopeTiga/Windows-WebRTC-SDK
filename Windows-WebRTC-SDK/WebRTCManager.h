#pragma once
#define WEBRTC_WIN 1
#define NOMINMAX 1
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4146)
#pragma warning(disable: 4996)
#endif

#include <thread>
#include <memory>
#include <queue>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <unordered_map>
#include <chrono>
#include <deque>
#include <functional>
#include <atomic>

#include <api/create_peerconnection_factory.h>
#include <api/scoped_refptr.h>
#include <api/task_queue/default_task_queue_factory.h>
#include <api/audio_codecs/builtin_audio_decoder_factory.h>
#include <api/audio_codecs/builtin_audio_encoder_factory.h>
#include <api/video_codecs/builtin_video_decoder_factory.h>
#include <api/video_codecs/builtin_video_encoder_factory.h>
#include <rtc_base/thread.h>
#include <rtc_base/ssl_adapter.h>
#include <rtc_base/ref_counted_object.h>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>
#include <boost/json.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/ssl.hpp>

#include "HWebRTC.h"
#include "PeerConnectionManager.h"

#include "Utils.h"
#include "AsioConcurrentQueue.h"


namespace hope {

    namespace rtc {

        class WebRTCManager :public std::enable_shared_from_this<WebRTCManager> {

            friend class PeerConnectionObserverImpl;

            friend class DataChannelObserverImpl;

			friend class PeerConnectionManager;

        public:

            WebRTCManager(boost::asio::io_context& ioContext);

            ~WebRTCManager();

            void Cleanup();

            void connect(std::string ip);

            void disConnect();

            void webrtcAsyncWrite(std::string json);

            void addStunServer(std::string host);

            void addTurnServer(std::string host, std::string username, std::string password);

            void setAccountId(std::string accountId);

            std::string getAccountId();

            void setTargetId(std::string targetId);

            std::string getTargetId();

        public:

            std::string createPeerConnectionFactory(bool isUseAudioModel = false,std::unique_ptr<webrtc::VideoEncoderFactory> videoEncoderFactory = nullptr
                , std::unique_ptr<webrtc::VideoDecoderFactory> videoDecoderFactory = nullptr
                , webrtc::scoped_refptr<webrtc::AudioEncoderFactory> audioEncoderFactory = nullptr
                , webrtc::scoped_refptr<webrtc::AudioDecoderFactory> audioDecoderFactory = nullptr);

            std::string createPeerConnection(std::string peerConnectionFactoryId);

			std::string createVideoTrack(std::string peerConnectionId, WebRTCVideoCodec codec, WebRTCVideoPreference preference);

			std::string createAudioTrack(std::string peerConnectionId);

			std::string createDataChannel(std::string peerConnectionId,std::string label);

			void createOffer(std::string peerConnectionId);

			void processAnswer(std::string peerConnectionId, const std::string& sdp);

			void processOffer(std::string peerConnectionId, const std::string& sdp);

            void processIceCandidate(std::string peerConnectionId, std::string candidate,std::string mid, int mlineIndex);

            bool releaseSourcePeerConnection(std::string peerConnectionId);

			bool writerVideoFrame(std::string peerConnectionId, std::string videoTrackId, unsigned char* data, size_t size, int width, int height);

			bool writerAudioFrame(std::string peerConnectionId, std::string audioTrackId, unsigned char* data, size_t size);

			bool writerDataChannelData(std::string peerConnectionId, std::string dataChannelId, unsigned char* data, size_t size);

        public:

            std::function<void()> onSignalServerConnectHandle;

            std::function<void()> onSignalServerDisConnectHandle;

            std::function<void(std::string,std::string , const unsigned char*, size_t) > onDataChannelDataHandle;

            std::function<void(std::string,std::string)> onReceiveDataChannel;

            std::function<void(std::string,std::string, int)> onReceiveTrack;

            std::function<void(std::string,std::string, int, int, const uint8_t*, const uint8_t*, const uint8_t*, int, int, int)> onReceiveVideoFrameHandle;

			std::function<void(std::string, std::string, const void*, int, int, size_t, size_t)> onReceiveAudioFrameHandle;

            std::function<void(std::string)> onReceiveDataHandle;

            std::function<void(std::string,std::string)> onOfferHandle;

			std::function<void(std::string,std::string)> onAnswerHandle;

			std::function<void(std::string,std::string,std::string, int)> onIceCandidateHandle;

            std::function<void(std::string, int)> onPeerConnectionStateChangeHandle;

			std::function<void(std::string, int)> onIceConnectionStateChangeHandle;

        private:

            void releaseSource();

            void closeWebSocket();

            void setTcpKeepAlive(boost::asio::ip::tcp::socket& sock, int idle = 0, int intvl = 3, int probes = 3);

            boost::asio::awaitable<void> webrtcReceiveCoroutine();

            boost::asio::awaitable<void> webrtcWriteCoroutine();

        private:

            std::string accountId;

            std::string targetId;

            webrtc::RtpEncodingParameters rtpEncodingParameters;

            std::unique_ptr<boost::beast::websocket::stream<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>> webSocket;

            boost::asio::ssl::context sslContext{ boost::asio::ssl::context::tlsv12_client };

            AsioConcurrentQueue<std::string> asioConcurrentQueue;

            std::atomic<bool> webrtcSignalSocketRuns{ false };

            std::unordered_map<std::string, std::unique_ptr<webrtc::Thread>> networkThreadMaps;

            std::unordered_map<std::string, std::unique_ptr<webrtc::Thread>> workerThreadMaps;

            std::unordered_map<std::string, std::unique_ptr<webrtc::Thread>> signalingThreadMaps;

            std::unordered_map<std::string, webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>> peerConnectionFactorys;

            std::unordered_map<std::string, std::shared_ptr<PeerConnectionManager>> peerConnectionManagers;

            std::vector<webrtc::PeerConnectionInterface::IceServer> iceServers;

            webrtc::scoped_refptr<AudioDeviceModuleImpl> audioDeviceModuleImpl;

            boost::asio::io_context& ioContext;

            std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> ioContextWorkPtr;

            std::thread ioContextThread;

            boost::asio::steady_timer steadyTimer;

            std::atomic<bool> isRemote{ false };

        };

    }

}