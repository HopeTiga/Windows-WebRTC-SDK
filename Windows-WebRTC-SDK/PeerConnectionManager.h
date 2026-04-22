#pragma once

#include <string>
#include <memory>
#include <vector>


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

#include "HWebRTC.h"
#include "PeerConnectionObserverImpl.h"
#include "VideoTrackSourceImpl.h"
#include "AudioDeviceModuleImpl.h"
#include "DataChannelObserverImpl.h"
#include "SetDescriptionObserverImpl.h"
#include "CreateDescriptionObserverImpl.h"
#include "VideoTrackSinkImpl.h"
#include "AudioTrackSinkImpl.h"


namespace hope {

	namespace rtc
	{

        class WebRTCManager;

        class PeerConnectionManager : std::enable_shared_from_this<PeerConnectionManager> {

			friend class PeerConnectionObserverImpl;

			friend class CreateOfferObserverImpl;

			friend class CreateAnswerObserverImpl;

        public:

            PeerConnectionManager(webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peerConnectionFactory
				, std::string peerConnectionId
                , WebRTCManager * webrtcManager);

            ~PeerConnectionManager();

            void releaseSource();

            bool createPeerConnection();

            std::string createVideoTrack(WebRTCVideoCodec codec, WebRTCVideoPreference preference = WebRTCVideoPreference::DISABLED);

            std::string createAudioTrack();

            std::string createDataChannel(std::string label);

            bool writerVideoFrame(std::string videoTrackId, unsigned char* data, size_t size, int width, int height);

            bool writerAudioFrame(std::string audioTrackId, unsigned char* data, size_t size);

            bool writerDataChannelData(std::string dataChannelId, unsigned char* data, size_t size);

            bool createOffer();

            void processOffer(const std::string& sdp);

            void processAnswer(const std::string& sdp);

            void processIceCandidate(const std::string& candidate,
                const std::string& mid, int lineIndex);

        public:

            std::string peerConnectionId;

        private:

            WebRTCManager* webrtcManager;

            webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peerConnectionFactory;

            webrtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection;

            std::unordered_map<std::string,webrtc::scoped_refptr<webrtc::DataChannelInterface>> dataChannels;

            std::unordered_map<std::string, webrtc::scoped_refptr<webrtc::VideoTrackInterface>> videoTracks;

            std::unordered_map<std::string, webrtc::scoped_refptr<webrtc::AudioTrackInterface>> audioTracks;

            std::unique_ptr<PeerConnectionObserverImpl> peerConnectionObserver;

            webrtc::scoped_refptr<CreateOfferObserverImpl> createOfferObserver;

            webrtc::scoped_refptr<CreateAnswerObserverImpl> createAnswerObserver;

            std::unordered_map<std::string, std::unique_ptr<DataChannelObserverImpl>> dataChannelObserverMaps;

            std::unordered_map<std::string, webrtc::scoped_refptr<VideoTrackSourceImpl>> videoTrackSourceImplMaps;

            std::unordered_map<std::string, std::unique_ptr<VideoTrackSinkImpl>> videoTrackSinkMaps;

            std::unordered_map<std::string, std::unique_ptr<AudioTrackSinkImpl>> audioTrackSinkMaps;

			std::atomic<bool> isClosed{ false };

        };

	}

}

