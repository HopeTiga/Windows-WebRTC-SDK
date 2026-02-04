#include "DataChannelObserverImpl.h"

#include "WebRTCManager.h"

#include "Utils.h"

namespace hope {

	namespace rtc {

		DataChannelObserverImpl::DataChannelObserverImpl(WebRTCManager * manager, int dataChannelId):manager(manager), dataChannelId(dataChannelId){
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

			if (buffer.size() > 1024 * 1024) {
			
				LOG_ERROR("DataChannelObserverImpl::OnMessag webrtc::DataBuffer Exceeds the size limit");

				return;
			
			}

			if (manager->onDataChannelDataHandle) {
			
				manager->onDataChannelDataHandle(std::move(reinterpret_cast<const unsigned char*>(buffer.data.data())), buffer.data.size(), dataChannelId);

			}

		}

	}

}