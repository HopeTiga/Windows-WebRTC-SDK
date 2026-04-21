#include "PeerConnectionObserverImpl.h"

#include <boost/json.hpp>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "WebRTCManager.h"
#include "videotracksinkimpl.h"

#include "HWebRTC.h"

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

            if (!dataChannel) {
                LOG_ERROR("DataChannel is null");
                return;
			}

            boost::uuids::random_generator gen;

            std::string dataChannelId = boost::uuids::to_string(gen());

            peerConnectionManager->dataChannels[dataChannelId] = dataChannel;

            peerConnectionManager->dataChannelObserverMaps[dataChannelId] = std::make_unique<DataChannelObserverImpl>(manager,peerConnectionManager, dataChannelId);

            if (manager->onReceiveDataChannel) {
            
				manager->onReceiveDataChannel(peerConnectionManager->peerConnectionId,dataChannelId);

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

                boost::uuids::random_generator gen;

                std::string videoTrackId = boost::uuids::to_string(gen());

                receiver->SetJitterBufferMinimumDelay(std::optional<double>(0.00));

                peerConnectionManager->videoTracks[videoTrackId] = webrtc::scoped_refptr<webrtc::VideoTrackInterface>(
                    static_cast<webrtc::VideoTrackInterface*>(track.release())
                );

                std::unique_ptr<VideoTrackSinkImpl> videoSinkImpl = std::make_unique<VideoTrackSinkImpl>(manager,peerConnectionManager, videoTrackId);

                peerConnectionManager->videoTracks[videoTrackId]->AddOrUpdateSink(videoSinkImpl.get(), webrtc::VideoSinkWants());

                peerConnectionManager->videoTrackSinkMaps[videoTrackId] = std::move(videoSinkImpl);

                if (manager->onReceiveTrack) {
                
                    manager->onReceiveTrack(peerConnectionManager->peerConnectionId, videoTrackId,static_cast<int>(WebRTCTrackType::video));

                }

                return;
            }

            if (track->kind() == webrtc::MediaStreamTrackInterface::kAudioKind) {

                LOG_INFO("Audio track received");

                receiver->SetJitterBufferMinimumDelay(std::optional<double>(0.00));

                boost::uuids::random_generator gen;

                std::string audioTrackId = boost::uuids::to_string(gen());

                peerConnectionManager->audioTracks[audioTrackId] = webrtc::scoped_refptr<webrtc::AudioTrackInterface>(
                    static_cast<webrtc::AudioTrackInterface*>(track.release())
                );

                if (manager->onReceiveTrack) {

                    manager->onReceiveTrack(peerConnectionManager->peerConnectionId,audioTrackId, static_cast<int>(WebRTCTrackType::audio));

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

            if (manager->onIceCandidateHandle) {
            
				manager->onIceCandidateHandle(peerConnectionManager->peerConnectionId, sdp, candidate->sdp_mid(), candidate->sdp_mline_index());

            }
        }
        void PeerConnectionObserverImpl::OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState newState) {
            switch (newState) {
            case webrtc::PeerConnectionInterface::kIceConnectionConnected: {
            
                handle = webrtc::make_ref_counted<hope::rtc::RTCStatsCollectorHandle>();

                peerConnectionManager->peerConnection->GetStats(handle.get());

                if (manager->onRemoteConnectHandle) {
                    manager->onRemoteConnectHandle(peerConnectionManager->peerConnectionId);
                }

                break;

            }
            case webrtc::PeerConnectionInterface::kIceConnectionDisconnected: {

				LOG_INFO("ICE connection disconnected");

                if (manager->onRemoteDisConnectHandle) {
                    manager->onRemoteDisConnectHandle(peerConnectionManager->peerConnectionId);
                }
                
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