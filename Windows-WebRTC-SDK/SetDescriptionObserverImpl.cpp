#include "SetDescriptionObserverImpl.h"
#include "Utils.h"

namespace hope {

	namespace rtc {
	
		// SetLocalDescriptionObserver实现
		void SetLocalDescriptionObserver::OnSuccess() {
			LOG_INFO("SetLocalDescription Success!");
		}

		void SetLocalDescriptionObserver::OnFailure(webrtc::RTCError error) {
			LOG_ERROR("SetLocalDescription failed: %s", error.message());
		}

		// SetRemoteDescriptionObserver实现
		void SetRemoteDescriptionObserver::OnSuccess() {
			LOG_INFO("SetRemoteDescription Success!");
		}

		void SetRemoteDescriptionObserver::OnFailure(webrtc::RTCError error) {
			LOG_ERROR("SetRemoteDescription failed: %s" , error.message());
		}

	}
}