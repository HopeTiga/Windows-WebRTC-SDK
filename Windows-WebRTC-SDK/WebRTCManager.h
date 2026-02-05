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

#include <api/peer_connection_interface.h>
#include <api/create_peerconnection_factory.h>
#include <api/data_channel_interface.h>
#include <api/media_stream_interface.h>
#include <api/rtp_receiver_interface.h>
#include <api/rtp_transceiver_interface.h>
#include <api/jsep.h>
#include <api/rtc_error.h>
#include <api/scoped_refptr.h>
#include <api/rtc_event_log/rtc_event_log_factory.h>
#include <api/task_queue/default_task_queue_factory.h>
#include <api/audio_codecs/builtin_audio_decoder_factory.h>
#include <api/audio_codecs/builtin_audio_encoder_factory.h>
#include <api/video_codecs/builtin_video_decoder_factory.h>
#include <api/video_codecs/builtin_video_encoder_factory.h>
#include <api/audio/audio_mixer.h>
#include <api/audio/audio_processing.h>
#include <rtc_base/thread.h>
#include <rtc_base/ssl_adapter.h>
#include <rtc_base/ref_counted_object.h>
#include <pc/video_track_source.h>
#include <modules/video_capture/video_capture_factory.h>
#include <modules/audio_device/include/audio_device.h>
#include <api/enable_media_with_defaults.h>
#include <media/base/adapted_video_track_source.h>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>
#include <boost/json.hpp>

#include "PeerConnectionObserverImpl.h"
#include "VideoTrackSourceImpl.h"
#include "AudioDeviceModuleImpl.h"
#include "DataChannelObserverImpl.h"
#include "SetDescriptionObserverImpl.h"
#include "CreateDescriptionObserverImpl.h"
#include "videotracksinkimpl.h"

#include "MsquicSocketClient.h"
#include "LibWebRTC.h"
#include "Utils.h"
#include "concurrentqueue.h"


namespace hope {

    namespace rtc {


        class WebRTCManager :public std::enable_shared_from_this<WebRTCManager> {

            friend class PeerConnectionObserverImpl;

            friend class DataChannelObserverImpl;

        public:

            WebRTCManager(boost::asio::io_context& ioContext);

            ~WebRTCManager();

            void Cleanup();

            void sendSignalingMessage(const boost::json::object& message);

            void processOffer(const std::string& sdp);

            void processAnswer(const std::string& sdp);

            void processIceCandidate(const std::string& candidate, const std::string& mid, int lineIndex);

            int createVideoTrack(WebRTCVideoCodec codec, WebRTCVideoPreference preference = WebRTCVideoPreference::DISABLED, webrtc::RtpEncodingParameters rtpEncodingParameters = getDefaultRtpEncodingParameters());

			int createAudioTrack();

			int createDataChannel();

            void connect(std::string ip, std::string registerJson);

            void disConnect();

            void disConnectRemote();

            bool writerVideoFrame(int videoTrackId ,unsigned char* data, size_t size, int width, int height);

            bool writerAudioFrame(int audioTrackId,unsigned char* data, size_t size);

            bool writerDataChannelData(int dataChannelId,unsigned char* data, size_t size);

            void writerAsync(std::string json);

            void handleRemoteDisconnect();

            void addStunServer(std::string host);

            void addTurnServer(std::string host, std::string username, std::string password);

            void setAccountId(std::string accountId);

            std::string getAccountId();

            void setTargetId(std::string targetId);

            std::string getTargetId();

        public:

            std::function<void()> onSignalServerConnectHandle;

            std::function<void()> onSignalServerDisConnectHandle;

            std::function<void()> onRemoteConnectHandle;

            std::function<void()> onRemoteDisConnectHandle;

            std::function<void(const unsigned char*, size_t, int)> onDataChannelDataHandle;

            std::function<void()> onReInitHandle;

            std::function<void(int)> onReceiveDataChannel;

            std::function<void(int,int)> onReceiveTrack;

			std::function<void(int,int, int,const uint8_t *, const uint8_t*, const uint8_t*, int, int, int)> onReceiveVideoFrameHandle;

            std::function<void()> onCreateOfferBeforeHandle;

            std::function<void()> onReceiveOfferBeforeHandle;

        private:

            bool initializePeerConnection();

            void releaseSource();

            static webrtc::RtpEncodingParameters getDefaultRtpEncodingParameters() {

                webrtc::RtpEncodingParameters encoding;
                encoding.active = true;
                encoding.max_bitrate_bps = 1000000;  // 4 Mbps
                encoding.min_bitrate_bps = 1000000;  // 1 Mbps
                encoding.bitrate_priority = 4.0;
                encoding.max_framerate = 60;
                encoding.scale_resolution_down_by = 1.0;
                encoding.scalability_mode = "L1T1";
                encoding.network_priority = webrtc::Priority::kHigh;
                return encoding;
            }


        private:

            std::string accountId;

            std::string targetId;

            webrtc::RtpEncodingParameters rtpEncodingParameters;

            hope::quic::MsquicSocketClient* msquicSocketClient;

            std::unique_ptr<webrtc::Thread> networkThread;

            std::unique_ptr<webrtc::Thread> workerThread;

            std::unique_ptr<webrtc::Thread> signalingThread;

            webrtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection;

            webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peerConnectionFactory;

            std::vector<webrtc::scoped_refptr<webrtc::DataChannelInterface>> dataChannels;

            std::vector<webrtc::scoped_refptr<webrtc::VideoTrackInterface>> videoTracks;

            std::vector<webrtc::scoped_refptr<webrtc::RtpSenderInterface>> videoSenders;

            std::vector<webrtc::scoped_refptr<webrtc::AudioTrackInterface>> audioTracks;

            std::vector<webrtc::scoped_refptr<webrtc::RtpSenderInterface>> audioSenders;

            std::unique_ptr<PeerConnectionObserverImpl> peerConnectionObserver;

            std::vector<std::unique_ptr<DataChannelObserverImpl>> dataChannelObservers;

            webrtc::scoped_refptr<CreateOfferObserverImpl> createOfferObserver;

            webrtc::scoped_refptr<CreateAnswerObserverImpl> createAnswerObserver;

            std::vector<webrtc::scoped_refptr<VideoTrackSourceImpl>> videoTrackSourceImpls;

            webrtc::scoped_refptr<AudioDeviceModuleImpl> audioDeviceModuleImpl;

			std::vector<std::unique_ptr<VideoTrackSinkImpl>> videoTrackSinks;

            std::vector<webrtc::PeerConnectionInterface::IceServer> iceServers;

            std::atomic<WebRTCConnetState> connetState;

            boost::asio::io_context& ioContext;

            std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> ioContextWorkPtr;

            std::thread ioContextThread;

            boost::asio::steady_timer steadyTimer;

            std::atomic<bool> isRemote{ false };

        };

    }

}