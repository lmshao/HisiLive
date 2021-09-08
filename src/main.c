/******************************************************************************
  A simple program of Hisilicon Hi3516 multi-media live implementation.

  Copyright (c) 2018-2021 Liming Shao <lmshao@163.com>
******************************************************************************/
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/prctl.h>

#include "Network.h"
#include "RTP.h"
#include "Utils.h"
#include "sample_comm.h"

// clang-format off
typedef enum {
    MODE_FILE,
    MODE_RTP
} RunMode;
// clang-format on

typedef struct {
    RunMode mode;                // -m
    int frameRate;               // -f
    int bitRate;                 // -b
    char ip[16];                 // -i
    PAYLOAD_TYPE_E videoFormat;  // -e
    PIC_SIZE_E videoSize;        // -s
} ParamOption;

ParamOption gParamOption;
RTPMuxContext gRTPCtx;
UDPContext gUDPCtx;
static pthread_t gMediaProcPid;
static SAMPLE_VENC_GETSTREAM_PARA_S gMediaProcPara;

void HisiLive_ShowUsage(char *sPrgNm)
{
    printf("\033[32m");
    printf("Usage : %s \n", sPrgNm);
    printf("\t -m: mode: file/rtp, default file.\n");
    printf("\t -e: video decode format, default H.264.\n");
    printf("\t -f: frame rate, default 24 fps.\n");
    printf("\t -b: bitrate, default 1024 kbps.\n");
    printf("\t -i: IP, default 192.168.1.100.\n");
    printf("\t -s: video size: 1080p/720p/360p/CIF, default 1080p\n");
    printf("Default parameters: %s -m rtp -e 264 -f 30 -b 1024 -s 720p -i 192.168.1.100\n", sPrgNm);
    printf("\033[0m");

    return;
}

void printParamOptions(ParamOption *options)
{
    char buff[256] = { 0 };
    char *mode, *format, *resolution;

    if (options->mode == MODE_FILE) {
        mode = "File";
    } else if (options->mode == MODE_RTP) {
        mode = "RTP";
    } else {
        mode = "Unknown";
    }

    if (options->videoFormat == PT_H264) {
        format = "H.264";
    } else if (options->videoFormat == PT_H265) {
        format = "H.265";
    } else {
        format = "unknown";
    }

    if (options->videoSize == PIC_1080P) {
        resolution = "1080P";
    } else if (options->videoSize == PIC_720P) {
        resolution = "720P";
    } else if (options->videoSize == PIC_360P) {
        resolution = "360p";
    } else if (options->videoSize == PIC_CIF) {
        resolution = "CIF";
    } else {
        resolution = "unknown";
    }

    sprintf(buff, "mode:%s, format:%s, framerate: %dfps, bitrate: %dkb/s, to ip: %s, resolution: %s", mode, format, options->frameRate,
            options->bitRate, options->ip, resolution);

    LOGD("%s\n", buff);
    writeFile("log.txt", buff, strlen(buff), 1);

    return;
}

int HisiLive_ParseParam(int argc, char **argv)
{
    int ret = 0;

    if (argc % 2 == 0)
        return -1;

    // init default parameters
    gParamOption.mode = MODE_RTP;
    gParamOption.frameRate = 30;  // fps
    gParamOption.bitRate = 0;     // kbps
    sprintf(gParamOption.ip, "%s", "192.168.1.100");
    gParamOption.videoSize = PIC_720P;
    gParamOption.videoFormat = PT_H264;  // H.264

    while ((ret = getopt(argc, argv, ":m:e:f:b:i:s:")) != -1) {
        switch (ret) {
            case ('m'):
                LOGD("-m: %s \n", optarg);
                if (!strcmp(optarg, "file") || !strcmp(optarg, "FILE")) {
                    gParamOption.mode = MODE_FILE;
                } else if (!strcmp(optarg, "rtp") || !strcmp(optarg, "RTP")) {
                    gParamOption.mode = MODE_RTP;
                } else {
                    LOGE("mode %s is invalid\n", optarg);
                    return -1;
                }
                break;
            case ('e'):
                LOGD("-e: %s\n", optarg);
                if (strstr(optarg, "264") || !strcmp(optarg, "AVC") || !strcmp(optarg, "avc")) {
                    gParamOption.videoFormat = PT_H264;
                    LOGD("PT_H264\n");
                } else if (strstr(optarg, "265") || !strcmp(optarg, "HEVC") || !strcmp(optarg, "hevc")) {
                    gParamOption.videoFormat = PT_H265;
                    LOGD("PT_H265\n");
                } else {
                    LOGE("VedeoFormat is invalid.\n");
                    return -1;
                }
                break;
            case ('f'):
                LOGD("-f: %s\n", optarg);
                int f = atoi(optarg);
                if (f <= 0 || f > 30) {
                    LOGE("frameRate is not in (0, 30]\n");
                    return -1;
                } else {
                    gParamOption.frameRate = f;
                }
                break;
            case ('b'):
                LOGD("-b: %s\n", optarg);
                int b = atoi(optarg);
                if (b <= 0 || b > 4096) {
                    LOGE("bitRate is not in (0, 4096]\n");
                    return -1;
                } else {
                    gParamOption.bitRate = b;
                }
                break;
            case ('i'):
                LOGD("-i: %s\n", optarg);
                if (inet_addr(optarg) == INADDR_NONE) {
                    LOGE("IP is invalid.\n");
                    return -1;
                } else {
                    sprintf(gParamOption.ip, "%s", optarg);
                }
                break;
            case ('s'):
                LOGD("-s: %s\n", optarg);
                if (!strcmp(optarg, "1080p") || !strcmp(optarg, "1080P")) {
                    gParamOption.videoSize = PIC_1080P;
                } else if (!strcmp(optarg, "720p") || !strcmp(optarg, "720P")) {
                    gParamOption.videoSize = PIC_720P;
                } else if (!strcmp(optarg, "360p") || !strcmp(optarg, "360")) {
                    gParamOption.videoSize = PIC_360P;
                } else if (!strcmp(optarg, "CIF") || !strcmp(optarg, "cif")) {
                    gParamOption.videoSize = PIC_CIF;
                } else {
                    LOGE("VedeoSize is invalid.\n");
                    return -1;
                }
                break;
            case ':':
                LOGE("option [-%c] requires an argument\n", (char)optopt);
                break;
            case '?':
                LOGE("unknown option: %c\n", (char)optopt);
                break;
            default:
                break;
        }
    }

    if (gParamOption.bitRate == 0) {
        if (gParamOption.videoSize == PIC_1080P) {
            gParamOption.bitRate = 2048 * gParamOption.frameRate / 30;
        } else if (gParamOption.videoSize == PIC_720P) {
            gParamOption.bitRate = 1024 * gParamOption.frameRate / 30;
        } else {
            gParamOption.bitRate = 512 * gParamOption.frameRate / 30;
        }
    }

    printParamOptions(&gParamOption);
    return 0;
}

HI_S32 HisiLive_COMM_VENC_StopGetStream(void)
{
    if (HI_TRUE == gMediaProcPara.bThreadStart) {
        gMediaProcPara.bThreadStart = HI_FALSE;
        pthread_join(gMediaProcPid, 0);
    }
    return HI_SUCCESS;
}

VENC_GOP_MODE_E SAMPLE_VENC_GetGopMode(void)
{
    char c;
    VENC_GOP_MODE_E enGopMode = 0;

Begin_Get:

    printf("please input choose gop mode!\n");
    printf("\t 0) NORMALP.\n");
    printf("\t 1) DUALP.\n");
    printf("\t 2) SMARTP.\n");

    while ((c = getchar()) != '\n' && c != EOF)
        switch (c) {
            case '0':
                enGopMode = VENC_GOPMODE_NORMALP;
                break;
            case '1':
                enGopMode = VENC_GOPMODE_DUALP;
                break;
            case '2':
                enGopMode = VENC_GOPMODE_SMARTP;
                break;
            default:
                SAMPLE_PRT("input rcmode: %c, is invaild!\n", c);
                goto Begin_Get;
        }

    return enGopMode;
}

SAMPLE_RC_E SAMPLE_VENC_GetRcMode(void)
{
    char c;
    SAMPLE_RC_E enRcMode = 0;

Begin_Get:

    printf("please input choose rc mode!\n");
    printf("\t c) cbr.\n");
    printf("\t v) vbr.\n");
    printf("\t a) avbr.\n");
    printf("\t x) cvbr.\n");
    printf("\t q) qvbr.\n");
    printf("\t f) fixQp\n");

    while ((c = getchar()) != '\n' && c != EOF)
        switch (c) {
            case 'c':
                enRcMode = SAMPLE_RC_CBR;
                break;
            case 'v':
                enRcMode = SAMPLE_RC_VBR;
                break;
            case 'a':
                enRcMode = SAMPLE_RC_AVBR;
                break;
            case 'q':
                enRcMode = SAMPLE_RC_QVBR;
                break;
            case 'x':
                enRcMode = SAMPLE_RC_CVBR;
                break;
            case 'f':
                enRcMode = SAMPLE_RC_FIXQP;
                break;
            default:
                SAMPLE_PRT("input rcmode: %c, is invaild!\n", c);
                goto Begin_Get;
        }
    return enRcMode;
}

HI_S32 SAMPLE_VENC_SYS_Init(HI_U32 u32SupplementConfig, SAMPLE_SNS_TYPE_E enSnsType)
{
    HI_S32 s32Ret;
    HI_U64 u64BlkSize;
    VB_CONFIG_S stVbConf;
    PIC_SIZE_E enSnsSize;
    SIZE_S stSnsSize;

    memset(&stVbConf, 0, sizeof(VB_CONFIG_S));

    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(enSnsType, &enSnsSize);
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("SAMPLE_COMM_VI_GetSizeBySensor failed!\n");
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enSnsSize, &stSnsSize);
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        return s32Ret;
    }

    u64BlkSize = COMMON_GetPicBufferSize(stSnsSize.u32Width, stSnsSize.u32Height, PIXEL_FORMAT_YVU_SEMIPLANAR_422, DATA_BITWIDTH_8,
                                         COMPRESS_MODE_SEG, DEFAULT_ALIGN);
    stVbConf.astCommPool[0].u64BlkSize = u64BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt = 10;

    u64BlkSize = COMMON_GetPicBufferSize(720, 576, PIXEL_FORMAT_YVU_SEMIPLANAR_422, DATA_BITWIDTH_8, COMPRESS_MODE_SEG, DEFAULT_ALIGN);
    stVbConf.astCommPool[1].u64BlkSize = u64BlkSize;
    stVbConf.astCommPool[1].u32BlkCnt = 10;

    stVbConf.u32MaxPoolCnt = 2;

    if (0 == u32SupplementConfig) {
        s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    } else {
        s32Ret = SAMPLE_COMM_SYS_InitWithVbSupplement(&stVbConf, u32SupplementConfig);
    }
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        return s32Ret;
    }

    return HI_SUCCESS;
}

HI_S32 SAMPLE_VENC_VI_Init(SAMPLE_VI_CONFIG_S *pstViConfig, HI_BOOL bLowDelay, HI_U32 u32SupplementConfig)
{
    HI_S32 s32Ret;
    SAMPLE_SNS_TYPE_E enSnsType;
    ISP_CTRL_PARAM_S stIspCtrlParam;
    HI_U32 u32FrameRate;

    enSnsType = pstViConfig->astViInfo[0].stSnsInfo.enSnsType;

    pstViConfig->as32WorkingViId[0] = 0;
    // pstViConfig->s32WorkingViNum                              = 1;

    pstViConfig->astViInfo[0].stSnsInfo.MipiDev = SAMPLE_COMM_VI_GetComboDevBySensor(pstViConfig->astViInfo[0].stSnsInfo.enSnsType, 0);
    pstViConfig->astViInfo[0].stSnsInfo.s32BusId = 0;

    // pstViConfig->astViInfo[0].stDevInfo.ViDev              = ViDev0;
    pstViConfig->astViInfo[0].stDevInfo.enWDRMode = WDR_MODE_NONE;

    if (HI_TRUE == bLowDelay) {
        pstViConfig->astViInfo[0].stPipeInfo.enMastPipeMode = VI_ONLINE_VPSS_ONLINE;
    } else {
        pstViConfig->astViInfo[0].stPipeInfo.enMastPipeMode = VI_OFFLINE_VPSS_OFFLINE;
    }
    s32Ret = SAMPLE_VENC_SYS_Init(u32SupplementConfig, enSnsType);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("Init SYS err for %#x!\n", s32Ret);
        return s32Ret;
    }

    // pstViConfig->astViInfo[0].stPipeInfo.aPipe[0]          = ViPipe0;
    pstViConfig->astViInfo[0].stPipeInfo.aPipe[1] = -1;
    pstViConfig->astViInfo[0].stPipeInfo.aPipe[2] = -1;
    pstViConfig->astViInfo[0].stPipeInfo.aPipe[3] = -1;

    // pstViConfig->astViInfo[0].stChnInfo.ViChn              = ViChn;
    // pstViConfig->astViInfo[0].stChnInfo.enPixFormat        = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    // pstViConfig->astViInfo[0].stChnInfo.enDynamicRange     = enDynamicRange;
    pstViConfig->astViInfo[0].stChnInfo.enVideoFormat = VIDEO_FORMAT_LINEAR;
    pstViConfig->astViInfo[0].stChnInfo.enCompressMode = COMPRESS_MODE_SEG;  // COMPRESS_MODE_SEG;
    s32Ret = SAMPLE_COMM_VI_SetParam(pstViConfig);
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("SAMPLE_COMM_VI_SetParam failed with %d!\n", s32Ret);
        return s32Ret;
    }

    SAMPLE_COMM_VI_GetFrameRateBySensor(enSnsType, &u32FrameRate);

    s32Ret = HI_MPI_ISP_GetCtrlParam(pstViConfig->astViInfo[0].stPipeInfo.aPipe[0], &stIspCtrlParam);
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("HI_MPI_ISP_GetCtrlParam failed with %d!\n", s32Ret);
        return s32Ret;
    }
    stIspCtrlParam.u32StatIntvl = u32FrameRate / 30;

    s32Ret = HI_MPI_ISP_SetCtrlParam(pstViConfig->astViInfo[0].stPipeInfo.aPipe[0], &stIspCtrlParam);
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("HI_MPI_ISP_SetCtrlParam failed with %d!\n", s32Ret);
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_VI_StartVi(pstViConfig);
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_COMM_SYS_Exit();
        SAMPLE_PRT("SAMPLE_COMM_VI_StartVi failed with %d!\n", s32Ret);
        return s32Ret;
    }

    return HI_SUCCESS;
}

HI_S32 SAMPLE_VENC_VPSS_Init(VPSS_GRP VpssGrp, HI_BOOL *pabChnEnable, DYNAMIC_RANGE_E enDynamicRange, PIXEL_FORMAT_E enPixelFormat,
                             SIZE_S stSize, SAMPLE_SNS_TYPE_E enSnsType)
{
    HI_S32 i;
    HI_S32 s32Ret;
    PIC_SIZE_E enSnsSize;
    SIZE_S stSnsSize;
    VPSS_GRP_ATTR_S stVpssGrpAttr = { 0 };
    VPSS_CHN_ATTR_S stVpssChnAttr[VPSS_MAX_PHY_CHN_NUM];

    SAMPLE_PRT("before SAMPLE_COMM_VI_GetSizeBySensor enSnsType: %d\n", enSnsType);

    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(enSnsType, &enSnsSize);
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("SAMPLE_COMM_VI_GetSizeBySensor failed!\n");
        return s32Ret;
    }

    SAMPLE_PRT("after SAMPLE_COMM_VI_GetSizeBySensor enSnsSize: %d\n", enSnsSize);

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enSnsSize, &stSnsSize);
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        return s32Ret;
    }

    SAMPLE_PRT("after SAMPLE_COMM_SYS_GetPicSize stSnsSize: %dx%d\n", stSnsSize.u32Width, stSnsSize.u32Height);

    stVpssGrpAttr.enDynamicRange = enDynamicRange;
    stVpssGrpAttr.enPixelFormat = enPixelFormat;
    stVpssGrpAttr.u32MaxW = stSnsSize.u32Width;
    stVpssGrpAttr.u32MaxH = stSnsSize.u32Height;
    stVpssGrpAttr.stFrameRate.s32SrcFrameRate = -1;
    stVpssGrpAttr.stFrameRate.s32DstFrameRate = -1;
    stVpssGrpAttr.bNrEn = HI_TRUE;
    stVpssGrpAttr.stNrAttr.enNrType = VPSS_NR_TYPE_VIDEO;
    stVpssGrpAttr.stNrAttr.enNrMotionMode = NR_MOTION_MODE_NORMAL;
    stVpssGrpAttr.stNrAttr.enCompressMode = COMPRESS_MODE_FRAME;

    for (i = 0; i < VPSS_MAX_PHY_CHN_NUM; i++) {
        if (HI_TRUE == pabChnEnable[i]) {
            SAMPLE_PRT("set stVpssChnAttr[%d]\n", i);
            stVpssChnAttr[i].u32Width = stSize.u32Width;
            stVpssChnAttr[i].u32Height = stSize.u32Height;
            stVpssChnAttr[i].enChnMode = VPSS_CHN_MODE_USER;
            stVpssChnAttr[i].enCompressMode = COMPRESS_MODE_NONE;  // COMPRESS_MODE_SEG;
            stVpssChnAttr[i].enDynamicRange = enDynamicRange;
            stVpssChnAttr[i].enPixelFormat = enPixelFormat;
            stVpssChnAttr[i].stFrameRate.s32SrcFrameRate = -1;
            stVpssChnAttr[i].stFrameRate.s32DstFrameRate = -1;
            stVpssChnAttr[i].u32Depth = 0;
            stVpssChnAttr[i].bMirror = HI_FALSE;
            stVpssChnAttr[i].bFlip = HI_FALSE;
            stVpssChnAttr[i].enVideoFormat = VIDEO_FORMAT_LINEAR;
            stVpssChnAttr[i].stAspectRatio.enMode = ASPECT_RATIO_NONE;
        }
    }

    s32Ret = SAMPLE_COMM_VPSS_Start(VpssGrp, pabChnEnable, &stVpssGrpAttr, stVpssChnAttr);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("start VPSS fail for %#x!\n", s32Ret);
    }

    return s32Ret;
}

HI_S32 SAMPLE_VENC_CheckSensor(SAMPLE_SNS_TYPE_E enSnsType, SIZE_S stSize)
{
    HI_S32 s32Ret;
    SIZE_S stSnsSize;
    PIC_SIZE_E enSnsSize;

    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(enSnsType, &enSnsSize);
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("SAMPLE_COMM_VI_GetSizeBySensor failed!\n");
        return s32Ret;
    }
    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enSnsSize, &stSnsSize);
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        return s32Ret;
    }

    if ((stSnsSize.u32Width < stSize.u32Width) || (stSnsSize.u32Height < stSize.u32Height)) {
        SAMPLE_PRT("Sensor size is (%d,%d), but encode chnl is (%d,%d) !\n", stSnsSize.u32Width, stSnsSize.u32Height, stSize.u32Width,
                   stSize.u32Height);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

HI_S32 SAMPLE_VENC_ModifyResolution(SAMPLE_SNS_TYPE_E enSnsType, PIC_SIZE_E *penSize, SIZE_S *pstSize)
{
    HI_S32 s32Ret;
    SIZE_S stSnsSize;
    PIC_SIZE_E enSnsSize;

    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(enSnsType, &enSnsSize);
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("SAMPLE_COMM_VI_GetSizeBySensor failed!\n");
        return s32Ret;
    }
    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enSnsSize, &stSnsSize);
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        return s32Ret;
    }

    *penSize = enSnsSize;
    pstSize->u32Width = stSnsSize.u32Width;
    pstSize->u32Height = stSnsSize.u32Height;

    return HI_SUCCESS;
}

/******************************************************************************
 * funciton : get file postfix according palyload_type.
 ******************************************************************************/
HI_S32 HisiLive_COMM_VENC_GetFilePostfix(PAYLOAD_TYPE_E enPayload, char *szFilePostfix)
{
    if (PT_H264 == enPayload) {
        strcpy(szFilePostfix, ".h264");
    } else if (PT_H265 == enPayload) {
        strcpy(szFilePostfix, ".h265");
    } else if (PT_JPEG == enPayload) {
        strcpy(szFilePostfix, ".jpg");
    } else if (PT_MJPEG == enPayload) {
        strcpy(szFilePostfix, ".mjp");
    } else {
        SAMPLE_PRT("payload type err!\n");
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}

/******************************************************************************
 * funciton : save stream
 ******************************************************************************/
HI_S32 HisiLive_COMM_VENC_SaveStream(FILE *pFd, VENC_STREAM_S *pstStream)
{
    HI_S32 i;

    for (i = 0; i < pstStream->u32PackCount; i++) {
        fwrite(pstStream->pstPack[i].pu8Addr + pstStream->pstPack[i].u32Offset,
               pstStream->pstPack[i].u32Len - pstStream->pstPack[i].u32Offset, 1, pFd);

        fflush(pFd);
    }

    return HI_SUCCESS;
}

HI_S32 HisiLive_RTPSendVideo(VENC_STREAM_S *pstStream)
{
    int i;
    gRTPCtx.payload_type = (gParamOption.videoFormat == PT_H264) ? 0 : 1;

    static uint64_t packets = 0;
    ++packets;
    int count10s = gParamOption.frameRate * 10;

    for (i = 0; i < pstStream->u32PackCount; i++) {
        // LOG("packet %d / %d, %lld\n", i + 1, pstStream->u32PackCount, pstStream->pstPack[i].u64PTS);
        gRTPCtx.timestamp = (HI_U32)(pstStream->pstPack[i].u64PTS / 100 * 9);  // (μs / 10^6) * (90 * 10^3)
        if (packets % count10s == 0) {                                         // debug once every 10 seconds
            LOGD("packet pts %llu, rtp ts %u\n", pstStream->pstPack[0].u64PTS, gRTPCtx.timestamp);
        }
        rtpSendH264HEVC(&gRTPCtx, &gUDPCtx,
                        pstStream->pstPack[i].pu8Addr + pstStream->pstPack[i].u32Offset,  // stream ptr
                        pstStream->pstPack[i].u32Len - pstStream->pstPack[i].u32Offset);  // stream length
    }

    return 0;
}

/******************************************************************************
 * funciton : get stream from each channels and save them
 ******************************************************************************/
HI_VOID *HisiLive_COMM_VENC_GetVencStreamProc(HI_VOID *p)
{
    HI_S32 i;
    HI_S32 s32ChnTotal;
    VENC_CHN_ATTR_S stVencChnAttr;
    SAMPLE_VENC_GETSTREAM_PARA_S *pstPara;
    HI_S32 maxfd = 0;
    struct timeval TimeoutVal;
    fd_set read_fds;
    HI_U32 u32PictureCnt[VENC_MAX_CHN_NUM] = { 0 };
    HI_S32 VencFd[VENC_MAX_CHN_NUM];
    HI_CHAR aszFileName[VENC_MAX_CHN_NUM][64];
    FILE *pFile[VENC_MAX_CHN_NUM];
    char szFilePostfix[10];
    VENC_CHN_STATUS_S stStat;
    VENC_STREAM_S stStream;
    HI_S32 s32Ret;
    VENC_CHN VencChn;
    PAYLOAD_TYPE_E enPayLoadType[VENC_MAX_CHN_NUM];
    VENC_STREAM_BUF_INFO_S stStreamBufInfo[VENC_MAX_CHN_NUM];

    prctl(PR_SET_NAME, "GetVencStream", 0, 0, 0);

    pstPara = (SAMPLE_VENC_GETSTREAM_PARA_S *)p;
    s32ChnTotal = pstPara->s32Cnt;
    /******************************************
     step 1:  check & prepare save-file & venc-fd
    ******************************************/
    if (s32ChnTotal >= VENC_MAX_CHN_NUM) {
        SAMPLE_PRT("input count invaild\n");
        return NULL;
    }
    for (i = 0; i < s32ChnTotal; i++) {
        /* decide the stream file name, and open file to save stream */
        VencChn = pstPara->VeChn[i];
        s32Ret = HI_MPI_VENC_GetChnAttr(VencChn, &stVencChnAttr);
        if (s32Ret != HI_SUCCESS) {
            SAMPLE_PRT("HI_MPI_VENC_GetChnAttr chn[%d] failed with %#x!\n", VencChn, s32Ret);
            return NULL;
        }
        enPayLoadType[i] = stVencChnAttr.stVencAttr.enType;

        s32Ret = HisiLive_COMM_VENC_GetFilePostfix(enPayLoadType[i], szFilePostfix);
        if (s32Ret != HI_SUCCESS) {
            SAMPLE_PRT("HisiLive_COMM_VENC_GetFilePostfix [%d] failed with %#x!\n", stVencChnAttr.stVencAttr.enType, s32Ret);
            return NULL;
        }
        if (PT_JPEG != enPayLoadType[i] && gParamOption.mode == MODE_FILE) {
            snprintf(aszFileName[i], 32, "stream_chn%d%s", i, szFilePostfix);

            pFile[i] = fopen(aszFileName[i], "wb");
            if (!pFile[i]) {
                SAMPLE_PRT("open file[%s] failed!\n", aszFileName[i]);
                return NULL;
            }
        }
        /* Set Venc Fd. */
        VencFd[i] = HI_MPI_VENC_GetFd(i);
        if (VencFd[i] < 0) {
            SAMPLE_PRT("HI_MPI_VENC_GetFd failed with %#x!\n", VencFd[i]);
            return NULL;
        }
        if (maxfd <= VencFd[i]) {
            maxfd = VencFd[i];
        }

        s32Ret = HI_MPI_VENC_GetStreamBufInfo(i, &stStreamBufInfo[i]);
        if (HI_SUCCESS != s32Ret) {
            SAMPLE_PRT("HI_MPI_VENC_GetStreamBufInfo failed with %#x!\n", s32Ret);
            return (void *)HI_FAILURE;
        }
    }

    /******************************************
     step 2:  Start to get streams of each channel.
    ******************************************/
    while (HI_TRUE == pstPara->bThreadStart) {
        FD_ZERO(&read_fds);
        for (i = 0; i < s32ChnTotal; i++) {
            FD_SET(VencFd[i], &read_fds);
        }

        TimeoutVal.tv_sec = 2;
        TimeoutVal.tv_usec = 0;
        s32Ret = select(maxfd + 1, &read_fds, NULL, NULL, &TimeoutVal);
        if (s32Ret < 0) {
            SAMPLE_PRT("select failed!\n");
            break;
        } else if (s32Ret == 0) {
            SAMPLE_PRT("get venc stream time out, exit thread\n");
            continue;
        } else {
            for (i = 0; i < s32ChnTotal; i++) {
                if (FD_ISSET(VencFd[i], &read_fds)) {
                    // SAMPLE_PRT("get channel [%d] data.\n", i);
                    /*******************************************************
                     step 2.1 : query how many packs in one-frame stream.
                    *******************************************************/
                    memset(&stStream, 0, sizeof(stStream));

                    s32Ret = HI_MPI_VENC_QueryStatus(i, &stStat);
                    if (HI_SUCCESS != s32Ret) {
                        SAMPLE_PRT("HI_MPI_VENC_QueryStatus chn[%d] failed with %#x!\n", i, s32Ret);
                        break;
                    }

                    /*******************************************************
                    step 2.2 :suggest to check both u32CurPacks and u32LeftStreamFrames at the same time,for example:
                     if(0 == stStat.u32CurPacks || 0 == stStat.u32LeftStreamFrames)
                     {
                        SAMPLE_PRT("NOTE: Current  frame is NULL!\n");
                        continue;
                     }
                    *******************************************************/
                    if (0 == stStat.u32CurPacks) {
                        SAMPLE_PRT("NOTE: Current  frame is NULL!\n");
                        continue;
                    }
                    /*******************************************************
                     step 2.3 : malloc corresponding number of pack nodes.
                    *******************************************************/
                    stStream.pstPack = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
                    if (NULL == stStream.pstPack) {
                        SAMPLE_PRT("malloc stream pack failed!\n");
                        break;
                    }

                    /*******************************************************
                     step 2.4 : call mpi to get one-frame stream
                    *******************************************************/
                    stStream.u32PackCount = stStat.u32CurPacks;
                    s32Ret = HI_MPI_VENC_GetStream(i, &stStream, HI_TRUE);
                    if (HI_SUCCESS != s32Ret) {
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        SAMPLE_PRT("HI_MPI_VENC_GetStream failed with %#x!\n", s32Ret);
                        break;
                    }

                    /*******************************************************
                     step 2.5 : save frame to file
                    *******************************************************/
                    if (PT_JPEG == enPayLoadType[i]) {
                        snprintf(aszFileName[i], 32, "stream_chn%d_%d%s", i, u32PictureCnt[i], szFilePostfix);
                        pFile[i] = fopen(aszFileName[i], "wb");
                        if (!pFile[i]) {
                            SAMPLE_PRT("open file err!\n");
                            return NULL;
                        }
                    }

                    if (gParamOption.mode == MODE_FILE) {
                        s32Ret = HisiLive_COMM_VENC_SaveStream(pFile[i], &stStream);
                    } else if (gParamOption.mode == MODE_RTP) {
                        s32Ret = HisiLive_RTPSendVideo(&stStream);
                    } else {
                        LOGE("Unsupported running mode.\n");
                    }

                    if (HI_SUCCESS != s32Ret) {
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        SAMPLE_PRT("save stream failed!\n");
                        break;
                    }
                    /*******************************************************
                     step 2.6 : release stream
                     *******************************************************/
                    s32Ret = HI_MPI_VENC_ReleaseStream(i, &stStream);
                    if (HI_SUCCESS != s32Ret) {
                        SAMPLE_PRT("HI_MPI_VENC_ReleaseStream failed!\n");
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        break;
                    }

                    /*******************************************************
                     step 2.7 : free pack nodes
                    *******************************************************/
                    free(stStream.pstPack);
                    stStream.pstPack = NULL;
                    u32PictureCnt[i]++;
                    if (PT_JPEG == enPayLoadType[i]) {
                        fclose(pFile[i]);
                    }
                }
            }
        }
    }
    /*******************************************************
     * step 3 : close save-file
     *******************************************************/
    for (i = 0; i < s32ChnTotal; i++) {
        if (PT_JPEG != enPayLoadType[i]) {
            fclose(pFile[i]);
        }
    }
    return NULL;
}

/******************************************************************************
 * funciton : start get venc stream process thread
 ******************************************************************************/
HI_S32 HisiLive_COMM_VENC_StartGetStream(VENC_CHN VeChn)
{
    gMediaProcPara.bThreadStart = HI_TRUE;
    gMediaProcPara.s32Cnt = 1;
    gMediaProcPara.VeChn[0] = VeChn;
    return pthread_create(&gMediaProcPid, 0, HisiLive_COMM_VENC_GetVencStreamProc, (HI_VOID *)&gMediaProcPara);
}

HI_S32 HisiLive_COMM_VENC_CloseReEncode(VENC_CHN VencChn)
{
    HI_S32 s32Ret;
    VENC_RC_PARAM_S stRcParam;
    VENC_CHN_ATTR_S stChnAttr;

    s32Ret = HI_MPI_VENC_GetChnAttr(VencChn, &stChnAttr);
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("GetChnAttr failed!\n");
        return HI_FAILURE;
    }

    s32Ret = HI_MPI_VENC_GetRcParam(VencChn, &stRcParam);
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("GetRcParam failed!\n");
        return HI_FAILURE;
    }

    if (VENC_RC_MODE_H264CBR == stChnAttr.stRcAttr.enRcMode) {
        stRcParam.stParamH264Cbr.s32MaxReEncodeTimes = 0;
    } else if (VENC_RC_MODE_H264VBR == stChnAttr.stRcAttr.enRcMode) {
        stRcParam.stParamH264Vbr.s32MaxReEncodeTimes = 0;
    } else if (VENC_RC_MODE_H265CBR == stChnAttr.stRcAttr.enRcMode) {
        stRcParam.stParamH264Cbr.s32MaxReEncodeTimes = 0;
    } else if (VENC_RC_MODE_H265VBR == stChnAttr.stRcAttr.enRcMode) {
        stRcParam.stParamH264Vbr.s32MaxReEncodeTimes = 0;
    } else {
        return HI_SUCCESS;
    }
    s32Ret = HI_MPI_VENC_SetRcParam(VencChn, &stRcParam);
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("SetRcParam failed!\n");
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}

HI_S32 HisiLive_COMM_VENC_Create(VENC_CHN VencChn, PAYLOAD_TYPE_E enType, PIC_SIZE_E enSize, SAMPLE_RC_E enRcMode, HI_U32 u32Profile,
                                 HI_BOOL bRcnRefShareBuf, VENC_GOP_ATTR_S *pstGopAttr)
{
    HI_S32 s32Ret;
    SIZE_S stPicSize;
    VENC_CHN_ATTR_S stVencChnAttr;
    VENC_ATTR_JPEG_S stJpegAttr;
    SAMPLE_VI_CONFIG_S stViConfig;
    HI_U32 u32FrameRate;
    HI_U32 u32StatTime;
    HI_U32 u32Gop = 10;  // default 30

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enSize, &stPicSize);
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("Get picture size failed!\n");
        return HI_FAILURE;
    }

    SAMPLE_COMM_VI_GetSensorInfo(&stViConfig);
    if (SAMPLE_SNS_TYPE_BUTT == stViConfig.astViInfo[0].stSnsInfo.enSnsType) {
        SAMPLE_PRT("Not set SENSOR%d_TYPE !\n", 0);
        return HI_FALSE;
    }

    u32FrameRate = 30;  // set framerate
    s32Ret = SAMPLE_COMM_VI_GetFrameRateBySensor(stViConfig.astViInfo[0].stSnsInfo.enSnsType, &u32FrameRate);
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("SAMPLE_COMM_VI_GetFrameRateBySensor failed!\n");
        return s32Ret;
    }

    SAMPLE_PRT("Get video frameRate: %d\n", u32FrameRate);

    /******************************************
     step 1:  Create Venc Channel
    ******************************************/
    stVencChnAttr.stVencAttr.enType = enType;
    stVencChnAttr.stVencAttr.u32MaxPicWidth = stPicSize.u32Width;
    stVencChnAttr.stVencAttr.u32MaxPicHeight = stPicSize.u32Height;
    stVencChnAttr.stVencAttr.u32PicWidth = stPicSize.u32Width;                          /*the picture width*/
    stVencChnAttr.stVencAttr.u32PicHeight = stPicSize.u32Height;                        /*the picture height*/
    stVencChnAttr.stVencAttr.u32BufSize = stPicSize.u32Width * stPicSize.u32Height * 2; /*stream buffer size*/
    stVencChnAttr.stVencAttr.u32Profile = u32Profile;
    stVencChnAttr.stVencAttr.bByFrame = HI_TRUE; /*get stream mode is slice mode or frame mode?*/

    if (VENC_GOPMODE_SMARTP == pstGopAttr->enGopMode) {
        u32StatTime = pstGopAttr->stSmartP.u32BgInterval / u32Gop;
    } else {
        u32StatTime = 1;
    }

    switch (enType) {
        case PT_H265: {
            if (SAMPLE_RC_CBR == enRcMode) {
                VENC_H265_CBR_S stH265Cbr;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265CBR;
                stH265Cbr.u32Gop = u32Gop;
                stH265Cbr.u32StatTime = u32StatTime;       /* stream rate statics time(s) */
                stH265Cbr.u32SrcFrameRate = u32FrameRate;  /* input (vi) frame rate */
                stH265Cbr.fr32DstFrameRate = u32FrameRate; /* target frame rate */
                switch (enSize) {
                    case PIC_720P:
                        stH265Cbr.u32BitRate = 1024 * 2 + 1024 * u32FrameRate / 30;
                        break;
                    case PIC_1080P:
                        stH265Cbr.u32BitRate = 1024 * 2 + 2048 * u32FrameRate / 30;
                        break;
                    case PIC_2592x1944:
                        stH265Cbr.u32BitRate = 1024 * 3 + 3072 * u32FrameRate / 30;
                        break;
                    case PIC_3840x2160:
                        stH265Cbr.u32BitRate = 1024 * 5 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_4000x3000:
                        stH265Cbr.u32BitRate = 1024 * 10 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_7680x4320:
                        stH265Cbr.u32BitRate = 1024 * 20 + 5120 * u32FrameRate / 30;
                        break;
                    default:
                        stH265Cbr.u32BitRate = 1024 * 15 + 2048 * u32FrameRate / 30;
                        break;
                }
                memcpy(&stVencChnAttr.stRcAttr.stH265Cbr, &stH265Cbr, sizeof(VENC_H265_CBR_S));
            } else if (SAMPLE_RC_FIXQP == enRcMode) {
                VENC_H265_FIXQP_S stH265FixQp;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265FIXQP;
                stH265FixQp.u32Gop = 30;
                stH265FixQp.u32SrcFrameRate = u32FrameRate;
                stH265FixQp.fr32DstFrameRate = u32FrameRate;
                stH265FixQp.u32IQp = 25;
                stH265FixQp.u32PQp = 30;
                stH265FixQp.u32BQp = 32;
                memcpy(&stVencChnAttr.stRcAttr.stH265FixQp, &stH265FixQp, sizeof(VENC_H265_FIXQP_S));
            } else if (SAMPLE_RC_VBR == enRcMode) {
                VENC_H265_VBR_S stH265Vbr;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265VBR;
                stH265Vbr.u32Gop = u32Gop;
                stH265Vbr.u32StatTime = u32StatTime;
                stH265Vbr.u32SrcFrameRate = u32FrameRate;
                stH265Vbr.fr32DstFrameRate = u32FrameRate;
                switch (enSize) {
                    case PIC_720P:
                        stH265Vbr.u32MaxBitRate = 1024 * 2 + 1024 * u32FrameRate / 30;
                        break;
                    case PIC_1080P:
                        stH265Vbr.u32MaxBitRate = 1024 * 2 + 2048 * u32FrameRate / 30;
                        break;
                    case PIC_2592x1944:
                        stH265Vbr.u32MaxBitRate = 1024 * 3 + 3072 * u32FrameRate / 30;
                        break;
                    case PIC_3840x2160:
                        stH265Vbr.u32MaxBitRate = 1024 * 5 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_4000x3000:
                        stH265Vbr.u32MaxBitRate = 1024 * 10 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_7680x4320:
                        stH265Vbr.u32MaxBitRate = 1024 * 20 + 5120 * u32FrameRate / 30;
                        break;
                    default:
                        stH265Vbr.u32MaxBitRate = 1024 * 15 + 2048 * u32FrameRate / 30;
                        break;
                }
                memcpy(&stVencChnAttr.stRcAttr.stH265Vbr, &stH265Vbr, sizeof(VENC_H265_VBR_S));
            } else if (SAMPLE_RC_AVBR == enRcMode) {
                VENC_H265_AVBR_S stH265AVbr;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265AVBR;
                stH265AVbr.u32Gop = u32Gop;
                stH265AVbr.u32StatTime = u32StatTime;
                stH265AVbr.u32SrcFrameRate = u32FrameRate;
                stH265AVbr.fr32DstFrameRate = u32FrameRate;
                switch (enSize) {
                    case PIC_720P:
                        stH265AVbr.u32MaxBitRate = 1024 * 2 + 1024 * u32FrameRate / 30;
                        break;
                    case PIC_1080P:
                        stH265AVbr.u32MaxBitRate = 1024 * 2 + 2048 * u32FrameRate / 30;
                        break;
                    case PIC_2592x1944:
                        stH265AVbr.u32MaxBitRate = 1024 * 3 + 3072 * u32FrameRate / 30;
                        break;
                    case PIC_3840x2160:
                        stH265AVbr.u32MaxBitRate = 1024 * 5 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_4000x3000:
                        stH265AVbr.u32MaxBitRate = 1024 * 10 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_7680x4320:
                        stH265AVbr.u32MaxBitRate = 1024 * 20 + 5120 * u32FrameRate / 30;
                        break;
                    default:
                        stH265AVbr.u32MaxBitRate = 1024 * 15 + 2048 * u32FrameRate / 30;
                        break;
                }
                memcpy(&stVencChnAttr.stRcAttr.stH265AVbr, &stH265AVbr, sizeof(VENC_H265_AVBR_S));
            } else if (SAMPLE_RC_QVBR == enRcMode) {
                VENC_H265_QVBR_S stH265QVbr;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265QVBR;
                stH265QVbr.u32Gop = u32Gop;
                stH265QVbr.u32StatTime = u32StatTime;
                stH265QVbr.u32SrcFrameRate = u32FrameRate;
                stH265QVbr.fr32DstFrameRate = u32FrameRate;
                switch (enSize) {
                    case PIC_720P:
                        stH265QVbr.u32TargetBitRate = 1024 * 2 + 1024 * u32FrameRate / 30;
                        break;
                    case PIC_1080P:
                        stH265QVbr.u32TargetBitRate = 1024 * 2 + 2048 * u32FrameRate / 30;
                        break;
                    case PIC_2592x1944:
                        stH265QVbr.u32TargetBitRate = 1024 * 3 + 3072 * u32FrameRate / 30;
                        break;
                    case PIC_3840x2160:
                        stH265QVbr.u32TargetBitRate = 1024 * 5 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_4000x3000:
                        stH265QVbr.u32TargetBitRate = 1024 * 10 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_7680x4320:
                        stH265QVbr.u32TargetBitRate = 1024 * 20 + 5120 * u32FrameRate / 30;
                        break;
                    default:
                        stH265QVbr.u32TargetBitRate = 1024 * 15 + 2048 * u32FrameRate / 30;
                        break;
                }
                memcpy(&stVencChnAttr.stRcAttr.stH265QVbr, &stH265QVbr, sizeof(VENC_H265_QVBR_S));
            } else if (SAMPLE_RC_CVBR == enRcMode) {
                VENC_H265_CVBR_S stH265CVbr;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265CVBR;
                stH265CVbr.u32Gop = u32Gop;
                stH265CVbr.u32StatTime = u32StatTime;
                stH265CVbr.u32SrcFrameRate = u32FrameRate;
                stH265CVbr.fr32DstFrameRate = u32FrameRate;
                stH265CVbr.u32LongTermStatTime = 1;
                stH265CVbr.u32ShortTermStatTime = u32StatTime;
                switch (enSize) {
                    case PIC_720P:
                        stH265CVbr.u32MaxBitRate = 1024 * 3 + 1024 * u32FrameRate / 30;
                        stH265CVbr.u32LongTermMaxBitrate = 1024 * 2 + 1024 * u32FrameRate / 30;
                        stH265CVbr.u32LongTermMinBitrate = 512;
                        break;
                    case PIC_1080P:
                        stH265CVbr.u32MaxBitRate = 1024 * 2 + 2048 * u32FrameRate / 30;
                        stH265CVbr.u32LongTermMaxBitrate = 1024 * 2 + 2048 * u32FrameRate / 30;
                        stH265CVbr.u32LongTermMinBitrate = 1024;
                        break;
                    case PIC_2592x1944:
                        stH265CVbr.u32MaxBitRate = 1024 * 4 + 3072 * u32FrameRate / 30;
                        stH265CVbr.u32LongTermMaxBitrate = 1024 * 3 + 3072 * u32FrameRate / 30;
                        stH265CVbr.u32LongTermMinBitrate = 1024 * 2;
                        break;
                    case PIC_3840x2160:
                        stH265CVbr.u32MaxBitRate = 1024 * 8 + 5120 * u32FrameRate / 30;
                        stH265CVbr.u32LongTermMaxBitrate = 1024 * 5 + 5120 * u32FrameRate / 30;
                        stH265CVbr.u32LongTermMinBitrate = 1024 * 3;
                        break;
                    case PIC_4000x3000:
                        stH265CVbr.u32MaxBitRate = 1024 * 12 + 5120 * u32FrameRate / 30;
                        stH265CVbr.u32LongTermMaxBitrate = 1024 * 10 + 5120 * u32FrameRate / 30;
                        stH265CVbr.u32LongTermMinBitrate = 1024 * 4;
                        break;
                    case PIC_7680x4320:
                        stH265CVbr.u32MaxBitRate = 1024 * 24 + 5120 * u32FrameRate / 30;
                        stH265CVbr.u32LongTermMaxBitrate = 1024 * 20 + 5120 * u32FrameRate / 30;
                        stH265CVbr.u32LongTermMinBitrate = 1024 * 6;
                        break;
                    default:
                        stH265CVbr.u32MaxBitRate = 1024 * 24 + 5120 * u32FrameRate / 30;
                        stH265CVbr.u32LongTermMaxBitrate = 1024 * 15 + 2048 * u32FrameRate / 30;
                        stH265CVbr.u32LongTermMinBitrate = 1024 * 5;
                        break;
                }
                memcpy(&stVencChnAttr.stRcAttr.stH265CVbr, &stH265CVbr, sizeof(VENC_H265_CVBR_S));
            } else if (SAMPLE_RC_QPMAP == enRcMode) {
                VENC_H265_QPMAP_S stH265QpMap;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265QPMAP;
                stH265QpMap.u32Gop = u32Gop;
                stH265QpMap.u32StatTime = u32StatTime;
                stH265QpMap.u32SrcFrameRate = u32FrameRate;
                stH265QpMap.fr32DstFrameRate = u32FrameRate;
                stH265QpMap.enQpMapMode = VENC_RC_QPMAP_MODE_MEANQP;
                memcpy(&stVencChnAttr.stRcAttr.stH265QpMap, &stH265QpMap, sizeof(VENC_H265_QPMAP_S));
            } else {
                SAMPLE_PRT("%s,%d,enRcMode(%d) not support\n", __FUNCTION__, __LINE__, enRcMode);
                return HI_FAILURE;
            }
            stVencChnAttr.stVencAttr.stAttrH265e.bRcnRefShareBuf = bRcnRefShareBuf;
        } break;
        case PT_H264: {
            if (SAMPLE_RC_CBR == enRcMode) {
                VENC_H264_CBR_S stH264Cbr;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
                stH264Cbr.u32Gop = u32Gop;                           /*the interval of IFrame*/
                stH264Cbr.u32StatTime = u32StatTime;                 /* stream rate statics time(s) */
                stH264Cbr.u32SrcFrameRate = u32FrameRate;            /* input (vi) frame rate */
                stH264Cbr.fr32DstFrameRate = gParamOption.frameRate; /* target frame rate */
                stH264Cbr.u32BitRate = gParamOption.bitRate;
#if 0
                switch (enSize) {
                    case PIC_720P:
                        stH264Cbr.u32BitRate = 1024 * 3 + 1024 * u32FrameRate / 30;
                        break;
                    case PIC_1080P:
                        stH264Cbr.u32BitRate = 1024 * 2 + 2048 * u32FrameRate / 30;
                        break;
                    case PIC_2592x1944:
                        stH264Cbr.u32BitRate = 1024 * 4 + 3072 * u32FrameRate / 30;
                        break;
                    case PIC_3840x2160:
                        stH264Cbr.u32BitRate = 1024 * 8 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_4000x3000:
                        stH264Cbr.u32BitRate = 1024 * 12 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_7680x4320:
                        stH264Cbr.u32BitRate = 1024 * 24 + 5120 * u32FrameRate / 30;
                        break;
                    default:
                        stH264Cbr.u32BitRate = 1024 * 24 + 5120 * u32FrameRate / 30;
                        break;
                }
#endif
                memcpy(&stVencChnAttr.stRcAttr.stH264Cbr, &stH264Cbr, sizeof(VENC_H264_CBR_S));
            } else if (SAMPLE_RC_FIXQP == enRcMode) {
                VENC_H264_FIXQP_S stH264FixQp;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264FIXQP;
                stH264FixQp.u32Gop = 30;
                stH264FixQp.u32SrcFrameRate = u32FrameRate;
                stH264FixQp.fr32DstFrameRate = u32FrameRate;
                stH264FixQp.u32IQp = 25;
                stH264FixQp.u32PQp = 30;
                stH264FixQp.u32BQp = 32;
                memcpy(&stVencChnAttr.stRcAttr.stH264FixQp, &stH264FixQp, sizeof(VENC_H264_FIXQP_S));
            } else if (SAMPLE_RC_VBR == enRcMode) {
                VENC_H264_VBR_S stH264Vbr;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264VBR;
                stH264Vbr.u32Gop = u32Gop;
                stH264Vbr.u32StatTime = u32StatTime;
                stH264Vbr.u32SrcFrameRate = u32FrameRate;
                stH264Vbr.fr32DstFrameRate = u32FrameRate;
                switch (enSize) {
                    case PIC_360P:
                        stH264Vbr.u32MaxBitRate = 1024 * 2 + 1024 * u32FrameRate / 30;
                        break;
                    case PIC_720P:
                        stH264Vbr.u32MaxBitRate = 1024 * 2 + 1024 * u32FrameRate / 30;
                        break;
                    case PIC_1080P:
                        stH264Vbr.u32MaxBitRate = 1024 * 2 + 2048 * u32FrameRate / 30;
                        break;
                    case PIC_2592x1944:
                        stH264Vbr.u32MaxBitRate = 1024 * 3 + 3072 * u32FrameRate / 30;
                        break;
                    case PIC_3840x2160:
                        stH264Vbr.u32MaxBitRate = 1024 * 5 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_4000x3000:
                        stH264Vbr.u32MaxBitRate = 1024 * 10 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_7680x4320:
                        stH264Vbr.u32MaxBitRate = 1024 * 20 + 5120 * u32FrameRate / 30;
                        break;
                    default:
                        stH264Vbr.u32MaxBitRate = 1024 * 15 + 2048 * u32FrameRate / 30;
                        break;
                }
                memcpy(&stVencChnAttr.stRcAttr.stH264Vbr, &stH264Vbr, sizeof(VENC_H264_VBR_S));
            } else if (SAMPLE_RC_AVBR == enRcMode) {
                VENC_H264_VBR_S stH264AVbr;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264AVBR;
                stH264AVbr.u32Gop = u32Gop;
                stH264AVbr.u32StatTime = u32StatTime;
                stH264AVbr.u32SrcFrameRate = u32FrameRate;
                stH264AVbr.fr32DstFrameRate = u32FrameRate;
                switch (enSize) {
                    case PIC_360P:
                        stH264AVbr.u32MaxBitRate = 1024 * 2 + 1024 * u32FrameRate / 30;
                        break;
                    case PIC_720P:
                        stH264AVbr.u32MaxBitRate = 1024 * 2 + 1024 * u32FrameRate / 30;
                        break;
                    case PIC_1080P:
                        stH264AVbr.u32MaxBitRate = 1024 * 2 + 2048 * u32FrameRate / 30;
                        break;
                    case PIC_2592x1944:
                        stH264AVbr.u32MaxBitRate = 1024 * 3 + 3072 * u32FrameRate / 30;
                        break;
                    case PIC_3840x2160:
                        stH264AVbr.u32MaxBitRate = 1024 * 5 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_4000x3000:
                        stH264AVbr.u32MaxBitRate = 1024 * 10 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_7680x4320:
                        stH264AVbr.u32MaxBitRate = 1024 * 20 + 5120 * u32FrameRate / 30;
                        break;
                    default:
                        stH264AVbr.u32MaxBitRate = 1024 * 15 + 2048 * u32FrameRate / 30;
                        break;
                }
                memcpy(&stVencChnAttr.stRcAttr.stH264AVbr, &stH264AVbr, sizeof(VENC_H264_AVBR_S));
            } else if (SAMPLE_RC_QVBR == enRcMode) {
                VENC_H264_QVBR_S stH264QVbr;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264QVBR;
                stH264QVbr.u32Gop = u32Gop;
                stH264QVbr.u32StatTime = u32StatTime;
                stH264QVbr.u32SrcFrameRate = u32FrameRate;
                stH264QVbr.fr32DstFrameRate = u32FrameRate;
                switch (enSize) {
                    case PIC_360P:
                        stH264QVbr.u32TargetBitRate = 1024 * 2 + 1024 * u32FrameRate / 30;
                        break;
                    case PIC_720P:
                        stH264QVbr.u32TargetBitRate = 1024 * 2 + 1024 * u32FrameRate / 30;
                        break;
                    case PIC_1080P:
                        stH264QVbr.u32TargetBitRate = 1024 * 2 + 2048 * u32FrameRate / 30;
                        break;
                    case PIC_2592x1944:
                        stH264QVbr.u32TargetBitRate = 1024 * 3 + 3072 * u32FrameRate / 30;
                        break;
                    case PIC_3840x2160:
                        stH264QVbr.u32TargetBitRate = 1024 * 5 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_4000x3000:
                        stH264QVbr.u32TargetBitRate = 1024 * 10 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_7680x4320:
                        stH264QVbr.u32TargetBitRate = 1024 * 20 + 5120 * u32FrameRate / 30;
                        break;
                    default:
                        stH264QVbr.u32TargetBitRate = 1024 * 15 + 2048 * u32FrameRate / 30;
                        break;
                }
                memcpy(&stVencChnAttr.stRcAttr.stH264QVbr, &stH264QVbr, sizeof(VENC_H264_QVBR_S));
            } else if (SAMPLE_RC_CVBR == enRcMode) {
                VENC_H264_CVBR_S stH264CVbr;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264CVBR;
                stH264CVbr.u32Gop = u32Gop;
                stH264CVbr.u32StatTime = u32StatTime;
                stH264CVbr.u32SrcFrameRate = u32FrameRate;
                stH264CVbr.fr32DstFrameRate = u32FrameRate;
                stH264CVbr.u32LongTermStatTime = 1;
                stH264CVbr.u32ShortTermStatTime = u32StatTime;
                switch (enSize) {
                    case PIC_720P:
                        stH264CVbr.u32MaxBitRate = 1024 * 3 + 1024 * u32FrameRate / 30;
                        stH264CVbr.u32LongTermMaxBitrate = 1024 * 2 + 1024 * u32FrameRate / 30;
                        stH264CVbr.u32LongTermMinBitrate = 512;
                        break;
                    case PIC_1080P:
                        stH264CVbr.u32MaxBitRate = 1024 * 2 + 2048 * u32FrameRate / 30;
                        stH264CVbr.u32LongTermMaxBitrate = 1024 * 2 + 2048 * u32FrameRate / 30;
                        stH264CVbr.u32LongTermMinBitrate = 1024;
                        break;
                    case PIC_2592x1944:
                        stH264CVbr.u32MaxBitRate = 1024 * 4 + 3072 * u32FrameRate / 30;
                        stH264CVbr.u32LongTermMaxBitrate = 1024 * 3 + 3072 * u32FrameRate / 30;
                        stH264CVbr.u32LongTermMinBitrate = 1024 * 2;
                        break;
                    case PIC_3840x2160:
                        stH264CVbr.u32MaxBitRate = 1024 * 8 + 5120 * u32FrameRate / 30;
                        stH264CVbr.u32LongTermMaxBitrate = 1024 * 5 + 5120 * u32FrameRate / 30;
                        stH264CVbr.u32LongTermMinBitrate = 1024 * 3;
                        break;
                    case PIC_4000x3000:
                        stH264CVbr.u32MaxBitRate = 1024 * 12 + 5120 * u32FrameRate / 30;
                        stH264CVbr.u32LongTermMaxBitrate = 1024 * 10 + 5120 * u32FrameRate / 30;
                        stH264CVbr.u32LongTermMinBitrate = 1024 * 4;
                        break;
                    case PIC_7680x4320:
                        stH264CVbr.u32MaxBitRate = 1024 * 24 + 5120 * u32FrameRate / 30;
                        stH264CVbr.u32LongTermMaxBitrate = 1024 * 20 + 5120 * u32FrameRate / 30;
                        stH264CVbr.u32LongTermMinBitrate = 1024 * 6;
                        break;
                    default:
                        stH264CVbr.u32MaxBitRate = 1024 * 24 + 5120 * u32FrameRate / 30;
                        stH264CVbr.u32LongTermMaxBitrate = 1024 * 15 + 2048 * u32FrameRate / 30;
                        stH264CVbr.u32LongTermMinBitrate = 1024 * 5;
                        break;
                }
                memcpy(&stVencChnAttr.stRcAttr.stH264CVbr, &stH264CVbr, sizeof(VENC_H264_CVBR_S));
            } else if (SAMPLE_RC_QPMAP == enRcMode) {
                VENC_H264_QPMAP_S stH264QpMap;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264QPMAP;
                stH264QpMap.u32Gop = u32Gop;
                stH264QpMap.u32StatTime = u32StatTime;
                stH264QpMap.u32SrcFrameRate = u32FrameRate;
                stH264QpMap.fr32DstFrameRate = u32FrameRate;
                memcpy(&stVencChnAttr.stRcAttr.stH264QpMap, &stH264QpMap, sizeof(VENC_H264_QPMAP_S));
            } else {
                SAMPLE_PRT("%s,%d,enRcMode(%d) not support\n", __FUNCTION__, __LINE__, enRcMode);
                return HI_FAILURE;
            }
            stVencChnAttr.stVencAttr.stAttrH264e.bRcnRefShareBuf = bRcnRefShareBuf;
        } break;
        case PT_MJPEG: {
            if (SAMPLE_RC_FIXQP == enRcMode) {
                VENC_MJPEG_FIXQP_S stMjpegeFixQp;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_MJPEGFIXQP;
                stMjpegeFixQp.u32Qfactor = 95;
                stMjpegeFixQp.u32SrcFrameRate = u32FrameRate;
                stMjpegeFixQp.fr32DstFrameRate = u32FrameRate;

                memcpy(&stVencChnAttr.stRcAttr.stMjpegFixQp, &stMjpegeFixQp, sizeof(VENC_MJPEG_FIXQP_S));
            } else if (SAMPLE_RC_CBR == enRcMode) {
                VENC_MJPEG_CBR_S stMjpegeCbr;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_MJPEGCBR;
                stMjpegeCbr.u32StatTime = u32StatTime;
                stMjpegeCbr.u32SrcFrameRate = u32FrameRate;
                stMjpegeCbr.fr32DstFrameRate = u32FrameRate;
                switch (enSize) {
                    case PIC_360P:
                        stMjpegeCbr.u32BitRate = 1024 * 3 + 1024 * u32FrameRate / 30;
                        break;
                    case PIC_720P:
                        stMjpegeCbr.u32BitRate = 1024 * 5 + 1024 * u32FrameRate / 30;
                        break;
                    case PIC_1080P:
                        stMjpegeCbr.u32BitRate = 1024 * 8 + 2048 * u32FrameRate / 30;
                        break;
                    case PIC_2592x1944:
                        stMjpegeCbr.u32BitRate = 1024 * 20 + 3072 * u32FrameRate / 30;
                        break;
                    case PIC_3840x2160:
                        stMjpegeCbr.u32BitRate = 1024 * 25 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_4000x3000:
                        stMjpegeCbr.u32BitRate = 1024 * 30 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_7680x4320:
                        stMjpegeCbr.u32BitRate = 1024 * 40 + 5120 * u32FrameRate / 30;
                        break;
                    default:
                        stMjpegeCbr.u32BitRate = 1024 * 20 + 2048 * u32FrameRate / 30;
                        break;
                }

                memcpy(&stVencChnAttr.stRcAttr.stMjpegCbr, &stMjpegeCbr, sizeof(VENC_MJPEG_CBR_S));
            } else if ((SAMPLE_RC_VBR == enRcMode) || (SAMPLE_RC_AVBR == enRcMode) || (SAMPLE_RC_QVBR == enRcMode) ||
                       (SAMPLE_RC_CVBR == enRcMode)) {
                VENC_MJPEG_VBR_S stMjpegVbr;

                if (SAMPLE_RC_AVBR == enRcMode) {
                    SAMPLE_PRT("Mjpege not support AVBR, so change rcmode to VBR!\n");
                }

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_MJPEGVBR;
                stMjpegVbr.u32StatTime = u32StatTime;
                stMjpegVbr.u32SrcFrameRate = u32FrameRate;
                stMjpegVbr.fr32DstFrameRate = 5;

                switch (enSize) {
                    case PIC_360P:
                        stMjpegVbr.u32MaxBitRate = 1024 * 3 + 1024 * u32FrameRate / 30;
                        break;
                    case PIC_720P:
                        stMjpegVbr.u32MaxBitRate = 1024 * 5 + 1024 * u32FrameRate / 30;
                        break;
                    case PIC_1080P:
                        stMjpegVbr.u32MaxBitRate = 1024 * 8 + 2048 * u32FrameRate / 30;
                        break;
                    case PIC_2592x1944:
                        stMjpegVbr.u32MaxBitRate = 1024 * 20 + 3072 * u32FrameRate / 30;
                        break;
                    case PIC_3840x2160:
                        stMjpegVbr.u32MaxBitRate = 1024 * 25 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_4000x3000:
                        stMjpegVbr.u32MaxBitRate = 1024 * 30 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_7680x4320:
                        stMjpegVbr.u32MaxBitRate = 1024 * 40 + 5120 * u32FrameRate / 30;
                        break;
                    default:
                        stMjpegVbr.u32MaxBitRate = 1024 * 20 + 2048 * u32FrameRate / 30;
                        break;
                }

                memcpy(&stVencChnAttr.stRcAttr.stMjpegVbr, &stMjpegVbr, sizeof(VENC_MJPEG_VBR_S));
            } else {
                SAMPLE_PRT("cann't support other mode(%d) in this version!\n", enRcMode);
                return HI_FAILURE;
            }
        } break;

        case PT_JPEG:
            stJpegAttr.bSupportDCF = HI_FALSE;
            stJpegAttr.stMPFCfg.u8LargeThumbNailNum = 0;
            stJpegAttr.enReceiveMode = VENC_PIC_RECEIVE_SINGLE;
            memcpy(&stVencChnAttr.stVencAttr.stAttrJpege, &stJpegAttr, sizeof(VENC_ATTR_JPEG_S));
            break;
        default:
            SAMPLE_PRT("cann't support this enType (%d) in this version!\n", enType);
            return HI_ERR_VENC_NOT_SUPPORT;
    }

    if (PT_MJPEG == enType || PT_JPEG == enType) {
        stVencChnAttr.stGopAttr.enGopMode = VENC_GOPMODE_NORMALP;
        stVencChnAttr.stGopAttr.stNormalP.s32IPQpDelta = 0;
    } else {
        memcpy(&stVencChnAttr.stGopAttr, pstGopAttr, sizeof(VENC_GOP_ATTR_S));
        if ((VENC_GOPMODE_BIPREDB == pstGopAttr->enGopMode) && (PT_H264 == enType)) {
            if (0 == stVencChnAttr.stVencAttr.u32Profile) {
                stVencChnAttr.stVencAttr.u32Profile = 1;

                SAMPLE_PRT("H.264 base profile not support BIPREDB, so change profile to main profile!\n");
            }
        }

        if ((VENC_RC_MODE_H264QPMAP == stVencChnAttr.stRcAttr.enRcMode) || (VENC_RC_MODE_H265QPMAP == stVencChnAttr.stRcAttr.enRcMode)) {
            if (VENC_GOPMODE_ADVSMARTP == pstGopAttr->enGopMode) {
                stVencChnAttr.stGopAttr.enGopMode = VENC_GOPMODE_SMARTP;

                SAMPLE_PRT("advsmartp not support QPMAP, so change gopmode to smartp!\n");
            }
        }
    }

    s32Ret = HI_MPI_VENC_CreateChn(VencChn, &stVencChnAttr);
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("HI_MPI_VENC_CreateChn [%d] faild with %#x! ===\n", VencChn, s32Ret);
        return s32Ret;
    }

    s32Ret = HisiLive_COMM_VENC_CloseReEncode(VencChn);
    if (HI_SUCCESS != s32Ret) {
        HI_MPI_VENC_DestroyChn(VencChn);
        return s32Ret;
    }

    return HI_SUCCESS;
}

/******************************************************************************
 * funciton : Start venc stream mode
 * note      : rate control parameter need adjust, according your case.
 ******************************************************************************/
HI_S32 HisiLive_COMM_VENC_Start(VENC_CHN VencChn, PAYLOAD_TYPE_E enType, PIC_SIZE_E enSize, SAMPLE_RC_E enRcMode, HI_U32 u32Profile,
                                HI_BOOL bRcnRefShareBuf, VENC_GOP_ATTR_S *pstGopAttr)
{
    HI_S32 s32Ret;
    VENC_RECV_PIC_PARAM_S stRecvParam;

    /******************************************
     step 1:  Creat Encode Chnl
    ******************************************/
    s32Ret = HisiLive_COMM_VENC_Create(VencChn, enType, enSize, enRcMode, u32Profile, bRcnRefShareBuf, pstGopAttr);
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("SAMPLE_COMM_VENC_Creat faild with%#x! \n", s32Ret);
        return HI_FAILURE;
    }
    /******************************************
     step 2:  Start Recv Venc Pictures
    ******************************************/
    stRecvParam.s32RecvPicNum = -1;
    s32Ret = HI_MPI_VENC_StartRecvFrame(VencChn, &stRecvParam);
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("HI_MPI_VENC_StartRecvPic faild with%#x! \n", s32Ret);
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}

/******************************************************************************
 * function: H.265e + H264e@720P, H.265 Channel resolution adaptable with sensor
 ******************************************************************************/
HI_S32 SAMPLE_VENC_H265_H264(void)
{
    HI_S32 s32Ret;
    SIZE_S stSize;
    PIC_SIZE_E enSize = gParamOption.videoSize;
    VENC_CHN VencChn = 0;
    HI_U32 u32Profile = 0;  // H.264: 0:baseline; 1:MP; 2:HP; 3:SVC-T ; H.265: 0:MP; 1:Main 10 [0 1];
    PAYLOAD_TYPE_E enPayLoad = gParamOption.videoFormat;
    VENC_GOP_MODE_E enGopMode;
    VENC_GOP_ATTR_S stGopAttr;
    SAMPLE_RC_E enRcMode;
    HI_BOOL bRcnRefShareBuf = HI_TRUE;

    VI_DEV ViDev = 0;
    VI_PIPE ViPipe = 0;
    VI_CHN ViChn = 0;
    SAMPLE_VI_CONFIG_S stViConfig;

    VPSS_GRP VpssGrp = 0;
    VPSS_CHN VpssChn = 1;  // use chn 1 for zoom-out, chn 0 for zoom-in only
    HI_BOOL abChnEnable[VPSS_MAX_PHY_CHN_NUM] = { 0, 1, 0 };

    HI_U32 u32SupplementConfig = HI_FALSE;

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enSize, &stSize);
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        return s32Ret;
    }

    SAMPLE_COMM_VI_GetSensorInfo(&stViConfig);
    if (SAMPLE_SNS_TYPE_BUTT == stViConfig.astViInfo[0].stSnsInfo.enSnsType) {
        SAMPLE_PRT("Not set SENSOR%d_TYPE !\n", 0);
        return HI_FAILURE;
    }

    s32Ret = SAMPLE_VENC_CheckSensor(stViConfig.astViInfo[0].stSnsInfo.enSnsType, stSize);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("before SAMPLE_VENC_ModifyResolution\n");
        s32Ret = SAMPLE_VENC_ModifyResolution(stViConfig.astViInfo[0].stSnsInfo.enSnsType, &enSize, &stSize);
        if (s32Ret != HI_SUCCESS) {
            return HI_FAILURE;
        }
    }

    stViConfig.s32WorkingViNum = 1;
    stViConfig.astViInfo[0].stDevInfo.ViDev = ViDev;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[0] = ViPipe;
    stViConfig.astViInfo[0].stChnInfo.ViChn = ViChn;
    stViConfig.astViInfo[0].stChnInfo.enDynamicRange = DYNAMIC_RANGE_SDR8;
    stViConfig.astViInfo[0].stChnInfo.enPixFormat = PIXEL_FORMAT_YVU_SEMIPLANAR_420;

    s32Ret = SAMPLE_VENC_VI_Init(&stViConfig, HI_FALSE, u32SupplementConfig);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("Init VI err for %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    s32Ret = SAMPLE_VENC_VPSS_Init(VpssGrp, abChnEnable, DYNAMIC_RANGE_SDR8, PIXEL_FORMAT_YVU_SEMIPLANAR_420, stSize,
                                   stViConfig.astViInfo[0].stSnsInfo.enSnsType);
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("Init VPSS err for %#x!\n", s32Ret);
        goto EXIT_VI_STOP;
    }

    s32Ret = SAMPLE_COMM_VI_Bind_VPSS(ViPipe, ViChn, VpssGrp);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("VI Bind VPSS err for %#x!\n", s32Ret);
        goto EXIT_VPSS_STOP;
    }

    /******************************************
     start stream venc
     ******************************************/

    // enRcMode = SAMPLE_VENC_GetRcMode();
    enRcMode = SAMPLE_RC_CBR;

    // enGopMode = SAMPLE_VENC_GetGopMode();
    enGopMode = VENC_GOPMODE_NORMALP;
    s32Ret = SAMPLE_COMM_VENC_GetGopAttr(enGopMode, &stGopAttr);
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("Venc Get GopAttr for %#x!\n", s32Ret);
        goto EXIT_VI_VPSS_UNBIND;
    }

    s32Ret = HisiLive_COMM_VENC_Start(VencChn, enPayLoad, enSize, enRcMode, u32Profile, bRcnRefShareBuf, &stGopAttr);
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("Venc Start failed for %#x!\n", s32Ret);
        goto EXIT_VI_VPSS_UNBIND;
    }

    s32Ret = SAMPLE_COMM_VPSS_Bind_VENC(VpssGrp, VpssChn, VencChn);
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("Venc Get GopAttr failed for %#x!\n", s32Ret);
        goto EXIT_VENC_H265_STOP;
    }

    /******************************************
     stream save process
    ******************************************/
    s32Ret = HisiLive_COMM_VENC_StartGetStream(VencChn);
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("Start Venc failed!\n");
        goto EXIT_VENC_H264_UnBind;
    }

    LOGD("please press twice ENTER to exit this sample\n");
    getchar();
    getchar();

    /******************************************
     exit process
    ******************************************/
    HisiLive_COMM_VENC_StopGetStream();

EXIT_VENC_H264_UnBind:
    SAMPLE_COMM_VPSS_UnBind_VENC(VpssGrp, VpssChn, VencChn);
EXIT_VENC_H265_STOP:
    SAMPLE_COMM_VENC_Stop(VencChn);
EXIT_VI_VPSS_UNBIND:
    SAMPLE_COMM_VI_UnBind_VPSS(ViPipe, ViChn, VpssGrp);
EXIT_VPSS_STOP:
    SAMPLE_COMM_VPSS_Stop(VpssGrp, abChnEnable);
EXIT_VI_STOP:
    SAMPLE_COMM_VI_StopVi(&stViConfig);
    SAMPLE_COMM_SYS_Exit();

    return s32Ret;
}

/******************************************************************************
 * function    : main()
 * Description : video venc sample
 ******************************************************************************/

int main(int argc, char *argv[])
{
    HI_S32 s32Ret;
    char logo[200] = { 0 };
    sprintf(logo, "+-------------------------+\n|         HisiLive        |\n|  %s %s   |\n+-------------------------+\n", __DATE__,
            __TIME__);
    GREEN("%s\n", logo);

    writeFile("log.txt", logo, strlen(logo), 0);

    if (HisiLive_ParseParam(argc, argv)) {
        HisiLive_ShowUsage(argv[0]);
        return -1;
    }

    if (gParamOption.mode == MODE_RTP) {
        strcpy(gUDPCtx.dstIp, gParamOption.ip);
        gUDPCtx.dstPort = 1234;
        int res = udpInit(&gUDPCtx);
        if (res) {
            LOGE("udpInit error.\n");
            return -1;
        }

        initRTPMuxContext(&gRTPCtx);
        gRTPCtx.aggregation = 1;  // 1 use Aggregation Unit, 0 Single NALU Unit， default 0.
    }

    s32Ret = SAMPLE_VENC_H265_H264();
    if (HI_SUCCESS == s32Ret) {
        LOGD("program exit normally!\n");
    } else {
        LOGD("program exit abnormally!\n");
    }

    exit(s32Ret);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
