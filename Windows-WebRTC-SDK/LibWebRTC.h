#pragma once
#include <string>
#include <optional>

enum class WebRTCTrackType {

	video = 0, // 视频轨道
	audio = 1, // 音频轨道

};

// Shared enum definitions for WebRTC Manager
enum class WebRTCRemoteState {
    nullRemote = 0,
    masterRemote = 1,
    followerRemote = 2,
};

enum class WebRTCConnetState {
    none,
    connect,
};

enum class WebRTCRequestState {
    REGISTER = 0,
    REQUEST = 1,
    RESTART = 2,
    STOPREMOTE = 3,
};

enum class WebRTCVideoCodec {
    VP8,    // VP8 视频编解码器
    VP9,    // VP9 视频编解码器
    H264,   // H.264 视频编解码器
    H265,   // H.265 视频编解码器
    AV1,    // AV1 视频编解码器
};

enum class WebRTCVideoPreference {
 
    DISABLED,

    MAINTAIN_FRAMERATE,

    MAINTAIN_RESOLUTION,

    BALANCED,
};



enum class LogLevels {
    DEBUGS = 0,
    INFO = 1,
    WARNING = 2,
    ERRORS = 3,
};

enum class Priority {
    VeryLow,
    Low,
    Medium,
    High
};

// RTP 编码参数配置结构体
struct RtpEncodingConfig {
    // 基本参数
    bool active = true;
    int max_bitrate_bps = 2000000;      // 4 Mbps 默认设置
    bool has_max_bitrate_bps = true;    // 标记是否设置
    int min_bitrate_bps = 2000000;      // 1 Mbps 默认设置
    bool has_min_bitrate_bps = true;    // 标记是否设置
    double max_framerate = 120.0;       // 120 fps 默认设置
    bool has_max_framerate = true;      // 标记是否设置
    double scale_resolution_down_by = 1.0;  // 缩放比例，默认设置
    bool has_scale_resolution_down_by = true; // 标记是否设置
    std::string scalability_mode = "L1T1"; // 默认模式
    bool has_scalability_mode = true;   // 标记是否设置
    Priority network_priority = Priority::High;
    double bitrate_priority = 4.0;  // 对应 High 优先级
    // 高级可选参数
    int num_temporal_layers = 0;        // 默认值，通常0表示未设置
    bool has_num_temporal_layers = false; // 标记是否设置
    std::string rid;
    bool request_key_frame = false;
    bool adaptive_ptime = false;
};