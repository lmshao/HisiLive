# HisiLive

原项目是基于海思 Hi3516A 开发板的 RTP 流媒体服务器系统。因为海思 SDK 版本更新以及为适配不同开发板和 Sensor，所以更新原目录结构，仅保存最基础的源码部分，原项目标记 tag v1.0 作为备份。

当前使用 Hi3516CV500_SDK_V2.0.2.0 + Hi3616D + IMX335 进行开发。

## 使用方法

把本工程放置在 SDK sample venc 同级目录下进行编译。

### 帮助 -h

```txt
~ # ./HisiLive -h
Usage : ./HisiLive
         -m: mode: file/rtp, default file.
         -e: video decode format, default H.264.
         -f: frame rate, default 24 fps.
         -b: bitrate, default 1024 kbps.
         -i: IP, default 192.168.1.100.
         -s: video size: 1080p/720p/360p/CIF, default 1080p
Default parameters: ./HisiLive -m rtp -e 264 -f 30 -b 1024 -s 720p -i 192.168.1.100
```

### 本地保存视频

```sh
./HisiLive -m file
```

### RTP 协议发送

```sh
./HisiLive -m rtp -i 192.168.1.xxx
```

VLC 打开此目录下的 play.sdp 文件可以播放实时视频。
