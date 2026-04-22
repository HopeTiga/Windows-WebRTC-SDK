#pragma once
#include <api/media_stream_interface.h>

namespace hope{
	namespace rtc {

		class WebRTCManager;

		class PeerConnectionManager;

		class AudioTrackSinkImpl : public webrtc::AudioTrackSinkInterface
		{

		public:

			AudioTrackSinkImpl(WebRTCManager* manager, PeerConnectionManager* peerConnectionManager,std::string audioTrackId);

			~AudioTrackSinkImpl();

			void OnData(const void* audio_data, int bits_per_sample, int sample_rate, size_t number_of_channels, size_t number_of_frames) override;

		private:

			WebRTCManager * manager;

			PeerConnectionManager * peerConnectionManager;

			std::string audioTrackId;
		};
	}
}

