#include "DataChannelObserverImpl.h"

#include "WebRTCManager.h"
#include "PeerConnectionManager.h"

#include "Utils.h"

namespace hope {

	namespace rtc {

		DataChannelObserverImpl::DataChannelObserverImpl(WebRTCManager * manager, PeerConnectionManager* peerConnectionManager,std::string dataChannelId):manager(manager), peerConnectionManager(peerConnectionManager), dataChannelId(dataChannelId) {
		}
	
		// The data channel state have changed.
		void DataChannelObserverImpl::OnStateChange() {
		}
		//  A data buffer was successfully received.
		void DataChannelObserverImpl::OnMessage(const webrtc::DataBuffer& buffer) {
		
			if (buffer.size() == 0) {
			
				LOG_ERROR("DataChannelObserverImpl::OnMessag webrtc::DataBuffer size : 0");

				return;
			}

			if (manager->onDataChannelDataHandle) {
			
				manager->onDataChannelDataHandle(peerConnectionManager->peerConnectionId, dataChannelId,std::move(reinterpret_cast<const unsigned char*>(buffer.data.data())), buffer.data.size());

			}

		}

	}

}