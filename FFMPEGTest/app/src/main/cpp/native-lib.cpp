#include <jni.h>
#include <string>
#include <time.h>
#ifdef ANDROID
#include <android/log.h>
#define LOGE(format, ...)  __android_log_print(ANDROID_LOG_ERROR, "(>_<)", format, ##__VA_ARGS__)
#define LOGI(format, ...)  __android_log_print(ANDROID_LOG_INFO,  "(^_^)", format, ##__VA_ARGS__)
#else
#define LOGE(format, ...)  printf("(>_<) " format "\n", ##__VA_ARGS__)
#define LOGI(format, ...)  printf("(^_^) " format "\n", ##__VA_ARGS__)
#endif

extern "C"{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavfilter/avfilter.h"
#include "libswscale/swscale.h"
#include <libavutil/imgutils.h>
#include "libavutil/log.h"
};

const char *J_CLASS_NAME = "com/bignerdranch/android/ffmpegtest/MainActivity";
jclass mainActivity;
jmethodID onDecoder;
jbyteArray result;
unsigned char *ydata;
unsigned char *udata;
unsigned char *vdata;
unsigned char *uvdata;
int64_t global_video_pkt_pts = AV_NOPTS_VALUE;

//Output FFMPEG's av_log()
void custom_log(void *ptr, int level, const char* fmt, va_list vl){
    FILE *fp = fopen("/storage/emulated/0/av_log.txt","a+");
    if(fp){
        vfprintf(fp,fmt,vl);
        fflush(fp);
        fclose(fp);
    }
}

int our_get_buffer(struct AVCodecContext *c, AVFrame *pic, int flags){
    int ret = avcodec_default_get_buffer2(c, pic, 0);
    int64_t *pts = (int64_t *)av_malloc(sizeof(int64_t));
    *pts = global_video_pkt_pts;
    pic->opaque = pts;
    LOGI("init pic->opaque, global_pts = ");
    return ret;
}

extern "C"
jbyteArray Java_com_bignerdranch_android_ffmpegtest_MainActivity_decode
        (JNIEnv* env,jobject obj/* this */,jstring input_jstr){
    mainActivity = env->FindClass(J_CLASS_NAME);

    onDecoder = env->GetMethodID(mainActivity,"onDecoder","([BIIIZD)V");
    //AVFormatContext是FFMPEG里非常重要的一个结构，是其他输入输出相关信息的一个容器，主要用于处理封装格式（FLV/MP4/RMVB等）
    AVFormatContext *pFormatCtx;
    int i,videoindex;
    AVCodecContext *pCodecCtx;
    //AVCodec是存储编解码器信息的结构体
    AVCodec *pCodec;
    //AVFrame结构体一般用于存储原始数据（YUV，PCM）
    AVFrame *pFrame,*pFrameYUV;
    uint8_t *out_buffer;
    //AVPacket是存储压缩编码后的数据的结构体，比如说对于H.264，1个AVPacket的data通常对应一个NAL
    AVPacket *packet;
    double pts = 0;
    int y_size;
    int ret,got_picture;
    struct SwsContext *img_convert_ctx;
//    FILE *fp_yuv = NULL;
    int frame_cnt;
    clock_t time_start,time_finish;
    double time_duration = 0.0;

    char input_str[500] = {0};
    char output_str[500] = {0};
    char info[1000] = {0};
    //获取源文件的绝对路径以及输出文件的路径信息
    sprintf(input_str,"%s",env->GetStringUTFChars(input_jstr,NULL));
//    sprintf(output_str,"%s",env->GetStringUTFChars(output_jstr,NULL));

    av_log_set_callback(custom_log);
    //初始化libavformat并注册所有的编解码器信息和相关协议
    av_register_all();
    avformat_network_init();
    pFormatCtx = avformat_alloc_context();

    if(avformat_open_input(&pFormatCtx,input_str,NULL,NULL) != 0){
        LOGE("Couldn't open input stream.\n");

        return NULL;
    }
    if(avformat_find_stream_info(pFormatCtx,NULL) < 0){
        LOGE("Couldn't find stream information.\n");
        return NULL;
    }
    videoindex = -1;

    //nb_streams 音视频流的数量
    for(i = 0; i < pFormatCtx->nb_streams; i++){
        if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO){
            videoindex = i;
            break;
        }
    }
    if(videoindex == -1){
        LOGE("Couldn't find a video stream.\n");
        return NULL;
    }
    pCodecCtx = pFormatCtx->streams[videoindex]->codec;
    pCodecCtx->get_buffer2 = our_get_buffer;
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if(pCodec == NULL){
        LOGE("Couldn't find Codec.\n");
        return NULL;
    }
    if(avcodec_open2(pCodecCtx,pCodec,NULL) < 0){
        LOGE("Couldn't open codec.\n");
        return NULL;
    }

    pFrame = av_frame_alloc();
    pFrameYUV = av_frame_alloc();
    out_buffer = (unsigned char *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P,pCodecCtx->width,pCodecCtx->height,1));

    //av_frame_alloc并没有给像素分配内存空间，所以需要调用av_image_fill_arrays方法进行像素数据的空间分配
    av_image_fill_arrays(pFrameYUV->data,pFrameYUV->linesize,out_buffer,AV_PIX_FMT_YUV420P,pCodecCtx->width,pCodecCtx->height,1);

    packet = (AVPacket *)av_malloc(sizeof(AVPacket));

    //libswscale是一个主要用于处理图片像素数据的类库。可以完成图片像素格式的转换，图片的拉伸等工作。
    //常用的函数通常有3个：
    //sws_getContext()初始化一个SwsContext
    //sws_scale()处理图像数据
    //sws_freeContext()释放一个SwsContext
    img_convert_ctx = sws_getContext(pCodecCtx->width,pCodecCtx->height,pCodecCtx->pix_fmt,pCodecCtx->width,pCodecCtx->height,AV_PIX_FMT_YUV420P,SWS_BICUBIC,NULL,NULL,NULL);

    sprintf(info,  "[Input     ]%s\n", input_str);
    sprintf(info,"%s[Output    ]%s\n", info, output_str);
    sprintf(info,"%s[Format    ]%s\n", info, pFormatCtx->iformat->name);
    sprintf(info,"%s[Codec     ]%s\n", info, pCodecCtx->codec->name);
    sprintf(info,"%s[Resolution]%dx%d",info, pCodecCtx->width,pCodecCtx->height);

//    LOGI("%s",output_str);
//    fp_yuv = fopen("/storage/emulated/0/test.yuv","wb+");
//    if(fp_yuv == NULL){
//        LOGE("Cannot open output file.\n");
//        return NULL;
//    }

    frame_cnt = 0;
    time_start = clock();

// int av_read_frame(AVFormatContext *s, AVPacket *pkt);
// 从输入源文件容器中读取一个AVPacket数据包
// 该函数读出的包并不每次都是有效的,对于读出的包我们都应该进行相应的解码(视频解码/音频解码),
// 在返回值>=0时,循环调用该函数进行读取,循环调用之前请调用av_free_packet函数清理AVPacket
// AVPacket中包含的pts和dts是指视频帧需要显示和解码的顺序，每增加一帧就加一，而不是播放视频的时间戳
    while(av_read_frame(pFormatCtx,packet) >= 0){
        if(packet->stream_index == videoindex){
            global_video_pkt_pts = packet->pts;
            ret = avcodec_decode_video2(pCodecCtx,pFrame,&got_picture,packet);
            if(ret < 0){
                printf("Decode Error.\n");
                return NULL;
            }
//            LOGI("pFrame->opaque = %lld", *(uint64_t*)pFrame->opaque);
            if(packet->dts != AV_NOPTS_VALUE && pFrame->opaque && *(int64_t*)pFrame->opaque != AV_NOPTS_VALUE){
                LOGI("pts = *(uint64_t*)pFrame->opaque;");
                pts = *(int64_t*)pFrame->opaque;
            } else if(packet->dts != AV_NOPTS_VALUE){
                LOGI("pts = packet->pts");
                pts = packet->dts;
            } else{
                LOGI("pts = 0");
                pts = 0;
            }
            LOGI("pts = %f time_base = %f", pts, av_q2d(pFormatCtx->streams[videoindex]->time_base));
            pts *= av_q2d(pFormatCtx->streams[videoindex]->time_base);
            LOGI("calculate pts = %f got_picture = %d", pts, got_picture);
            if(got_picture){
                sws_scale(img_convert_ctx,(const uint8_t* const*)pFrame->data,pFrame->linesize,0,pCodecCtx->height,pFrameYUV->data,pFrameYUV->linesize);
                y_size = pCodecCtx->width*pCodecCtx->height;
                if(NULL == result){
                    result = env->NewByteArray(y_size*3/2);
                }
                ydata = pFrameYUV->data[0];
                udata = pFrameYUV->data[1];
                vdata = pFrameYUV->data[2];
                env->SetByteArrayRegion(result,0,y_size,(jbyte *)ydata);
                env->SetByteArrayRegion(result,y_size,y_size/4,(jbyte *)udata);
                env->SetByteArrayRegion(result,y_size*5/4,y_size/4,(jbyte *)vdata);
//                LOGI("packet->pts = %lld packet->dts = %lld", packet->pts, packet->dts);
//                LOGI("pFrame->width = %d  pFrame->height = %d pCodecCtx->width = %d pCodecCtx->height = %d pFrameYUV->pts = %lld pFrame->pts = %lld",pFrame->width,pFrame->height, pCodecCtx->width, pCodecCtx->height, pFrameYUV->pts, pFrame->pts);
                env->CallVoidMethod(obj, onDecoder, result, pFrame->width, pFrame->height, 0, false, pts*1000000000);
//                LOGI(""+pFrameYUV->pts);
//                fwrite(pFrameYUV->data[0],1,y_size,fp_yuv);    //Y
//                fwrite(pFrameYUV->data[1],1,y_size/4,fp_yuv);  //U
//                fwrite(pFrameYUV->data[2],1,y_size/4,fp_yuv);  //V

                //output info
                char pictype_str[10] = {0};
                switch(pFrame->pict_type){
                    case AV_PICTURE_TYPE_I:sprintf(pictype_str,"I");break;
                    case AV_PICTURE_TYPE_P:sprintf(pictype_str,"P");break;
                    case AV_PICTURE_TYPE_B:sprintf(pictype_str,"B");break;
                    default:sprintf(pictype_str,"Other");break;
                }
                frame_cnt++;
//                LOGI("decoding...");
            }
        }
        av_free_packet(packet);
    }

    //flush decoder
    //FIX: flush frames remained in Codec
    while(1) {
        global_video_pkt_pts = packet->pts;
        ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
        if(packet->dts != AV_NOPTS_VALUE && pFrame->opaque && *(uint64_t*)pFrame->opaque != AV_NOPTS_VALUE){
            pts = *(uint64_t*)pFrame->opaque;
        } else if(packet->dts != AV_NOPTS_VALUE){
            pts = packet->dts;
        } else{
            pts = 0;
        }
        pts *= av_q2d(pCodecCtx->time_base);
        if (ret < 0) {
            break;
        }
        if (!got_picture) {
            break;
        }
        sws_scale(img_convert_ctx, (const uint8_t *const *) pFrame->data, pFrame->linesize, 0,
                  pCodecCtx->height,
                  pFrameYUV->data, pFrameYUV->linesize);
        y_size = pCodecCtx->width * pCodecCtx->height;
        if(NULL == result){
            result = env->NewByteArray(y_size*3/2);
        }
        ydata = pFrameYUV->data[0];
        udata = pFrameYUV->data[1];
        vdata = pFrameYUV->data[2];
        env->SetByteArrayRegion(result,0,y_size,(jbyte *)ydata);
        env->SetByteArrayRegion(result,y_size,y_size/4,(jbyte *)udata);
        env->SetByteArrayRegion(result,y_size*5/4,y_size/4,(jbyte *)vdata);
//        env->CallNonvirtualVoidMethod(obj, mainActivity, onDecoder, result, pFrameYUV->width, pFrameYUV->height, 0, false, pFrameYUV->pts);
        env->CallVoidMethod(obj, onDecoder, result, pFrame->width, pFrame->height, 0, false, pts*1000000);
//        fwrite(pFrameYUV->data[0], 1, y_size, fp_yuv);    //Y
//        fwrite(pFrameYUV->data[1], 1, y_size / 4, fp_yuv);  //U
//        fwrite(pFrameYUV->data[2], 1, y_size / 4, fp_yuv);  //V


        //Output info
        char pictype_str[10] = {0};
        switch (pFrame->pict_type) {
            case AV_PICTURE_TYPE_I:
                sprintf(pictype_str, "I");
                break;
            case AV_PICTURE_TYPE_P:
                sprintf(pictype_str, "P");
                break;
            case AV_PICTURE_TYPE_B:
                sprintf(pictype_str, "B");
                break;
            default:
                sprintf(pictype_str, "Other");
                break;
        }
        frame_cnt++;
        LOGI("almost done");
    }
//    y = env->GetByteArrayElements(result,0);
//    u = env->GetByteArrayElements(result,0);
//    v = env->GetByteArrayElements(result,0);
//    int m,n;
//    for(m = 0; m < y_size; m++){
//        y[m] = pFrameYUV->data[0][m];
//    }
//    for(n = 0; n < y_size/4; n++){
//        u[n] = pFrameYUV->data[1][n];
//        v[n] = pFrameYUV->data[2][n];
//    }

    time_finish = clock();
    time_duration = (double)(time_finish-time_start);
    sprintf(info, "%s[Time      ]%fms\n",info,time_duration);
    sprintf(info, "%s[Count     ]%d\n",info,frame_cnt);

    sws_freeContext(img_convert_ctx);
//    fclose(fp_yuv);
    av_frame_free(&pFrameYUV);
    av_frame_free(&pFrame);
    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);

    LOGI("free has done");
    return result;
}

