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



namespace hope {

    namespace rtc {

        WebRTCManager::WebRTCManager(boost::asio::io_context& ioContext)
            : connetState(WebRTCConnetState::none)
            , peerConnection(nullptr)
            , ioContext(ioContext)
            , steadyTimer(ioContext)
            , msquicSocketClient(nullptr)
        {

            ioContextWorkPtr = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
                boost::asio::make_work_guard(ioContext));

            ioContextThread = std::move(std::thread([this]() {
                this->ioContext.run();
                }));

            msquicSocketClient = new hope::quic::MsquicSocketClient(ioContext);

            msquicSocketClient->initialize();

        }

        void WebRTCManager::sendSignalingMessage(const boost::json::object& message) {

            if (!msquicSocketClient) return;

            boost::json::object fullMsg;
            fullMsg["requestType"] = static_cast<int64_t>(WebRTCRequestState::REQUEST);
            fullMsg["accountId"] = accountId;
            fullMsg["targetId"] = targetId;
            fullMsg["state"] = 200;

            for (auto& [key, value] : message) {
                fullMsg[key] = value;
            }

            msquicSocketClient->writeJsonAsync(fullMsg);

        }

        void WebRTCManager::processOffer(const std::string& sdp) {
            if (sdp.empty()) {
                LOG_ERROR("Received empty SDP offer");
 
                return;
            }

            webrtc::SdpParseError error;
            std::unique_ptr<webrtc::SessionDescriptionInterface> desc =
                webrtc::CreateSessionDescription(webrtc::SdpType::kOffer, sdp, &error);

            if (desc) {
                peerConnection->SetRemoteDescription(
                    SetRemoteDescriptionObserver::Create().get(), desc.release());

                webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
                createAnswerObserver = CreateAnswerObserverImpl::Create(this, peerConnection);
                peerConnection->CreateAnswer(createAnswerObserver.get(), options);
            }
            else {
                LOG_ERROR("Failed to parse offer: %s" , error.description.c_str());

            }
        }

        void WebRTCManager::processAnswer(const std::string& sdp) {
            if (sdp.empty()) {
                LOG_ERROR("Received empty SDP answer");
     
                return;
            }

            webrtc::SdpParseError error;
            std::unique_ptr<webrtc::SessionDescriptionInterface> desc =
                webrtc::CreateSessionDescription(webrtc::SdpType::kAnswer, sdp, &error);

            if (desc) {
                peerConnection->SetRemoteDescription(
                    SetRemoteDescriptionObserver::Create().get(), desc.release());
            }
            else {
                LOG_ERROR("Failed to parse answer: %s" , error.description.c_str());

            }
        }

        void WebRTCManager::processIceCandidate(const std::string& candidate,
            const std::string& mid, int lineIndex) {
            if (candidate.empty()) {
                return;
            }

            webrtc::SdpParseError error;
            std::unique_ptr<webrtc::IceCandidateInterface> iceCandidate(
                webrtc::CreateIceCandidate(mid, lineIndex, candidate, &error));

            if (iceCandidate) {
                peerConnection->AddIceCandidate(iceCandidate.release());
            }
            else {
                LOG_ERROR("Failed to parse ICE candidate: %s" ,error.description.c_str());
            }
        }

        int WebRTCManager::createVideoTrack(WebRTCVideoCodec codec, WebRTCVideoPreference preference,webrtc::RtpEncodingParameters rtpEncodingParameters)
        {
            if (!peerConnectionFactory) return -1;

            std::string videoTrackIdStr = "videoTrack" + std::to_string(videoTracks.size());

            webrtc::scoped_refptr<VideoTrackSourceImpl> videoTrackSourceImpl = webrtc::make_ref_counted<VideoTrackSourceImpl>();

            webrtc::scoped_refptr<webrtc::VideoTrackInterface> videoTrack = peerConnectionFactory->CreateVideoTrack(videoTrackSourceImpl, videoTrackIdStr);

            if (!videoTrack) {
                LOG_ERROR("Failed to create video track");
                return false;
            }

            std::vector<webrtc::RtpEncodingParameters> encodings;

            webrtc::RtpEncodingParameters encoding;

            encodings.push_back(rtpEncodingParameters);

            std::vector<std::string> videoStreamIds = { videoTrackIdStr };

            auto addTrackResult = peerConnection->AddTrack(videoTrack, videoStreamIds, encodings);

            if (!addTrackResult.ok()) {
                LOG_ERROR("Failed to add video track: %s", addTrackResult.error().message());
                return false;
            }

            webrtc::scoped_refptr<webrtc::RtpSenderInterface> videoSender = addTrackResult.MoveValue();

            // Configure H264 codec
            auto transceivers = peerConnection->GetTransceivers();

            for (auto& transceiver : transceivers) {
                if (transceiver->media_type() == webrtc::MediaType::MEDIA_TYPE_VIDEO) {

                    webrtc::RtpCapabilities senderCapabilities = peerConnectionFactory->GetRtpSenderCapabilities(
                        webrtc::MediaType::MEDIA_TYPE_VIDEO);

                    senderCapabilities.fec.clear();

                    if (senderCapabilities.codecs.empty()) {
                        continue;
                    }

                    std::vector<webrtc::RtpCodecCapability> preferredCodecs;

                    // 根据枚举选择优先编解码器
                    std::string priorityCodec;
                    switch (codec) {
                    case WebRTCVideoCodec::VP9: priorityCodec = "VP9"; break;
                    case WebRTCVideoCodec::H264: priorityCodec = "H264"; break;
                    case WebRTCVideoCodec::VP8: priorityCodec = "VP8"; break;
                    case WebRTCVideoCodec::H265: priorityCodec = "H265"; break;
                    case WebRTCVideoCodec::AV1: priorityCodec = "AV1"; break;
                    }

                    // 首先添加优先编解码器
                    bool foundPriorityCodec = false;
                    for (const auto& codec : senderCapabilities.codecs) {
                        if (codec.name == priorityCodec) {
                            preferredCodecs.push_back(codec);
                            foundPriorityCodec = true;
                            break;
                        }
                    }

                    // 添加其他可用编解码器（排除重复项和辅助编解码器）
                    for (const auto& codec : senderCapabilities.codecs) {
                        if (codec.name != priorityCodec) {
                            preferredCodecs.push_back(codec);
                        }
                    }

                    // 验证是否有编解码器可设置
                    if (preferredCodecs.empty()) {
                        LOG_ERROR("No valid codecs to set as preferences");
                        continue;
                    }

                    // 设置编解码器偏好
                    auto result = transceiver->SetCodecPreferences(preferredCodecs);
                    if (!result.ok()) {
                        LOG_ERROR("Failed to set codec preferences: %s", result.message());
                    }
                }
            }

            webrtc::RtpParameters parameters = videoSender->GetParameters();

            if (!parameters.encodings.empty()) {

                parameters.encodings[0] = encoding;

            }
            else {
                parameters.encodings = encodings;
            }

            parameters.degradation_preference = webrtc::DegradationPreference(static_cast<int>(preference));

            auto setParamsResult = videoSender->SetParameters(parameters);

            if (!setParamsResult.ok()) {
                LOG_ERROR("Failed to set RTP parameters: %s", setParamsResult.message());
                return -1;
            }

            int videoTrackId = videoTracks.size();

            videoSenders.emplace_back(std::move(videoSender));

            videoTrackSourceImpls.emplace_back(std::move(videoTrackSourceImpl));

			videoTracks.emplace_back(std::move(videoTrack));
            
            return videoTrackId;
        }

        int WebRTCManager::createAudioTrack()
        {
            if (!peerConnectionFactory) return -1;

            if (audioTracks.size() > 0) return -1;

			std::string audioTrackIdStr = "audioTrack" + std::to_string(audioTracks.size());

            webrtc::scoped_refptr<webrtc::AudioTrackInterface> audioTrack = peerConnectionFactory->CreateAudioTrack(audioTrackIdStr, nullptr);

            webrtc::RTCErrorOr<webrtc::scoped_refptr<webrtc::RtpSenderInterface>> audioTrackResult = peerConnection->AddTrack(audioTrack, { audioTrackIdStr });

            if (!audioTrackResult.ok()) {

                LOG_ERROR("Failed to add audio track: %s", audioTrackResult.error().message());

                return -1;

            }

            webrtc::scoped_refptr<webrtc::RtpSenderInterface> audioSender = audioTrackResult.MoveValue();

			int audioTrackId = audioTracks.size();

			audioSenders.emplace_back(std::move(audioSender));

			audioTracks.emplace_back(std::move(audioTrack));

            return audioTrackId;
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

        bool WebRTCManager::initializePeerConnection() {
            // Clean up any existing connection first
            if (peerConnection) {
                releaseSource();
            }

            webrtc::InitializeSSL();

            if (!peerConnectionFactory) {
                networkThread = webrtc::Thread::CreateWithSocketServer();
                if (!networkThread) {
                    LOG_ERROR("Failed to create network thread");
                    return false;
                }
                networkThread->SetName("network_thread", nullptr);
                if (!networkThread->Start()) {
                    LOG_ERROR("Failed to start network thread");
                    return false;
                }

                workerThread = webrtc::Thread::Create();
                if (!workerThread) {
                    LOG_ERROR("Failed to create worker thread");
                    return false;
                }
                workerThread->SetName("worker_thread", nullptr);
                if (!workerThread->Start()) {
                    LOG_ERROR("Failed to start worker thread");
                    return false;
                }

                signalingThread = webrtc::Thread::Create();
                if (!signalingThread) {
                    LOG_ERROR("Failed to create signaling thread");
                    return false;
                }
                signalingThread->SetName("signaling_thread", nullptr);
                if (!signalingThread->Start()) {
                    LOG_ERROR("Failed to start signaling thread");
                    return false;
                }

                audioDeviceModuleImpl = AudioDeviceModuleImpl::Create();

                peerConnectionFactory = webrtc::CreatePeerConnectionFactory(
                    networkThread.get(),
                    workerThread.get(),
                    signalingThread.get(),
                    audioDeviceModuleImpl,
                    webrtc::CreateBuiltinAudioEncoderFactory(),
                    webrtc::CreateBuiltinAudioDecoderFactory(),
                    webrtc::CreateBuiltinVideoEncoderFactory(),
                    webrtc::CreateBuiltinVideoDecoderFactory(),
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr
                );

                if (!peerConnectionFactory) {
                    LOG_ERROR("Failed to create PeerConnectionFactory");
                    return false;
                }
            }

            webrtc::PeerConnectionInterface::RTCConfiguration config;
            config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
            config.bundle_policy = webrtc::PeerConnectionInterface::kBundlePolicyMaxBundle;
            config.rtcp_mux_policy = webrtc::PeerConnectionInterface::kRtcpMuxPolicyRequire;
            config.continual_gathering_policy = webrtc::PeerConnectionInterface::GATHER_CONTINUALLY;

            config.ice_connection_receiving_timeout = 5000;        // 5秒无数据包则认为断开
            config.ice_unwritable_timeout = 5000;                  // 3秒无响应则标记为不可写
            config.ice_inactive_timeout = 5000;                    // 5秒后标记为非活跃

            config.servers = iceServers;

            peerConnectionObserver = std::make_unique<PeerConnectionObserverImpl>(this);

            webrtc::PeerConnectionDependencies pcDependencies(peerConnectionObserver.get());

            auto pcResult = peerConnectionFactory->CreatePeerConnectionOrError(config, std::move(pcDependencies));
            if (!pcResult.ok()) {
                LOG_ERROR("Failed to create PeerConnection: %s" , pcResult.error().message());
                return false;
            }

            peerConnection = pcResult.MoveValue();

            return true;
        }

        void WebRTCManager::handleRemoteDisconnect() {

            this->releaseSource();

            this->initializePeerConnection();

            if (this->onRemoteDisConnectHandle) {

                this->onRemoteDisConnectHandle();

            }

        }

        void WebRTCManager::writerAsync(std::string json)
        {

            if (msquicSocketClient) {

                try {

                    LOG_INFO("writerAsync:%s",json.c_str());

                    boost::json::object jsonObject = boost::json::parse(json).as_object();

                    msquicSocketClient->writeJsonAsync(jsonObject);
                }
                catch(std::exception & e){
                
                    LOG_INFO("ERROR:%s",e.what());

                }

            }

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


        int WebRTCManager::createDataChannel()
        {
            if (!peerConnection) return -1;

            std::unique_ptr<webrtc::DataChannelInit> dataChannelConfig = std::make_unique<webrtc::DataChannelInit>();

			std::string dataChannelIdStr = "dataChannel" + std::to_string(dataChannels.size());

            webrtc::scoped_refptr<webrtc::DataChannelInterface> dataChannel = peerConnection->CreateDataChannel(dataChannelIdStr, dataChannelConfig.get());

            if (!dataChannel) {

                LOG_ERROR("Failed to create data channel");

                return -1;

            }

            int dataChannelId = dataChannels.size();

            std::unique_ptr<DataChannelObserverImpl> dataChannelObserver = std::make_unique<DataChannelObserverImpl>(this, dataChannelId);

            dataChannel->RegisterObserver(dataChannelObserver.get());

			dataChannelObservers.emplace_back(std::move(dataChannelObserver));

            dataChannels.emplace_back(std::move(dataChannel));

            return dataChannelId;
        }

        void WebRTCManager::connect(std::string ip, std::string registerJson)
        {
            std::string host = ip;
            std::string port = "8080"; // 默认端口，你可以根据需要修改

            // 如果 IP 包含端口号（格式：ip:port）
            size_t colonPos = ip.find(':');
            if (colonPos != std::string::npos) {
                host = ip.substr(0, colonPos);
                port = ip.substr(colonPos + 1);
            }

            msquicSocketClient->setOnConnectionHandle([this, registerJson](bool isSuccess) {

                if (isSuccess) {

                    boost::json::object jsonObject = boost::json::parse(registerJson).as_object();

                    msquicSocketClient->writeJsonAsync(jsonObject);
                   
                }
                else {


                    if (onSignalServerConnectHandle) {

                        onSignalServerDisConnectHandle();

                    }

                    if (isRemote == false) {

                        return;

                    }

                    isRemote = false;

                    if (onRemoteDisConnectHandle) {

                        onRemoteDisConnectHandle();

                    }

                    releaseSource();

                    initializePeerConnection();

                }

                });

            msquicSocketClient->setOnDataReceivedHandle([this](boost::json::object& json) {

                if (json.contains("requestType")) {

                    int64_t requestType = json["requestType"].as_int64();

                    int64_t responseState = json["state"].as_int64();

                    switch (WebRTCRequestState(requestType))
                    {
                    case WebRTCRequestState::REGISTER :
                    {
                        if (responseState == 200) {

                            connetState = WebRTCConnetState::connect;

                            if (onSignalServerConnectHandle) {
                                onSignalServerConnectHandle();
                            }
                        }
                        break;
                    }

                    case WebRTCRequestState::REQUEST:
                    {
                        if (responseState == 200) {

                            if (json.contains("webRTCRemoteState")) {

                                WebRTCRemoteState remoteState = WebRTCRemoteState(json["webRTCRemoteState"].as_int64());

                                if (remoteState == WebRTCRemoteState::masterRemote) {

                                    if (json.contains("bitrate")) {

                                        rtpEncodingParameters.max_bitrate_bps = json["bitrate"].as_int64();

                                        rtpEncodingParameters.min_bitrate_bps = json["bitrate"].as_int64();

                                    }

                                    if (!initializePeerConnection()) {

                                        LOG_ERROR("Failed to initialize peer connection");

                                        return;
                                    }

                                    if (json.contains("accountId")) {
                                        targetId = std::string(json["accountId"].as_string().c_str());
                                    }

                                    webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;

                                    options.offer_to_receive_video = true;

                                    options.offer_to_receive_audio = true;

                                    createOfferObserver = CreateOfferObserverImpl::Create(this, peerConnection);

                                    peerConnection->CreateOffer(createOfferObserver.get(), options);

                                    boost::asio::co_spawn(ioContext, [this]()->boost::asio::awaitable<void> {

                                        steadyTimer.expires_after(std::chrono::seconds(10));

                                        co_await steadyTimer.async_wait(boost::asio::use_awaitable);

                                        LOG_INFO("isRemote:%s", isRemote.load() ? "Yes" : "No");

                                        if (!isRemote.load()) {

                                            releaseSource();

                                            initializePeerConnection();

                                            LOG_INFO("webrtc reInit");

                                        }

                                        }, boost::asio::detached);
                                }

                                return;

                            }

                            if (json.contains("type")) {

                                std::string type(json["type"].as_string().c_str());
                                
                                if (type == "offer") {

                                    if (!initializePeerConnection()) {

                                        LOG_ERROR("Failed to initialize peer connection");

                                        return;
                                    }

                                    std::string sdp(json["sdp"].as_string().c_str());

                                    processOffer(sdp);
                                }

                                else if (type == "answer") {
                                    std::string sdp(json["sdp"].as_string().c_str());
                                    processAnswer(sdp);
                                }
                                else if (type == "candidate") {
                                    std::string candidateStr(json["candidate"].as_string().c_str());
                                    std::string mid = json.contains("mid") ? std::string(json["mid"].as_string().c_str()) : "";
                                    int lineIndex = json.contains("mlineIndex") ? json["mlineIndex"].as_int64() : 0;

                                    if (peerConnection) {
                                        processIceCandidate(candidateStr, mid, lineIndex);
                                    }
                                }
                            }
                        }
                        break;
                    }

                    case WebRTCRequestState::RESTART:
                    { 

                        if (responseState == 200) {

                            if (msquicSocketClient && msquicSocketClient->isConnected()) {

                                releaseSource();

                                initializePeerConnection();

                                boost::json::object request;

                                request["requestType"] = static_cast<int64_t>(WebRTCRequestState::RESTART);

                                request["accountId"] = accountId;

                                request["targetId"] = targetId;

                                request["state"] = 200;

                                request["remoteState"] = "Ready";

                                msquicSocketClient->writeJsonAsync(request);

                            }
                        }
                        break;
                    }

                    case WebRTCRequestState::STOPREMOTE:
                    {
                        if (responseState == 200) {
                            if (this->isRemote) {
                                if (onRemoteDisConnectHandle) {
                                    onRemoteDisConnectHandle();
                                }
                                releaseSource();
                                initializePeerConnection();
                            }
                        }
                        break;
                    }

                    default:
                        // 可选：记录未知类型或忽略
                        break;
                    }
                }

                });

            msquicSocketClient->connect(host, std::stoi(port));

        }

        WebRTCManager::~WebRTCManager() {
            Cleanup();
        }


        void WebRTCManager::disConnect()
        {

            if (msquicSocketClient && msquicSocketClient->isConnected()) {

                msquicSocketClient->disconnect();
            }

            releaseSource();

            connetState = WebRTCConnetState::none;

            if (onSignalServerDisConnectHandle) {

                onSignalServerDisConnectHandle();

            }

        }

        void WebRTCManager::disConnectRemote()
        {
            releaseSource();

            initializePeerConnection();

            if (msquicSocketClient && msquicSocketClient->isConnected()) {
                boost::json::object message;
                message["accountId"] = this->accountId;
                message["targetId"] = this->targetId;
                message["requestType"] = static_cast<int64_t>(WebRTCRequestState::STOPREMOTE);

                msquicSocketClient->writeJsonAsync(message);
            }
        }

        bool WebRTCManager::writerVideoFrame(int videoTrackId,unsigned char* data, size_t size, int width, int height)
        {
            if ((videoTrackId < -1 && videoTrackId > videoTracks.size()) || !videoTracks[videoTrackId] || !data || size == 0) {
                return false;
            }

            size_t expectedSize = width * height * 3 / 2;
            if (size != expectedSize) {
                return false;
            }

            webrtc::scoped_refptr<webrtc::I420Buffer> i420Buffer =
                webrtc::I420Buffer::Create(width, height);

            if (!i420Buffer) {
                return false;
            }

            const int ySize = width * height;
            const int uvWidth = (width + 1) / 2;
            const int uvHeight = (height + 1) / 2;
            const int uvSize = uvWidth * uvHeight;

            memcpy(i420Buffer->MutableDataY(), data, ySize);
            memcpy(i420Buffer->MutableDataU(), data + ySize, uvSize);
            memcpy(i420Buffer->MutableDataV(), data + ySize + uvSize, uvSize);

            int64_t timestampUs = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();

            webrtc::VideoFrame frame = webrtc::VideoFrame::Builder()
                .set_video_frame_buffer(i420Buffer)
                .set_timestamp_us(timestampUs)
                .build();

            if (videoTrackSourceImpls[videoTrackId]) {

                videoTrackSourceImpls[videoTrackId]->PushFrame(frame);

                return true;

            }

            return false;

        }

        bool WebRTCManager::writerAudioFrame(int audioTrackId,unsigned char* data, size_t size)
        {

            if ((audioTrackId < 0 && audioTrackId > audioTracks.size()) || audioTracks[audioTrackId]) {

                return false;

            }

            if (audioDeviceModuleImpl) {
            
				audioDeviceModuleImpl->PushAudioData(data, size);

                return true;

            }

            return false;
        }

        bool WebRTCManager::writerDataChannelData(int dataChannelId, unsigned char* data, size_t size)
        {

            if ((dataChannelId < 0 && dataChannelId > dataChannels.size()) || dataChannels[dataChannelId]) {

                return false;

            }

            if (!dataChannels[dataChannelId]) {

                LOG_ERROR("dataChannel is null");

                delete[] data;

                return false;
            }

            if (!data) {

                LOG_INFO("data is nullptr");

                return false;

            }

            webrtc::DataBuffer buffer(webrtc::CopyOnWriteBuffer(data, size), true);

            dataChannels[dataChannelId]->SendAsync(buffer, [this](webrtc::RTCError error) {

                if (error.type() != webrtc::RTCErrorType::NONE){

                    LOG_ERROR("Failed to send data on DataChannel: %s", error.message());

                }

            });

            return true;

        }

        // Add releaseSource implementation
        void WebRTCManager::releaseSource() {

            for (auto& videoSink : videoTrackSinks) {
            
                if (videoSink) {

                    if (videoTracks[videoSink->videoTrackId]) {
                    
						videoTracks[videoSink->videoTrackId]->RemoveSink(videoSink.get());

                    }

                    videoSink = nullptr;

                }

            }

            if (peerConnection) {
                peerConnection->Close();
                peerConnection = nullptr;
            }

            if (peerConnectionObserver) {
                peerConnectionObserver.reset();
            }

            dataChannelObservers.clear();

            if (createOfferObserver) {
                createOfferObserver = nullptr;
            }

            if (createAnswerObserver) {
                createAnswerObserver = nullptr;
            }

            videoTracks.clear();

            videoSenders.clear();

            videoTrackSourceImpls.clear();

			dataChannels.clear();

            isRemote = false;
        }


        void WebRTCManager::Cleanup() {

            if (msquicSocketClient) {

                delete msquicSocketClient;

                msquicSocketClient = nullptr;

            }

            if (audioDeviceModuleImpl) {
            
                audioDeviceModuleImpl = nullptr;

            }

            ioContext.stop();

            if (ioContextWorkPtr) {
                ioContextWorkPtr.reset();
            }

            if (ioContextThread.joinable()) {
                ioContextThread.join();
            }

            releaseSource();
      
            // Reset factory
            if (peerConnectionFactory) {
                peerConnectionFactory = nullptr;
            }

            if (networkThread) {
                networkThread->Quit();
            }

            if (workerThread) {
                networkThread->Quit();
            }

            if (signalingThread) {
                networkThread->Quit();
            }


        }

    }

}