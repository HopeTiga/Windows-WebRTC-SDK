#include "AudioTrackSinkImpl.h"

#include "WebRTCManager.h"
#include "PeerConnectionManager.h"

#include "Utils.h"

namespace hope {

	namespace rtc {
		AudioTrackSinkImpl::AudioTrackSinkImpl(WebRTCManager* manager, PeerConnectionManager* peerConnectionManager, std::string audioTrackId) :manager(manager), peerConnectionManager(peerConnectionManager), audioTrackId(audioTrackId)
		{

		}
		AudioTrackSinkImpl::~AudioTrackSinkImpl()
		{
		}
		void AudioTrackSinkImpl::OnData(const void* audioData, int bitsPerSample, int sampleRate, size_t numberOfChannels, size_t numberOfFrames)
		{

			if (!manager || !peerConnectionManager || !audioData) return;

			if (manager->onReceiveAudioFrameHandle) {

				manager->onReceiveAudioFrameHandle(peerConnectionManager->peerConnectionId, audioTrackId, audioData, bitsPerSample, sampleRate, numberOfChannels, numberOfFrames);

			}

		}
	}

}
