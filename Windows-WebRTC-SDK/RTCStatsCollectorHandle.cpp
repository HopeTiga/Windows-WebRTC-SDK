#include "RTCStatsCollectorHandle.h"

#include "Utils.h"

namespace hope {

	namespace rtc {
	
        void RTCStatsCollectorHandle::OnStatsDelivered(const webrtc::scoped_refptr<const webrtc::RTCStatsReport>& report) {
            std::string selected_pair_id;

            // 1. 遍历报告，寻找 Transport Stats 以获取选中的 Candidate Pair ID
            for (const auto& stat : *report) {
                if (stat.type() == webrtc::RTCTransportStats::kType) {
                    const auto& transport = stat.cast_to<webrtc::RTCTransportStats>();

                    // 使用 .has_value() 检查是否存在
                    if (transport.selected_candidate_pair_id.has_value()) {
                        selected_pair_id = *transport.selected_candidate_pair_id;
                        break;
                    }
                }
            }

            if (selected_pair_id.empty()) {
                LOG_WARN("No selected candidate pair yet (Connection might not be ready).");
                return;
            }

            // 2. 根据 ID 找到选中的 Candidate Pair
            const webrtc::RTCStats* pair_stat = report->Get(selected_pair_id);
            if (!pair_stat) return;

            const auto& candidate_pair = pair_stat->cast_to<webrtc::RTCIceCandidatePairStats>();

            // 获取本地和远端候选者的 ID
            std::string local_candidate_id = *candidate_pair.local_candidate_id;
            std::string remote_candidate_id = *candidate_pair.remote_candidate_id;

            // 3. 查找具体的 Candidate 对象并判断类型
            const webrtc::RTCStats* local_cand_stat = report->Get(local_candidate_id);
            const webrtc::RTCStats* remote_cand_stat = report->Get(remote_candidate_id);

            if (local_cand_stat && remote_cand_stat) {
                const auto& local_cand = local_cand_stat->cast_to<webrtc::RTCIceCandidateStats>();
                const auto& remote_cand = remote_cand_stat->cast_to<webrtc::RTCIceCandidateStats>();

                std::string local_type = *local_cand.candidate_type;
                std::string remote_type = *remote_cand.candidate_type;

                LOG_INFO("Active Connection Info:");

                // 注意：printf 风格需要使用 %s，并且 string 必须调用 .c_str()
                LOG_INFO("  Local Type: %s | IP: %s",
                    local_type.c_str(),
                    local_cand.ip->c_str());

                LOG_INFO("  Remote Type: %s | IP: %s",
                    remote_type.c_str(),
                    remote_cand.ip->c_str());

                // 4. 判断逻辑
                if (local_type == "relay" || remote_type == "relay") {
                    LOG_INFO("==> Connection Type: TURN (Relayed)");
                }
                else {
                    LOG_INFO("==> Connection Type: P2P (STUN/Direct)");
                }
            }
        }

	}

}
