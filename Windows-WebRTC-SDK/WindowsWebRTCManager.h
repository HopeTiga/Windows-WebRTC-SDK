#pragma once

// Windows 7 compatibility settings
#ifndef WINVER
#define WINVER 0x0601          // Windows 7
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601    // Windows 7
#endif

#ifndef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_WIN7  // Windows 7
#endif

// Reduce Windows header bloat
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// Prevent min/max macro conflicts
#ifndef NOMINMAX
#define NOMINMAX
#endif

// Standard includes
#include <windows.h>
#include <memory>
#include <string>
#include <functional>
#include "LibWebRTC.h"

#ifdef WEBRTCMANAGER_EXPORTS
#define WEBRTCMANAGER_API __declspec(dllexport)
#else
#define WEBRTCMANAGER_API __declspec(dllimport)
#endif

// Forward declaration to hide WebRTCManager from public interface
namespace hope {

	namespace rtc {
        class WebRTCManager;

        /**
 * @class WindowsWebRTCManager
 * @brief Windows平台WebRTC管理器类，提供实时音视频通信和数据通道功能
 *
 * 该类封装了WebRTC的核心功能，包括点对点连接、媒体流传输和数据通道通信。
 * 采用Pimpl惯用法隐藏实现细节，确保接口稳定性。
 * @note 此类不可复制，使用时需通过智能指针管理生命周期
 * @see WebRTCManager 实际实现类
 */
        class WEBRTCMANAGER_API WindowsWebRTCManager
        {
        public:
            // 优先级枚举已注释掉，保留以备将来扩展
            // enum class Priority { kVeryLow, kLow, kMedium, kHigh };

            /**
             * @brief 构造函数 - 创建WebRTC管理器实例
             * @note 构造函数会初始化WebRTC环境，但不立即建立连接
             */
            WindowsWebRTCManager();

            /**
             * @brief 析构函数 - 清理资源并断开所有连接
             * @warning 析构时会自动断开与信令服务器和远程端的连接
             */
            ~WindowsWebRTCManager();

            // Connection management
            /**
             * @brief 连接到信令服务器
             * @param ip 信令服务器IP地址
             * @param registerJson 注册信息JSON字符串
             * @see setOnSignalServerConnectHandle 设置连接结果回调
             * @note 连接结果为异步通知，需通过回调函数获取
             */
            void connect(const char* ip, const char* registerJson);

            /**
             * @brief 断开所有连接（包括信令服务器和远程端）
             */
            void disConnect();

            /**
             * @brief 仅断开远程端连接，保持信令服务器连接
             */
            void disConnectRemote();
            
            /**
             * @brief 创建视频轨道
             * @param codec 视频编解码器类型（VP8/VP9/H264/AV1等）
             * @param rtpEncodingConfig RTP编码配置参数，如码率、分辨率缩放、fec等
             * @return 视频轨道ID（非负整数），失败返回-1
             * @note 创建后需通过writeVideoFrame(trackId, ...)推送数据
             * @warning 需在connect()之前调用，连接建立后无法再创建新轨道
             * @see writeVideoFrame, setOnReceiveTrack
             * @example
             *   int camTrack = manager->createVideoTrack(WebRTCVideoCodec::VP8);
             *   int screenTrack = manager->createVideoTrack(
             *       WebRTCVideoCodec::H264,
             *       {.maxBitrateBps = 4000000, .scaleResolutionDownBy = 1.0}
             *   );
             */
            int createVideoTrack(WebRTCVideoCodec codec,WebRTCVideoPreference preference = WebRTCVideoPreference::DISABLED,RtpEncodingConfig rtpEncodingConfig = {});

            /**
             * @brief 创建音频轨道
             * @return 音频轨道ID（非负整数），失败返回-1
             * @note 创建后需通过writeAudioFrame(trackId, ...)推送PCM数据 目前仅能创建一个
             * @warning 需在connect()之前调用，支持同时创建多路音频（如麦克风+系统声音）
             * @see writeAudioFrame, setOnReceiveTrack
             * @example
             *   int micTrack = manager->createAudioTrack();      // 默认opus 48kHz
             *   int sysTrack = manager->createAudioTrack();      // 系统声音混音
             */
            int createAudioTrack();

            /**
             * @brief 创建数据通道（DataChannel）
             * @return 数据通道ID（非负整数），失败返回-1
             * @note 用于传输任意二进制数据，基于SCTP协议，支持可靠/不可靠传输模式
             * @warning 需在connect()之前调用，连接建立后无法创建（WebRTC限制）
             * @see wrtieDataChannelData, setOnDataChannelDataHandle, setOnReceiveDataChannel
             * @example
             *   int ctrlChannel = manager->createDataChannel();  // 控制信令
             *   int fileChannel = manager->createDataChannel();  // 文件传输（建议另开通道避免阻塞）
             */
            int createDataChannel();

            // Account management
            /**
             * @brief 设置当前用户账号ID
             * @param accountId 账号标识字符串
             */
            void setAccountId(const char* accountId);

            /**
             * @brief 获取当前用户账号ID
             * @return 账号标识字符串
             */
            std::string getAccountId() const;

            // Account management
            /**
            * @brief 设置对端用户账号ID
            * @param targetId 账号标识字符串
            */
            void setTargetId(const char* targetId);

            /**
             * @brief 获取对端用户账号ID
             * @return 账号标识字符串
             */
            std::string getTargetId() const;

            // Media streaming
            /**
             * @brief 写入视频帧数据
             * @param data 视频帧数据指针
             * @param size 数据大小（字节）
             * @param width 视频宽度（像素）
             * @param height 视频高度（像素）
             * @note 数据格式需与编码器要求一致
             */
            bool writeVideoFrame(int videoTrackId,void* data, size_t size, int width, int height);

            /**
             * @brief 写入音频帧数据
             * @param data 音频帧数据指针
             * @param size 数据大小（字节）
             */
            bool writeAudioFrame(int audioTrackId,void* data, size_t size);

            /**
             * @brief 通过数据通道发送数据
             * @param data 要发送的数据指针
             * @param size 数据大小（字节）
             * @see setOnDataChannelDataHandle 接收数据通道数据的回调
             */
            bool writeDataChannelData(int dataChannelId,void* data, size_t size);

            /**
             * @brief 设置信令服务器连接结果回调
             * @param handle 回调函数，参数true表示成功，false表示失败
             */
            void setOnSignalServerConnectHandle(std::function<void()> handle);

            /**
             * @brief 设置信令服务器断开连接回调
             * @param handle 无参数无返回值的回调函数
             */
            void setOnSignalServerDisConnectHandle(std::function<void()> handle);

            /**
             * @brief 设置远程端连接结果回调
             * @param handle 回调函数，参数true表示成功，false表示失败
             */
            void setOnRemoteConnectHandle(std::function<void()> handle);

            /**
             * @brief 设置远程端断开连接回调
             * @param handle 无参数无返回值的回调函数
             */
            void setOnRemoteDisConnectHandle(std::function<void()> handle);

            /**
             * @brief 设置数据通道数据接收回调
             * @param handle 回调函数，参数为数据指针和数据大小
             */
            void setOnDataChannelDataHandle(std::function<void(const unsigned char*, size_t,int)> handle);

            /**
            * @brief 设置重新初始化的回调处理函数
            * @param func 重新初始化回调函数，需满足 std::function<void()> 签名
            * @note 当系统或组件需要执行重新初始化操作时，调用此函数注册的回调
            *       常用于重置内部状态、清理资源后重新建立初始环境等场景
            * @warning 回调函数应避免执行耗时操作，以免阻塞重新初始化流程
            * @see getOnReInitHandle()
            * @example
            *   obj.setOnReInitHandle([]() {
            *       // 执行重新初始化逻辑
            *       resetInternalState();
            *       reloadConfiguration();
            *   });
            */
            void setOnReInitHandle(std::function<void()> func);

            /**
            * @brief 设置接收DataChannel的回调函数
			 *@param func 回调函数参数为DataChannel ID
			 */
            void setOnReceiveDataChannel(std::function<void(int)> handle);

            /**
             * @brief 设置接收媒体轨道的回调函数
             * @param func 回调函数，参数为轨道ID和类型ID
			 */
            void setOnReceiveTrack(std::function<void(int, int)> handle);

            /**
             * @brief 设置接收视频帧的回调函数
             * @param func 回调函数，参数为视频轨道ID、宽度、高度、Y分量、U分量、V分量、Y步长、U步长、V步长
             * @note 该回调在接收到新的视频帧时触发
			 */
            void setOnReceiveVideoFrameHandle(std::function<void(int, int, int, const uint8_t*, const uint8_t*, const uint8_t*, int, int, int)> handle);

            /**
             * @brief 获取当前连接状态
             * @return 连接状态枚举值
             */
            WebRTCConnetState getConnectionState() const;

            /**
             * @brief 添加STUN服务器
             * @param host STUN服务器地址
             * @note 应在连接前调用此方法
             */
            void addStunServer(const char* host);

            /**
             * @brief 添加TURN服务器
             * @param host TURN服务器地址
             * @param username 用户名
             * @param password 密码
             * @note 应在连接前调用此方法
             */
            void addTurnServer(const char* host, const char* username, const char* password);

            /**
             * @brief 异步发送JSON数据到信令服务器
             * @param json 要发送的JSON字符串
             * @warning 此方法为内部使用，不建议直接调用
             */
            void writerAsync(const char* json);

        private:
            /// @brief Pimpl指针，指向实际实现类
            std::shared_ptr<WebRTCManager> webRTCManager;

            // Disable copy constructor and assignment operator
            WindowsWebRTCManager(const WindowsWebRTCManager&) = delete;
            WindowsWebRTCManager& operator=(const WindowsWebRTCManager&) = delete;
        };
	}

}