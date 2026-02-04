#include "PeerConnectionObserverImpl.h"

#include <boost/json.hpp>

#include "WebRTCManager.h"
#include "videotracksinkimpl.h"

#include "LibWebRTC.h"

#include "Utils.h"

namespace hope {

	namespace rtc {
	
        // Observer实现
        void PeerConnectionObserverImpl::OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState newState) {
            switch (newState) {
            case webrtc::PeerConnectionInterface::kClosed:
                break;
            default:
                break;
            }
        }
        void PeerConnectionObserverImpl::OnDataChannel(webrtc::scoped_refptr<webrtc::DataChannelInterface> dataChannel) {

            if (!manager) return;

            int dataChannelId = manager->dataChannels.size();

            if (!dataChannel) {
                LOG_ERROR("DataChannel is null");
                return;
			}

			manager->dataChannels.emplace_back(dataChannel);

            manager->dataChannelObservers.emplace_back(
				std::make_unique<DataChannelObserverImpl>(manager, dataChannelId));

            if (manager->onReceiveDataChannel) {
            
				manager->onReceiveDataChannel(dataChannelId);

            }

        }

        void PeerConnectionObserverImpl::OnTrack(webrtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) {

            if (!transceiver) {
                LOG_ERROR("RtpTransceiverInterface is null");
                return;
			}

            auto receiver = transceiver->receiver();

            auto track = receiver->track();

            if (track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind) {

                LOG_INFO("Video track received");

                int videoTrackId = manager->videoTracks.size();

                receiver->SetJitterBufferMinimumDelay(std::optional<double>(0.00));

                manager->videoTracks.emplace_back(webrtc::scoped_refptr<webrtc::VideoTrackInterface>(
                    static_cast<webrtc::VideoTrackInterface*>(track.release())

                ));

                std::unique_ptr<VideoTrackSinkImpl> videoSinkImpl = std::make_unique<VideoTrackSinkImpl>(manager,videoTrackId);

                manager->videoTracks[videoTrackId]->AddOrUpdateSink(videoSinkImpl.get(), webrtc::VideoSinkWants());

                manager->videoTrackSinks.emplace_back(std::move(videoSinkImpl));

                manager->videoSenders.emplace_back(transceiver->sender());

                if (manager->onReceiveTrack) {
                
                    manager->onReceiveTrack(videoTrackId,static_cast<int>(WebRTCTrackType::video));

                }

                return;
            }

            if (track->kind() == webrtc::MediaStreamTrackInterface::kAudioKind) {

                LOG_INFO("Video track received");

                receiver->SetJitterBufferMinimumDelay(std::optional<double>(0.00));

				int audioTrackId = manager->audioTracks.size();

                manager->audioTracks.emplace_back(webrtc::scoped_refptr<webrtc::AudioTrackInterface>(
                    static_cast<webrtc::AudioTrackInterface*>(track.release())
                ));

                manager->audioSenders.emplace_back(transceiver->sender());

                if (manager->onReceiveTrack) {

                    manager->onReceiveTrack(audioTrackId, static_cast<int>(WebRTCTrackType::audio));

                }

                return;
            }
        
        }

        void PeerConnectionObserverImpl::OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState newState) {
            if (newState == webrtc::PeerConnectionInterface::kIceGatheringComplete) {
            }
        }
        void PeerConnectionObserverImpl::OnIceCandidate(const webrtc::IceCandidateInterface* candidate) {
            if (!candidate) {
                LOG_ERROR("OnIceCandidate called with null candidate");
                return;
            }
            std::string sdp;
            if (!candidate->ToString(&sdp)) {
                LOG_ERROR("Failed to convert ICE candidate to string");
                return;
            }

            LOG_INFO("candidate mlineIndex: %d", candidate->sdp_mline_index());

            boost::json::object msg;
            msg["type"] = "candidate";
            msg["candidate"] = sdp;
            msg["mid"] = candidate->sdp_mid();
            msg["mlineIndex"] = candidate->sdp_mline_index();
            manager->sendSignalingMessage(msg);
        }
        void PeerConnectionObserverImpl::OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState newState) {
            switch (newState) {
            case webrtc::PeerConnectionInterface::kIceConnectionConnected: {
            
                handle = webrtc::make_ref_counted<hope::rtc::RTCStatsCollectorHandle>();

				manager->peerConnection->GetStats(handle.get());

                try {
                    manager->steadyTimer.cancel();
                    LOG_INFO("Connection successful, timeout timer cancelled.");
                }
                catch (const std::exception& e) {
                    LOG_ERROR("Failed to cancel timer: %s", e.what());
                }
                manager->isRemote.store(true);
                if (manager->onRemoteConnectHandle) {
                    manager->onRemoteConnectHandle();
                }

                auto localDesc = manager->peerConnection->local_description();
                if (localDesc) {
                    std::string sdp;
                    localDesc->ToString(&sdp);
                    // 简单解析SDP找到第一个video编解码器
                    std::istringstream stream(sdp);
                    std::string line;
                    while (std::getline(stream, line)) {
                        if (line.find("a=rtpmap:") == 0 && line.find("/90000") != std::string::npos) {
                            // 找到视频编解码器行，提取编解码器名称
                            size_t spacePos = line.find(' ');
                            if (spacePos != std::string::npos) {
                                std::string codecInfo = line.substr(spacePos + 1);
                                size_t slashPos = codecInfo.find('/');
                                if (slashPos != std::string::npos) {
                                    std::string codecName = codecInfo.substr(0, slashPos);
                                    LOG_INFO("=== Video codec actually being used: %s ===", codecName.c_str());
                                    break;
                                }
                            }
                        }
                    }
                }

                break;

            }
            case webrtc::PeerConnectionInterface::kIceConnectionDisconnected: {

				LOG_INFO("ICE connection disconnected");

                manager->handleRemoteDisconnect();

                break;

            }

            case webrtc::PeerConnectionInterface::kIceConnectionFailed:
                LOG_ERROR("ICE connection failed");
                break;
            default:
                break;
            }
        }
        void PeerConnectionObserverImpl::OnConnectionChange(webrtc::PeerConnectionInterface::PeerConnectionState newState) {
            switch (newState) {
            case webrtc::PeerConnectionInterface::PeerConnectionState::kConnected: {
                LOG_INFO("Peer connection established");
                break;
            }
            case webrtc::PeerConnectionInterface::PeerConnectionState::kDisconnected: {
    
                LOG_ERROR("Peer connection disconnected");
                break;
            }
            case webrtc::PeerConnectionInterface::PeerConnectionState::kFailed: {

                LOG_ERROR("Peer connection failed");
                break;
            }
            case webrtc::PeerConnectionInterface::PeerConnectionState::kClosed: {
                break;
            }
            default:
                break;
            }
        }

	}

}