/**
 *
 * Created by 公众号：字节流动 on 2021/3/16.
 * https://github.com/githubhaohao/LearnFFmpeg
 * 最新文章首发于公众号：字节流动，有疑问或者技术交流可以添加微信 Byte-Flow ,领取视频教程, 拉你进技术交流群
 *
 * */

#include <cstdio>
#include <cstring>
#include <PlayerWrapper.h>
#include <render/video/VideoGLRender.h>
#include <render/video/VRGLRender.h>
#include <render/audio/OpenSLRender.h>
#include <MediaRecorderContext.h>
#include "util/LogUtil.h"
#include "jni.h"
#include "ASanTestCase.h"

extern "C" {
#include <libavcodec/version.h>
#include <libavcodec/avcodec.h>
#include <libavformat/version.h>
#include <libavformat/avformat.h>
#include <libavutil/version.h>
#include <libavutil/mathematics.h>
#include <libavfilter/version.h>
#include <libswresample/version.h>
#include <libswscale/version.h>
};

#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     com_byteflow_learnffmpeg_media_FFMediaPlayer
 * Method:    native_GetFFmpegVersion
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_com_byteflow_learnffmpeg_media_FFMediaPlayer_native_1GetFFmpegVersion
        (JNIEnv *env, jclass cls)
{
    char strBuffer[1024 * 4] = {0};
    strcat(strBuffer, "libavcodec : ");
    strcat(strBuffer, AV_STRINGIFY(LIBAVCODEC_VERSION));
    strcat(strBuffer, "\nlibavformat : ");
    strcat(strBuffer, AV_STRINGIFY(LIBAVFORMAT_VERSION));
    strcat(strBuffer, "\nlibavutil : ");
    strcat(strBuffer, AV_STRINGIFY(LIBAVUTIL_VERSION));
    strcat(strBuffer, "\nlibavfilter : ");
    strcat(strBuffer, AV_STRINGIFY(LIBAVFILTER_VERSION));
    strcat(strBuffer, "\nlibswresample : ");
    strcat(strBuffer, AV_STRINGIFY(LIBSWRESAMPLE_VERSION));
    strcat(strBuffer, "\nlibswscale : ");
    strcat(strBuffer, AV_STRINGIFY(LIBSWSCALE_VERSION));
    strcat(strBuffer, "\navcodec_configure : \n");
    strcat(strBuffer, avcodec_configuration());
    strcat(strBuffer, "\navcodec_license : ");
    strcat(strBuffer, avcodec_license());
    LOGCATE("GetFFmpegVersion\n%s", strBuffer);

    //ASanTestCase::MainTest();

    return env->NewStringUTF(strBuffer);
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_byteflow_learnffmpeg_media_FFMediaPlayer_native_1ConvertVideo(JNIEnv *env, jclass clazz) {
    AVOutputFormat *ofmt = NULL;  // 输出格式
    AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL; // 输入、输出是上下文环境
    AVPacket pkt;
    const char *in_filename, *out_filename;
    int ret, i;
    int stream_index = 0;
    int *stream_mapping = NULL; // 数组用于存放输出文件流的Index
    int stream_mapping_size = 0; // 输入文件中流的总数量

//    in_filename  = "/sdcard/DCIM/midway.mp4";
//    out_filename = "/sdcard/DCIM/output.mov";

    in_filename  = "/sdcard/DCIM/output.mov";
    out_filename = "/sdcard/DCIM/output.mp4";

    // 打开输入文件为ifmt_ctx分配内存
    if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
        fprintf(stderr, "Could not open input file '%s'", in_filename);
        goto end;
    }

    // 检索输入文件的流信息
    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        fprintf(stderr, "Failed to retrieve input stream information");
        goto end;
    }

    // 打印输入文件相关信息
    av_dump_format(ifmt_ctx, 0, in_filename, 0);

    // 为输出上下文环境分配内存
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);
    if (!ofmt_ctx) {
        fprintf(stderr, "Could not create output context\n");
        ret = AVERROR_UNKNOWN;
        goto end;
    }

    // 输入文件流的数量
    stream_mapping_size = ifmt_ctx->nb_streams;

    // 分配stream_mapping_size段内存，每段内存大小是sizeof(*stream_mapping)
    stream_mapping = static_cast<int *>(av_mallocz_array(stream_mapping_size,
                                                         sizeof(*stream_mapping)));
    if (!stream_mapping) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    // 输出文件格式
    ofmt = ofmt_ctx->oformat;

    // 遍历输入文件中的每一路流，对于每一路流都要创建一个新的流进行输出
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        AVStream *out_stream; // 输出流
        AVStream *in_stream = ifmt_ctx->streams[i]; // 输入流
        AVCodecParameters *in_codecpar = in_stream->codecpar; // 输入流的编解码参数

        // 只保留音频、视频、字幕流，其他的流不需要
        if (in_codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) {
            stream_mapping[i] = -1;
            continue;
        }

        // 对于输出的流的index重写编号
        stream_mapping[i] = stream_index++;

        // 创建一个对应的输出流
        out_stream = avformat_new_stream(ofmt_ctx, NULL);
        if (!out_stream) {
            fprintf(stderr, "Failed allocating output stream\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }

        // 直接将输入流的编解码参数拷贝到输出流中
        ret = avcodec_parameters_copy(out_stream->codecpar, in_codecpar);
        if (ret < 0) {
            fprintf(stderr, "Failed to copy codec parameters\n");
            goto end;
        }
        out_stream->codecpar->codec_tag = 0;
    }

    // 打印要输出的多媒体文件的详细信息
    av_dump_format(ofmt_ctx, 0, out_filename, 1);

    if (!(ofmt->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            fprintf(stderr, "Could not open output file '%s'", out_filename);
            goto end;
        }
    }

    // 写入新的多媒体文件的头
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "Error occurred when opening output file\n");
        goto end;
    }

    while (1) {
        AVStream *in_stream, *out_stream;

        // 循环读取每一帧数据
        ret = av_read_frame(ifmt_ctx, &pkt);
        if (ret < 0) // 读取完后退出循环
            break;

        in_stream  = ifmt_ctx->streams[pkt.stream_index];
        if (pkt.stream_index >= stream_mapping_size ||
            stream_mapping[pkt.stream_index] < 0) {
            av_packet_unref(&pkt);
            continue;
        }

        pkt.stream_index = stream_mapping[pkt.stream_index]; // 按照输出流的index给pkt重新编号
        out_stream = ofmt_ctx->streams[pkt.stream_index]; // 根据pkt的stream_index获取对应的输出流

        // 对pts、dts、duration进行时间基转换，不同格式时间基都不一样，不转换会导致音视频同步问题
        pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base,
                                   static_cast<AVRounding>(AV_ROUND_NEAR_INF |
                                                           AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base,
                                   static_cast<AVRounding>(AV_ROUND_NEAR_INF |
                                                           AV_ROUND_PASS_MINMAX));
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;

        // 将处理好的pkt写入输出文件
        ret = av_interleaved_write_frame(ofmt_ctx, &pkt);
        if (ret < 0) {
            fprintf(stderr, "Error muxing packet\n");
            break;
        }
        av_packet_unref(&pkt);
    }

    // 写入新的多媒体文件尾
    av_write_trailer(ofmt_ctx);
    end:

    avformat_close_input(&ifmt_ctx);

    /* close output */
    if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
        avio_closep(&ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);

    av_freep(&stream_mapping);

    if (ret < 0 && ret != AVERROR_EOF) {
        fprintf(stderr, "Error occurred: %s\n", av_err2str(ret));
        return 1;
    }

    return 0;
}

/*
 * Class:     com_byteflow_learnffmpeg_media_FFMediaPlayer
 * Method:    native_Init
 * Signature: (JLjava/lang/String;Ljava/lang/Object;)J
 */
JNIEXPORT jlong JNICALL Java_com_byteflow_learnffmpeg_media_FFMediaPlayer_native_1Init
        (JNIEnv *env, jobject obj, jstring jurl, int playerType, jint renderType, jobject surface)
{
    const char* url = env->GetStringUTFChars(jurl, nullptr);
    PlayerWrapper *player = new PlayerWrapper();
    player->Init(env, obj, const_cast<char *>(url), playerType, renderType, surface);
    env->ReleaseStringUTFChars(jurl, url);
    return reinterpret_cast<jlong>(player);
}

/*
 * Class:     com_byteflow_learnffmpeg_media_FFMediaPlayer
 * Method:    native_Play
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_com_byteflow_learnffmpeg_media_FFMediaPlayer_native_1Play
(JNIEnv *env, jobject obj, jlong player_handle)
{
    if(player_handle != 0)
    {
        PlayerWrapper *pPlayerWrapper = reinterpret_cast<PlayerWrapper *>(player_handle);
        pPlayerWrapper->Play();
    }

}

JNIEXPORT void JNICALL
Java_com_byteflow_learnffmpeg_media_FFMediaPlayer_native_1SeekToPosition(JNIEnv *env, jobject thiz,
                                                                      jlong player_handle, jfloat position) {
    if(player_handle != 0)
    {
        PlayerWrapper *ffMediaPlayer = reinterpret_cast<PlayerWrapper *>(player_handle);
        ffMediaPlayer->SeekToPosition(position);
    }
}

JNIEXPORT jlong JNICALL
Java_com_byteflow_learnffmpeg_media_FFMediaPlayer_native_1GetMediaParams(JNIEnv *env, jobject thiz,
                                                                         jlong player_handle,
                                                                         jint param_type) {
    long value = 0;
    if(player_handle != 0)
    {
        PlayerWrapper *ffMediaPlayer = reinterpret_cast<PlayerWrapper *>(player_handle);
        value = ffMediaPlayer->GetMediaParams(param_type);
    }
    return value;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_byteflow_learnffmpeg_media_FFMediaPlayer_native_1SetMediaParams(JNIEnv *env, jobject thiz,
                                                                         jlong player_handle,
                                                                         jint param_type,
                                                                         jobject param) {
    if(player_handle != 0)
    {
        PlayerWrapper *ffMediaPlayer = reinterpret_cast<PlayerWrapper *>(player_handle);
        ffMediaPlayer->SetMediaParams(param_type, param);
    }
}

/*
 * Class:     com_byteflow_learnffmpeg_media_FFMediaPlayer
 * Method:    native_Pause
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_com_byteflow_learnffmpeg_media_FFMediaPlayer_native_1Pause
(JNIEnv *env, jobject obj, jlong player_handle)
{
    if(player_handle != 0)
    {
        PlayerWrapper *ffMediaPlayer = reinterpret_cast<PlayerWrapper *>(player_handle);
        ffMediaPlayer->Pause();
    }
}

/*
 * Class:     com_byteflow_learnffmpeg_media_FFMediaPlayer
 * Method:    native_Stop
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_com_byteflow_learnffmpeg_media_FFMediaPlayer_native_1Stop
(JNIEnv *env, jobject obj, jlong player_handle)
{
    if(player_handle != 0)
    {
        PlayerWrapper *ffMediaPlayer = reinterpret_cast<PlayerWrapper *>(player_handle);
        ffMediaPlayer->Stop();
    }
}

/*
 * Class:     com_byteflow_learnffmpeg_media_FFMediaPlayer
 * Method:    native_UnInit
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_com_byteflow_learnffmpeg_media_FFMediaPlayer_native_1UnInit
(JNIEnv *env, jobject obj, jlong player_handle)
{
    if(player_handle != 0)
    {
        PlayerWrapper *ffMediaPlayer = reinterpret_cast<PlayerWrapper *>(player_handle);
        ffMediaPlayer->UnInit();
        delete ffMediaPlayer;
    }
}

JNIEXPORT void JNICALL
Java_com_byteflow_learnffmpeg_media_FFMediaPlayer_native_1OnSurfaceCreated(JNIEnv *env,
                                                                           jclass clazz,
                                                                           jint render_type) {
    switch (render_type)
    {
        case VIDEO_GL_RENDER:
            VideoGLRender::GetInstance()->OnSurfaceCreated();
            break;
        case AUDIO_GL_RENDER:
            AudioGLRender::GetInstance()->OnSurfaceCreated();
            break;
        case VR_3D_GL_RENDER:
            VRGLRender::GetInstance()->OnSurfaceCreated();
            break;
        default:
            break;
    }
}

JNIEXPORT void JNICALL
Java_com_byteflow_learnffmpeg_media_FFMediaPlayer_native_1OnSurfaceChanged(JNIEnv *env, jclass clazz,
                                                                           jint render_type,
                                                                           jint width,
                                                                           jint height) {
    switch (render_type)
    {
        case VIDEO_GL_RENDER:
            VideoGLRender::GetInstance()->OnSurfaceChanged(width, height);
            break;
        case AUDIO_GL_RENDER:
            AudioGLRender::GetInstance()->OnSurfaceChanged(width, height);
            break;
        case VR_3D_GL_RENDER:
            VRGLRender::GetInstance()->OnSurfaceChanged(width, height);
            break;
        default:
            break;
    }
}

JNIEXPORT void JNICALL
Java_com_byteflow_learnffmpeg_media_FFMediaPlayer_native_1OnDrawFrame(JNIEnv *env, jclass clazz, jint render_type) {
    switch (render_type)
    {
        case VIDEO_GL_RENDER:
            VideoGLRender::GetInstance()->OnDrawFrame();
            break;
        case AUDIO_GL_RENDER:
            AudioGLRender::GetInstance()->OnDrawFrame();
            break;
        case VR_3D_GL_RENDER:
            VRGLRender::GetInstance()->OnDrawFrame();
            break;
        default:
            break;
    }
}

JNIEXPORT void JNICALL
Java_com_byteflow_learnffmpeg_media_FFMediaPlayer_native_1SetGesture(JNIEnv *env, jclass clazz,
                                                                     jint   render_type,
                                                                     jfloat x_rotate_angle,
                                                                     jfloat y_rotate_angle,
                                                                     jfloat scale) {
    switch (render_type)
    {
        case VIDEO_GL_RENDER:
            VideoGLRender::GetInstance()->UpdateMVPMatrix(x_rotate_angle, y_rotate_angle, scale, scale);
            break;
        case AUDIO_GL_RENDER:
            AudioGLRender::GetInstance()->UpdateMVPMatrix(x_rotate_angle, y_rotate_angle, scale, scale);
            break;
        case VR_3D_GL_RENDER:
            VRGLRender::GetInstance()->UpdateMVPMatrix(x_rotate_angle, y_rotate_angle, scale, scale);
            break;
        default:
            break;
    }
}

JNIEXPORT void JNICALL
Java_com_byteflow_learnffmpeg_media_FFMediaPlayer_native_1SetTouchLoc(JNIEnv *env, jclass clazz,
                                                                      jint   render_type,
                                                                      jfloat touch_x,
                                                                      jfloat touch_y) {
    switch (render_type)
    {
        case VIDEO_GL_RENDER:
            VideoGLRender::GetInstance()->SetTouchLoc(touch_x, touch_y);
            break;
        case AUDIO_GL_RENDER:
            AudioGLRender::GetInstance()->SetTouchLoc(touch_x, touch_y);
            break;
        case VR_3D_GL_RENDER:
            VRGLRender::GetInstance()->SetTouchLoc(touch_x, touch_y);
            break;
        default:
            break;
    }
}

#ifdef __cplusplus
}
#endif

extern "C"
JNIEXPORT void JNICALL
Java_com_byteflow_learnffmpeg_media_MediaRecorderContext_native_1CreateContext(JNIEnv *env,
                                                                                jobject thiz) {
    MediaRecorderContext::CreateContext(env, thiz);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_byteflow_learnffmpeg_media_MediaRecorderContext_native_1DestroyContext(JNIEnv *env,
                                                                                 jobject thiz) {
    MediaRecorderContext::DeleteContext(env, thiz);
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_byteflow_learnffmpeg_media_MediaRecorderContext_native_1Init(JNIEnv *env, jobject thiz) {
    MediaRecorderContext *pContext = MediaRecorderContext::GetContext(env, thiz);
    if(pContext) return pContext->Init();
    return 0;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_byteflow_learnffmpeg_media_MediaRecorderContext_native_1UnInit(JNIEnv *env, jobject thiz) {
    MediaRecorderContext *pContext = MediaRecorderContext::GetContext(env, thiz);
    if(pContext) return pContext->UnInit();
    return 0;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_byteflow_learnffmpeg_media_MediaRecorderContext_native_1StartRecord(JNIEnv *env,
                                                                             jobject thiz,
                                                                             jint recorder_type,
                                                                             jstring out_url,
                                                                             jint frame_width,
                                                                             jint frame_height,
                                                                             jlong video_bit_rate,
                                                                             jint fps) {
    const char* url = env->GetStringUTFChars(out_url, nullptr);
    MediaRecorderContext *pContext = MediaRecorderContext::GetContext(env, thiz);
    env->ReleaseStringUTFChars(out_url, url);
    if(pContext) return pContext->StartRecord(recorder_type, url, frame_width, frame_height, video_bit_rate, fps);
    return 0;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_byteflow_learnffmpeg_media_MediaRecorderContext_native_1OnAudioData(JNIEnv *env,
                                                                             jobject thiz,
                                                                             jbyteArray data,
                                                                             jint size) {
    int len = env->GetArrayLength (data);
    unsigned char* buf = new unsigned char[len];
    env->GetByteArrayRegion(data, 0, len, reinterpret_cast<jbyte*>(buf));
    MediaRecorderContext *pContext = MediaRecorderContext::GetContext(env, thiz);
    if(pContext) pContext->OnAudioData(buf, len);
    delete[] buf;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_byteflow_learnffmpeg_media_MediaRecorderContext_native_1StopRecord(JNIEnv *env,
                                                                            jobject thiz) {
    MediaRecorderContext *pContext = MediaRecorderContext::GetContext(env, thiz);
    if(pContext) return pContext->StopRecord();
    return 0;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_byteflow_learnffmpeg_media_MediaRecorderContext_native_1OnPreviewFrame(JNIEnv *env,
                                                                                 jobject thiz,
                                                                                 jint format,
                                                                                 jbyteArray data,
                                                                                 jint width,
                                                                                 jint height) {
    int len = env->GetArrayLength (data);
    unsigned char* buf = new unsigned char[len];
    env->GetByteArrayRegion(data, 0, len, reinterpret_cast<jbyte*>(buf));
    MediaRecorderContext *pContext = MediaRecorderContext::GetContext(env, thiz);
    if(pContext) pContext->OnPreviewFrame(format, buf, width, height);
    delete[] buf;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_byteflow_learnffmpeg_media_MediaRecorderContext_native_1SetTransformMatrix(JNIEnv *env,
                                                                                     jobject thiz,
                                                                                     jfloat translate_x,
                                                                                     jfloat translate_y,
                                                                                     jfloat scale_x,
                                                                                     jfloat scale_y,
                                                                                     jint degree,
                                                                                     jint mirror) {
    MediaRecorderContext *pContext = MediaRecorderContext::GetContext(env, thiz);
    if(pContext) pContext->SetTransformMatrix(translate_x, translate_y, scale_x, scale_y, degree, mirror);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_byteflow_learnffmpeg_media_MediaRecorderContext_native_1OnSurfaceCreated(JNIEnv *env,
                                                                                   jobject thiz) {
    MediaRecorderContext *pContext = MediaRecorderContext::GetContext(env, thiz);
    if(pContext) pContext->OnSurfaceCreated();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_byteflow_learnffmpeg_media_MediaRecorderContext_native_1OnSurfaceChanged(JNIEnv *env,
                                                                                   jobject thiz,
                                                                                   jint width,
                                                                                   jint height) {
    MediaRecorderContext *pContext = MediaRecorderContext::GetContext(env, thiz);
    if(pContext) pContext->OnSurfaceChanged(width, height);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_byteflow_learnffmpeg_media_MediaRecorderContext_native_1OnDrawFrame(JNIEnv *env, jobject thiz) {
    MediaRecorderContext *pContext = MediaRecorderContext::GetContext(env, thiz);
    if(pContext) pContext->OnDrawFrame();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_byteflow_learnffmpeg_media_MediaRecorderContext_native_1SetFilterData(JNIEnv *env,
                                                                               jobject thiz,
                                                                               jint index,
                                                                               jint format,
                                                                               jint width,
                                                                               jint height,
                                                                               jbyteArray bytes) {
    int len = env->GetArrayLength (bytes);
    uint8_t* buf = new uint8_t[len];
    env->GetByteArrayRegion(bytes, 0, len, reinterpret_cast<jbyte*>(buf));
    MediaRecorderContext *pContext = MediaRecorderContext::GetContext(env, thiz);
    if(pContext) pContext->SetLUTImage(index, format, width, height, buf);
    delete[] buf;
    env->DeleteLocalRef(bytes);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_byteflow_learnffmpeg_media_MediaRecorderContext_native_1SetFragShader(JNIEnv *env,
                                                                               jobject thiz,
                                                                               jint index,
                                                                               jstring str) {
    int length = env->GetStringUTFLength(str);
    const char* cStr = env->GetStringUTFChars(str, JNI_FALSE);
    char *buf = static_cast<char *>(malloc(length + 1));
    memcpy(buf, cStr, length + 1);
    MediaRecorderContext *pContext = MediaRecorderContext::GetContext(env, thiz);
    if(pContext) pContext->SetFragShader(index, buf, length + 1);
    free(buf);
    env->ReleaseStringUTFChars(str, cStr);
}
