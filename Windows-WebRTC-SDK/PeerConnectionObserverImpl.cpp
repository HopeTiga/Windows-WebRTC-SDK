#include "PeerConnectionObserverImpl.h"

#include <boost/json.hpp>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "WebRTCManager.h"
#include "VideoTrackSinkImpl.h"
#include "AudioTrackSinkImpl.h"

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

                std::unique_ptr<AudioTrackSinkImpl> audioSinkImpl = std::make_unique<AudioTrackSinkImpl>(manager, peerConnectionManager, audioTrackId);

                peerConnectionManager->audioTracks[audioTrackId]->AddSink(audioSinkImpl.get());

                peerConnectionManager->audioTrackSinkMaps[audioTrackId] = std::move(audioSinkImpl);

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

            if (manager->onIceCandidateHandle) {
            
				manager->onIceCandidateHandle(peerConnectionManager->peerConnectionId, sdp, candidate->sdp_mid(), candidate->sdp_mline_index());

            }
        }
        void PeerConnectionObserverImpl::OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState newState) {

            if (manager->onIceConnectionStateChangeHandle) {
            
                manager->onIceConnectionStateChangeHandle(peerConnectionManager->peerConnectionId, static_cast<int>(newState));

            }

            switch (newState) {
            case webrtc::PeerConnectionInterface::kIceConnectionConnected: {
            
                handle = webrtc::make_ref_counted<hope::rtc::RTCStatsCollectorHandle>();

                peerConnectionManager->peerConnection->GetStats(handle.get());


                break;

            }
            case webrtc::PeerConnectionInterface::kIceConnectionDisconnected: {
                break;
            }

            case webrtc::PeerConnectionInterface::kIceConnectionFailed:
                break;
            default:
                break;
            }
        }
        void PeerConnectionObserverImpl::OnConnectionChange(webrtc::PeerConnectionInterface::PeerConnectionState newState) {

            if (manager->onPeerConnectionStateChangeHandle) {
            
                manager->onPeerConnectionStateChangeHandle(peerConnectionManager->peerConnectionId, static_cast<int>(newState));

            }

            switch (newState) {
            case webrtc::PeerConnectionInterface::PeerConnectionState::kConnected: {
                break;
            }
            case webrtc::PeerConnectionInterface::PeerConnectionState::kDisconnected: {

                break;
            }
            case webrtc::PeerConnectionInterface::PeerConnectionState::kFailed: {
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