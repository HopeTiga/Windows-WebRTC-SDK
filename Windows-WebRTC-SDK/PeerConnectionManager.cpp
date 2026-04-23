#include "PeerConnectionManager.h"

#include <chrono>

#include <api/video/frame_buffer.h>
#include <api/video/i420_buffer.h>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "WebRTCManager.h"

namespace hope {
	namespace rtc {

        PeerConnectionManager::PeerConnectionManager(webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peerConnectionFactory, std::string peerConnectionId, WebRTCManager* webrtcManager)
            : peerConnectionFactory(peerConnectionFactory)
            , peerConnectionId(peerConnectionId)
			, webrtcManager(webrtcManager)
        {
        }

        PeerConnectionManager::~PeerConnectionManager()
        {
            releaseSource();
        }

        void PeerConnectionManager::releaseSource() {
            // 确保只执行一次清理
            if (isClosed.exchange(true)) return;

            LOG_INFO("PeerConnectionManager releasing resources for PC: %s", peerConnectionId.c_str());

            for (auto& pair : videoTracks) {
                auto& trackId = pair.first;
                auto& track = pair.second;
                if (track && videoTrackSinkMaps.find(trackId) != videoTrackSinkMaps.end() && videoTrackSinkMaps[trackId]) {
                    track->RemoveSink(videoTrackSinkMaps[trackId].get());
                }
            }

            for (auto& pair : audioTracks) {
                auto& trackId = pair.first;
                auto& track = pair.second;
                if (track && audioTrackSinkMaps.find(trackId) != audioTrackSinkMaps.end() && audioTrackSinkMaps[trackId]) {
                    track->RemoveSink(audioTrackSinkMaps[trackId].get());
                }
            }

            // 2. 关闭并解绑所有的 DataChannel
            for (auto& pair : dataChannels) {
                if (pair.second) {
                    // 注销 observer，防止在 Close 过程中触发 OnStateChange 等回调
                    pair.second->UnregisterObserver();
                    pair.second->Close();
                }
            }

            // 3. 关闭 PeerConnection
            if (peerConnection) {
                peerConnection->Close();
            }

            videoTrackSinkMaps.clear();
            videoTrackSourceImplMaps.clear();
            videoTracks.clear();

            audioTrackSinkMaps.clear();
            audioTrackSourceImplMaps.clear();
            audioTracks.clear();

            dataChannelObserverMaps.clear();
            dataChannels.clear();

            // 5. 释放核心的 RTC 对象和 Observer
            createOfferObserver = nullptr;
            createAnswerObserver = nullptr;
            peerConnectionObserver.reset(); // 释放 Unique_ptr

            peerConnection = nullptr;
            peerConnectionFactory = nullptr;

            // 断开与上层 Manager 的弱关联（视你的生命周期设计而定）
            webrtcManager = nullptr;

            LOG_INFO("PeerConnectionManager resources released.");
        }
		
        bool PeerConnectionManager::createPeerConnection()
        {

            if (!peerConnectionFactory) return false;

            webrtc::PeerConnectionInterface::RTCConfiguration config;

            config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;

            config.bundle_policy = webrtc::PeerConnectionInterface::kBundlePolicyMaxBundle;

            config.rtcp_mux_policy = webrtc::PeerConnectionInterface::kRtcpMuxPolicyRequire;

            config.tcp_candidate_policy = webrtc::PeerConnectionInterface::kTcpCandidatePolicyDisabled;

            config.ice_connection_receiving_timeout = 10000;        // 5秒无数据包则认为断开

            config.ice_unwritable_timeout = 10000;                  // 3秒无响应则标记为不可写

            config.ice_inactive_timeout = 10000;                    // 5秒后标记为非活跃

            config.servers = webrtcManager->iceServers;

            peerConnectionObserver = std::make_unique<PeerConnectionObserverImpl>(webrtcManager,this);

            webrtc::PeerConnectionDependencies pcDependencies(peerConnectionObserver.get());

            auto pcResult = peerConnectionFactory->CreatePeerConnectionOrError(config, std::move(pcDependencies));

            if (!pcResult.ok()) {

                LOG_ERROR("Failed to create PeerConnection: %s", pcResult.error().message());

                return false;

            }

            peerConnection = pcResult.MoveValue();

            return true;
        }

        std::string PeerConnectionManager::createDataChannel(std::string label)
        {
            if (!peerConnection) return std::string();

            std::unique_ptr<webrtc::DataChannelInit> dataChannelConfig = std::make_unique<webrtc::DataChannelInit>();

            webrtc::scoped_refptr<webrtc::DataChannelInterface> dataChannel = peerConnection->CreateDataChannel(label, dataChannelConfig.get());

            if (!dataChannel) {

                LOG_ERROR("Failed to create data channel");

                return std::string();

            }
            boost::uuids::random_generator gen;

            std::string dataChannelId = boost::uuids::to_string(gen());

            std::unique_ptr<DataChannelObserverImpl> dataChannelObserver = std::make_unique<DataChannelObserverImpl>(webrtcManager,this, dataChannelId);

            dataChannel->RegisterObserver(dataChannelObserver.get());

            dataChannelObserverMaps[dataChannelId] = (std::move(dataChannelObserver));

            dataChannels[dataChannelId] = std::move(dataChannel);

            return dataChannelId;
        }

        std::string PeerConnectionManager::createVideoTrack(std::string label,WebRTCVideoCodec codec, WebRTCVideoPreference preference)
        {

            if (!peerConnectionFactory || !peerConnection) return std::string();

            webrtc::scoped_refptr<VideoTrackSourceImpl> videoTrackSourceImpl = webrtc::make_ref_counted<VideoTrackSourceImpl>();

            webrtc::scoped_refptr<webrtc::VideoTrackInterface> videoTrack = peerConnectionFactory->CreateVideoTrack(videoTrackSourceImpl, label);

            if (!videoTrack) {
                LOG_ERROR("Failed to create video track");
                return std::string();
            }

            std::vector<std::string> videoStreamIds = { label };

            auto addTrackResult = peerConnection->AddTrack(videoTrack, videoStreamIds);

            if (!addTrackResult.ok()) {
                LOG_ERROR("Failed to add video track: %s", addTrackResult.error().message());
                return std::string();
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

            parameters.degradation_preference = webrtc::DegradationPreference(static_cast<int>(preference));

            auto setParamsResult = videoSender->SetParameters(parameters);

            if (!setParamsResult.ok()) {
                LOG_ERROR("Failed to set RTP parameters: %s", setParamsResult.message());
                return std::string();
            }

            boost::uuids::random_generator gen;

            std::string videoTrackId = boost::uuids::to_string(gen());

            videoTrackSourceImplMaps[videoTrackId] = std::move(videoTrackSourceImpl);

            videoTracks[videoTrackId] = std::move(videoTrack);

            return videoTrackId;
        }

        std::string PeerConnectionManager::createAudioTrack(std::string label)
        {
            if (!peerConnectionFactory || !peerConnection) return std::string();

			webrtc::scoped_refptr<AudioTrackSourceImpl> audioTrackSourceImpl = webrtc::make_ref_counted<AudioTrackSourceImpl>();

            webrtc::scoped_refptr<webrtc::AudioTrackInterface> audioTrack = peerConnectionFactory->CreateAudioTrack(label, audioTrackSourceImpl.get());

            webrtc::RTCErrorOr<webrtc::scoped_refptr<webrtc::RtpSenderInterface>> audioTrackResult = peerConnection->AddTrack(audioTrack, { label });

            if (!audioTrackResult.ok()) {

                LOG_ERROR("Failed to add audio track: %s", audioTrackResult.error().message());

                return std::string();

            }

            boost::uuids::random_generator gen;

            std::string audioTrackId = boost::uuids::to_string(gen());

			audioTrackSourceImplMaps[audioTrackId] = std::move(audioTrackSourceImpl);

            audioTracks[audioTrackId] = std::move(audioTrack);

            return audioTrackId;
        }

        bool PeerConnectionManager::writerVideoFrame(std::string videoTrackId, unsigned char* data, size_t size, int width, int height)
        {
            if (videoTracks.find(videoTrackId)== videoTracks.end() || !videoTracks[videoTrackId] || !data || size <= 0) {
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

            if (videoTrackSourceImplMaps[videoTrackId]) {

                videoTrackSourceImplMaps[videoTrackId]->PushFrame(frame);

                return true;

            }

            return false;

        }

        bool PeerConnectionManager::writerAudioFrame(std::string audioTrackId, unsigned char* audioData,
            int bitsPerSample,
            int sampleRate,
            size_t numberOfChannels,
            size_t numberOfFrames)
        {

            if (audioTracks.find(audioTrackId) == audioTracks.end() || !audioTracks[audioTrackId] || !audioData) {

                return false;

            }

            if (webrtcManager->audioDeviceModuleImpl) {

                webrtcManager->audioDeviceModuleImpl->PushPcmFrame(
                    reinterpret_cast<const int16_t*>(audioData),
                    numberOfFrames * numberOfChannels,
                    sampleRate,
                    static_cast<int>(numberOfChannels)
                );

                return true;
            }

            if (audioTrackSourceImplMaps[audioTrackId]) {
            
				audioTrackSourceImplMaps[audioTrackId]->onData(audioData, bitsPerSample, sampleRate, numberOfChannels, numberOfFrames);

            }

            return true;
        }

        bool PeerConnectionManager::writerDataChannelData(std::string dataChannelId, unsigned char* data, size_t size)
        {

            if (dataChannels.find(dataChannelId) == dataChannels.end() || !dataChannels[dataChannelId] || !data || size <= 0) {

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

                if (error.type() != webrtc::RTCErrorType::NONE) {

                    LOG_ERROR("Failed to send data on DataChannel: %s", error.message());

                }

                });

            return true;

        }

        bool PeerConnectionManager::createOffer()
        {
			webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;

			options.offer_to_receive_audio = true;

			options.offer_to_receive_video = true;

			createOfferObserver = CreateOfferObserverImpl::Create(webrtcManager, this);

			peerConnection->CreateOffer(createOfferObserver.get(), options);

            return true;
        }


        void PeerConnectionManager::processOffer(const std::string& sdp) {
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
                createAnswerObserver = CreateAnswerObserverImpl::Create(webrtcManager, this);
                peerConnection->CreateAnswer(createAnswerObserver.get(), options);
            }
            else {
                LOG_ERROR("Failed to parse offer: %s", error.description.c_str());

            }
        }

        void PeerConnectionManager::processAnswer(const std::string& sdp) {
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
                LOG_ERROR("Failed to parse answer: %s", error.description.c_str());

            }
        }

        void PeerConnectionManager::processIceCandidate(const std::string& candidate,
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
                LOG_ERROR("Failed to parse ICE candidate: %s", error.description.c_str());
            }
        }

	}
}