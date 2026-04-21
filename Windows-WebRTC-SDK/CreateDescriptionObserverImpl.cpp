#include "CreateDescriptionObserverImpl.h"

#include "WebRTCManager.h"
#include "PeerConnectionManager.h"

#include "Utils.h"

namespace hope {

	namespace rtc {
	
        // CreateOfferObserverImpl实现
        void CreateOfferObserverImpl::OnSuccess(webrtc::SessionDescriptionInterface* desc) {
            if (!desc) {
                LOG_ERROR("CreateOffer success callback received null description");
                return;
            }

            std::string sdp;

            desc->ToString(&sdp);

            auto observer = SetLocalDescriptionObserver::Create();

            peerConnectionManager->peerConnection->SetLocalDescription(observer.get(), desc);

            if (manager->onOfferHandle) {
            
                manager->onOfferHandle(peerConnectionManager->peerConnectionId,sdp);

            }
        }

        void CreateOfferObserverImpl::OnFailure(webrtc::RTCError error) {
            LOG_ERROR("CreateOffer failed: %s",error.message());
        }

        // CreateAnswerObserverImpl实现
        void CreateAnswerObserverImpl::OnSuccess(webrtc::SessionDescriptionInterface* desc) {
            if (!desc) {
                LOG_ERROR("CreateAnswer success callback received null description");
                return;
            }

            peerConnectionManager->peerConnection->SetLocalDescription(SetLocalDescriptionObserver::Create().get(), desc);

            std::string sdp;
            if (!desc->ToString(&sdp)) {
                LOG_ERROR("Failed to convert answer to string");
                return;
            }

            if (manager->onAnswerHandle) {

                manager->onAnswerHandle(peerConnectionManager->peerConnectionId, sdp);

            }
        }

        void CreateAnswerObserverImpl::OnFailure(webrtc::RTCError error) {
            LOG_ERROR("CreateAnswer failed: %s" , error.message());
        }

	}

}