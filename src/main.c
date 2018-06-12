/******************************************************************************
  A simple program of Hisilicon Hi3516A+OV4689 multi-media live implementation.

  Copyright (c) 2018 Liming Shao <lmshao@163.com>
******************************************************************************/
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <arpa/inet.h>

#include "sample_comm.h"
#include "Utils.h"

/************ Global Variables ************/

typedef enum {
    MODE_FILE,
    MODE_RTP,
    MODE_RTSP
}RunMode;

typedef struct {
    RunMode mode;  // -m
    int frameRate;  // -f
    int bitRate;    // -b
    char ip[20];    // -i
    PAYLOAD_TYPE_E videoFormat;  // -e
    PIC_SIZE_E videoSize;   // -s
}ParamOption;

/************ Global Variables ************/
VIDEO_NORM_E gs_enNorm = VIDEO_ENCODING_MODE_NTSC;
ParamOption gParamOption;


/************ Show Usage ************/
void hiliShowUsage(char* sPrgNm)
{
    printf("Usage : %s \n", sPrgNm);
    printf("\t -m: mode: file/rtp, default file.\n");
    printf("\t -e: vedeo decode format, default H.264.\n");
    printf("\t -f: frame rate, default 24 fps.\n");
    printf("\t -b: bitrate, default 1024 kbps.\n");
    printf("\t -i: IP, default 192.168.1.100.\n");
    printf("\t -s: video size: 1080p/720p/D1/CIF, default 1080p\n");
    printf("Default parameters: %s -m file -e 96 -f 24 -b 1024 -s 1080p -i 192.168.1.100\n", sPrgNm);
    return;
}

/************ Parse Parameters ************/
int hiliParseParam(int argc, char**argv){
    int ret = 0, optIndex = 1;
    char *videoSize = "1080p";
    char *format = "H.264";
    char *mode = "file";

    if (argc % 2 == 0)
        return -1;

    // init default parameters
    gParamOption.mode = MODE_FILE;
    gParamOption.frameRate = 24;    // fps
    gParamOption.bitRate = 1024;    // kbps
    sprintf(gParamOption.ip, "%s", "192.168.1.100");
    gParamOption.videoSize = PIC_HD1080;
    gParamOption.videoFormat = PT_H264;       // H.264

    // parse parameters
    while (optIndex < argc && !ret){
        const char *opt = argv[optIndex++];
        int val = 0;
        char *str = NULL;
        
        if (opt[0] == '-' && opt[1] == 'm' && !opt[2]){
            mode = argv[optIndex++];
            if (!strcmp(mode, "file") || !strcmp(mode, "FILE")){
                gParamOption.mode = MODE_FILE;
            } else if (!strcmp(mode, "rtp") || !strcmp(mode, "RTP")){
                gParamOption.mode = MODE_RTP;
            } else {
                printf("mode %s is invalid\n", mode);
                ret = -1;
            }
            continue;
        }

        if (opt[0] == '-' && opt[1] == 'e' && !opt[2]){
            format = argv[optIndex++];
            if (!strstr(format, "264") || !strcmp(format, "AVC") || !strcmp(format, "avc")){
                gParamOption.videoFormat = PT_H264;
            } else if (!strstr(format, "265") || !strcmp(format, "HEVC") || !strcmp(format, "hevc")){
                gParamOption.videoFormat = PT_H265;
            } else {
                printf("VedeoFormat is invalid.\n");
                ret = -1;
            }

            continue;
        }

        if (opt[0] == '-' && opt[1] == 'f' && !opt[2]){
            val = atoi(argv[optIndex++]);
            if (val <= 0 || val > 30){
                printf("frameRate is not in (0, 30]\n");
                ret = -1;
            } else
                gParamOption.frameRate = val;
            continue;
        }

        else if (opt[0] == '-' && opt[1] == 'b' && !opt[2]){
            val = atoi(argv[optIndex++]);
            if (val <= 0 || val > 4096){
                printf("bitRate is not in (0, 4096]\n");
                ret = -1;
            } else
                gParamOption.bitRate = val;
            continue;
        }

        else if (opt[0] == '-' && opt[1] == 'i' && !opt[2]){
            str = argv[optIndex++];
            if (inet_addr(str) == INADDR_NONE){
                printf("IP is invalid.\n");
                ret = -1;
            } else
                sprintf(gParamOption.ip, "%s", str);
            continue;
        }

        else if (opt[0] == '-' && opt[1] == 's' && !opt[2]){
            videoSize = argv[optIndex++];
            if (!strcmp(videoSize, "1080p") || !strcmp(videoSize, "1080P")){
                gParamOption.videoSize = PIC_HD1080;
            } else if (!strcmp(videoSize, "720p") || !strcmp(videoSize, "720P")){
                gParamOption.videoSize = PIC_HD720;
            } else if (!strcmp(videoSize, "D1") || !strcmp(videoSize, "d1")){
                gParamOption.videoSize = PIC_D1;
            } else if (!strcmp(videoSize, "CIF") || !strcmp(videoSize, "cif")){
                gParamOption.videoSize = PIC_CIF;
            } else {
                printf("VedeoSize is invalid.\n");
                ret = -1;
            }
            continue;
        }

        else {
            printf("param [%s] is invalid.\n", opt);
            ret = -1;
        }
    }

    printf("param:\nmode=%s, format=%s, frameRate=%d fps, bitRate=%d kbps, videoSize=%s, IP=%s\n",
           mode, format, gParamOption.frameRate,
           gParamOption.bitRate, videoSize, gParamOption.ip);

    return ret;
}

/******************************************************************************
* function : to process abnormal case
******************************************************************************/
void SAMPLE_VENC_HandleSig(HI_S32 signo)
{
    if (SIGINT == signo || SIGTERM == signo)
    {
        SAMPLE_COMM_ISP_Stop();
        SAMPLE_COMM_SYS_Exit();
        LOGE("program termination abnormally!\n");
    }
    exit(-1);
}

/******************************************************************************
* funciton : get stream from each channels and save them
******************************************************************************/
HI_VOID* hiliGetVencStreamProc(HI_VOID* p)
{
    HI_S32 i=0;
    // HI_S32 s32ChnTotal;
    SAMPLE_VENC_GETSTREAM_PARA_S* pstPara;
    HI_S32 maxfd = 0;
    struct timeval TimeoutVal;
    fd_set read_fds;
    HI_S32 VencFd;
    HI_CHAR aszFileName[FILE_NAME_LEN];
    FILE* pFile;

    VENC_CHN_STAT_S stStat;
    VENC_STREAM_S stStream;
    HI_S32 s32Ret;
    VENC_CHN VencChn = 0;

    pstPara = (SAMPLE_VENC_GETSTREAM_PARA_S*)p;
    // s32ChnTotal = pstPara->s32Cnt;

    /******************************************
     step 1:  check & prepare save-file & venc-fd
    ******************************************/
    // if (s32ChnTotal >= VENC_MAX_CHN_NUM)
    // {
    //     LOGE("input count invaild\n");
    //     return NULL;
    // }

    /* decide the stream file name, and open file to save stream */
    if (gParamOption.mode == MODE_FILE) {
        if (gParamOption.videoFormat == PT_H264) {
            sprintf(aszFileName, "stream_%s.h264", getCurrentTime());
        } else if (gParamOption.videoFormat == PT_H265){
            sprintf(aszFileName, "stream_%s.h265", getCurrentTime());
        } else {
            LOGE("Video Format is invalid.\n");
            return NULL;
        }

        pFile = fopen(aszFileName, "wb");
        if (!pFile) {
            LOGE("open file[%s] failed!\n", aszFileName);
            return NULL;
        }
    }

    /* Set Venc Fd. */
    VencFd = HI_MPI_VENC_GetFd(i);
    if (VencFd < 0) {
        LOGE("HI_MPI_VENC_GetFd failed with %#x!\n", VencFd);
        return NULL;
    }
    
    if (maxfd <= VencFd) {
            maxfd = VencFd;
    }

    /******************************************
     step 2:  Start to get streams of each channel.
    ******************************************/
    while (HI_TRUE == pstPara->bThreadStart)
    {
        FD_ZERO(&read_fds);

        FD_SET(VencFd, &read_fds);

        TimeoutVal.tv_sec  = 2;
        TimeoutVal.tv_usec = 0;
        s32Ret = select(maxfd + 1, &read_fds, NULL, NULL, &TimeoutVal);
        if (s32Ret < 0) {
            LOGE("select failed!\n");
            break;
        } 
        else if (s32Ret == 0) {
            LOGE("get venc stream time out, exit thread\n");
            continue;
        }
        else {
            if (FD_ISSET(VencFd, &read_fds)) {
                /*******************************************************
                 step 2.1 : query how many packs in one-frame stream.
                *******************************************************/
                memset(&stStream, 0, sizeof(stStream));
                s32Ret = HI_MPI_VENC_Query(VencChn, &stStat);
                if (HI_SUCCESS != s32Ret) {
                    LOGE("HI_MPI_VENC_Query failed with %#x!\n", s32Ret);
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
                if(0 == stStat.u32CurPacks) {
                    LOGE("NOTE: Current  frame is NULL!\n");
                    continue;
                }
                /*******************************************************
                 step 2.3 : malloc corresponding number of pack nodes.
                *******************************************************/
                stStream.pstPack = (VENC_PACK_S*)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
                if (NULL == stStream.pstPack) {
                    LOGE("malloc stream pack failed!\n");
                    break;
                }

                /*******************************************************
                 step 2.4 : call mpi to get one-frame stream
                *******************************************************/
                stStream.u32PackCount = stStat.u32CurPacks;
                s32Ret = HI_MPI_VENC_GetStream(VencChn, &stStream, HI_TRUE);
                if (HI_SUCCESS != s32Ret) {
                    free(stStream.pstPack);
                    stStream.pstPack = NULL;
                    LOGE("HI_MPI_VENC_GetStream failed with %#x!\n", s32Ret);
                    break;
                }

                /*******************************************************
                 step 2.5 : save frame to file
                *******************************************************/
                s32Ret = SAMPLE_COMM_VENC_SaveStream(gParamOption.videoFormat, pFile, &stStream);
                if (HI_SUCCESS != s32Ret) {
                    free(stStream.pstPack);
                    stStream.pstPack = NULL;
                    LOGE("save stream failed!\n");
                    break;
                }
                /*******************************************************
                 step 2.6 : release stream
                *******************************************************/
                s32Ret = HI_MPI_VENC_ReleaseStream(i, &stStream);
                if (HI_SUCCESS != s32Ret) {
                    free(stStream.pstPack);
                    stStream.pstPack = NULL;
                    break;
                }
                
                /*******************************************************
                 step 2.7 : free pack nodes
                *******************************************************/
                free(stStream.pstPack);
                stStream.pstPack = NULL;
            }
        }
    }

    /*******************************************************
    * step 3 : close save-file
    *******************************************************/
    fclose(pFile);

    return NULL;
}


/******************************************************************************
* function :  H.264@1080p@30fps+H.265@1080p@30fps+H.264@D1@30fps
******************************************************************************/
HI_S32 SAMPLE_VENC_1080P_CLASSIC(HI_VOID)
{
    HI_U32 u32Profile = 0;

    VB_CONF_S stVbConf;
    SAMPLE_VI_CONFIG_S stViConfig = {0};

    VPSS_GRP VpssGrp;
    VPSS_CHN VpssChn;
    VPSS_GRP_ATTR_S stVpssGrpAttr;
    VPSS_CHN_ATTR_S stVpssChnAttr;
    VPSS_CHN_MODE_S stVpssChnMode;

    VENC_CHN VencChn;
    SAMPLE_RC_E enRcMode = SAMPLE_RC_CBR;   // or SAMPLE_RC_VBR

    HI_S32 s32ChnNum = 1; // use 1 channel video

    HI_S32 s32Ret = HI_SUCCESS;
    HI_U32 u32BlkSize;
    SIZE_S stSize;

    SAMPLE_VENC_GETSTREAM_PARA_S stPara;
    pthread_t vencPid;

    /******************************************
     step  1: init sys variable
    ******************************************/
    memset(&stVbConf, 0, sizeof(VB_CONF_S));
    stVbConf.u32MaxPoolCnt = 128;

    /*calculate VB Block size of picture.*/
    u32BlkSize = SAMPLE_COMM_SYS_CalcPicVbBlkSize(gs_enNorm, \
                 gParamOption.videoSize, SAMPLE_PIXEL_FORMAT, SAMPLE_SYS_ALIGN_WIDTH);
    stVbConf.astCommPool[0].u32BlkSize = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt = 8;

    /******************************************
     step 2: mpp system init.
    ******************************************/
    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    if (HI_SUCCESS != s32Ret)
    {
        LOGE("system init failed with %d!\n", s32Ret);
        goto END_VENC_1080P_CLASSIC_0;
    }

    /******************************************
     step 3: start vi dev & chn to capture
    ******************************************/
    stViConfig.enViMode   = SENSOR_TYPE;  // Defined in Makefile
    stViConfig.enRotate   = ROTATE_NONE;
    stViConfig.enNorm     = VIDEO_ENCODING_MODE_AUTO;
    stViConfig.enViChnSet = VI_CHN_SET_NORMAL;
    stViConfig.enWDRMode  = WDR_MODE_NONE;
    s32Ret = SAMPLE_COMM_VI_StartVi(&stViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        LOGE("start vi failed!\n");
        goto END_VENC_1080P_CLASSIC_1;
    }

    /******************************************
     step 4: start vpss and vi bind vpss
    ******************************************/
    s32Ret = SAMPLE_COMM_SYS_GetPicSize(gs_enNorm, gParamOption.videoSize, &stSize);
    if (HI_SUCCESS != s32Ret)
    {
        LOGE("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        goto END_VENC_1080P_CLASSIC_1;
    }

    VpssGrp = 0;
    stVpssGrpAttr.u32MaxW = stSize.u32Width;
    stVpssGrpAttr.u32MaxH = stSize.u32Height;
    stVpssGrpAttr.bIeEn = HI_FALSE;
    stVpssGrpAttr.bNrEn = HI_TRUE;
    stVpssGrpAttr.bHistEn = HI_FALSE;
    stVpssGrpAttr.bDciEn = HI_FALSE;
    stVpssGrpAttr.enDieMode = VPSS_DIE_MODE_NODIE;
    stVpssGrpAttr.enPixFmt = SAMPLE_PIXEL_FORMAT;

    s32Ret = SAMPLE_COMM_VPSS_StartGroup(VpssGrp, &stVpssGrpAttr);
    if (HI_SUCCESS != s32Ret)
    {
        LOGE("Start Vpss failed!\n");
        goto END_VENC_1080P_CLASSIC_2;
    }

    s32Ret = SAMPLE_COMM_VI_BindVpss(stViConfig.enViMode);
    if (HI_SUCCESS != s32Ret)
    {
        LOGE("Vi bind Vpss failed!\n");
        goto END_VENC_1080P_CLASSIC_3;
    }

    VpssChn = 0;
    stVpssChnMode.enChnMode      = VPSS_CHN_MODE_USER;
    stVpssChnMode.bDouble        = HI_FALSE;
    stVpssChnMode.enPixelFormat  = SAMPLE_PIXEL_FORMAT;
    stVpssChnMode.u32Width       = stSize.u32Width;
    stVpssChnMode.u32Height      = stSize.u32Height;
    stVpssChnMode.enCompressMode = COMPRESS_MODE_SEG;
    memset(&stVpssChnAttr, 0, sizeof(stVpssChnAttr));
    stVpssChnAttr.s32SrcFrameRate = -1;
    stVpssChnAttr.s32DstFrameRate = -1;
    s32Ret = SAMPLE_COMM_VPSS_EnableChn(VpssGrp, VpssChn, &stVpssChnAttr, &stVpssChnMode, HI_NULL);
    if (HI_SUCCESS != s32Ret)
    {
        LOGE("Enable vpss chn failed!\n");
        goto END_VENC_1080P_CLASSIC_4;
    }

    /******************************************
     step 5: start stream venc
    ******************************************/
    VpssGrp = 0;
    VpssChn = 0;
    VencChn = 0;
    s32Ret = SAMPLE_COMM_VENC_Start(VencChn, gParamOption.videoFormat, \
                                    gs_enNorm, gParamOption.videoSize, enRcMode, u32Profile);
    if (HI_SUCCESS != s32Ret)
    {
        LOGE("Start Venc failed!\n");
        goto END_VENC_1080P_CLASSIC_5;
    }

    s32Ret = SAMPLE_COMM_VENC_BindVpss(VencChn, VpssGrp, VpssChn);
    if (HI_SUCCESS != s32Ret)
    {
        LOGE("Start Venc failed!\n");
        goto END_VENC_1080P_CLASSIC_5;
    }

    /******************************************
     step 6: stream venc process -- get stream, then save it to file.
    ******************************************/
    stPara.bThreadStart = HI_TRUE;
    stPara.s32Cnt = s32ChnNum;

    s32Ret = pthread_create(&vencPid, 0, hiliGetVencStreamProc, (HI_VOID*)&stPara);

    if (HI_SUCCESS != s32Ret)
    {
        LOGE("Start Venc failed!\n");
        goto END_VENC_1080P_CLASSIC_5;
    }

    GREEN("please press twice ENTER to exit this sample\n");
    getchar();
    getchar();

    /******************************************
     step 7: exit process
    ******************************************/
    // SAMPLE_COMM_VENC_StopGetStream();

    if (HI_TRUE == stPara.bThreadStart)
    {
        stPara.bThreadStart = HI_FALSE;
        pthread_join(vencPid, 0);
    }
    
END_VENC_1080P_CLASSIC_5:
    VpssGrp = 0;

    VpssChn = 0;
    VencChn = 0;
    SAMPLE_COMM_VENC_UnBindVpss(VencChn, VpssGrp, VpssChn);
    SAMPLE_COMM_VENC_Stop(VencChn);
    SAMPLE_COMM_VI_UnBindVpss(stViConfig.enViMode);

END_VENC_1080P_CLASSIC_4:	//vpss stop
    VpssGrp = 0;
    VpssChn = 0;
    SAMPLE_COMM_VPSS_DisableChn(VpssGrp, VpssChn);

END_VENC_1080P_CLASSIC_3:    //vpss stop
    SAMPLE_COMM_VI_UnBindVpss(stViConfig.enViMode);
END_VENC_1080P_CLASSIC_2:    //vpss stop
    SAMPLE_COMM_VPSS_StopGroup(VpssGrp);
END_VENC_1080P_CLASSIC_1:	//vi stop
    SAMPLE_COMM_VI_StopVi(&stViConfig);
END_VENC_1080P_CLASSIC_0:	//system exit
    SAMPLE_COMM_SYS_Exit();

    return s32Ret;
}

/******************************************************************************
* function    : main()
* Description : video venc sample
******************************************************************************/
int main(int argc, char* argv[])
{
    int res = 0;
    
    GREEN("+-------------------------+\n");
    GREEN("|         HisiLive        |\n");
    GREEN("|  %s %s   |\n", __DATE__, __TIME__);
    GREEN("+-------------------------+\n");

    res = hiliParseParam(argc, argv);
    if (res){
        hiliShowUsage(argv[0]);
        return -1;
    }

    signal(SIGINT, SAMPLE_VENC_HandleSig);
    signal(SIGTERM, SAMPLE_VENC_HandleSig);

    /* H.264@1080p@30fps+H.265@1080p@30fps+H.264@D1@30fps */
    res = SAMPLE_VENC_1080P_CLASSIC();
    if (res) { 
        RED("program exit abnormally!\n"); 
    } else {
        GREEN("program exit normally!\n");
    }

    return res;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
