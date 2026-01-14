//
// Created by ghima on 07-01-2026.
//
#include <iostream>
#include "FrameGenerator.h"
#include "FrameHandler.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavutil/pixdesc.h"
}


namespace fd {
    AvIndex FrameGenerator::load_av_stream(const char *videoPath) {
        if (avformat_open_input(&m_avContext, videoPath, nullptr, nullptr) < 0) {
            std::cout << "failed to load the video from the context" << std::endl;
            std::exit(EXIT_FAILURE);
        }
        if (avformat_find_stream_info(m_avContext, nullptr) < 0) {
            std::cout << "Failed to load the stream info" << std::endl;
            std::exit(EXIT_FAILURE);
        }
        int videoStream = -1;
        int audioStream = -1;
        for (int i = 0; i < m_avContext->nb_streams; i++) {
            if (m_avContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                videoStream = i;
            } else if (m_avContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                audioStream = i;
            }
        }
        if (videoStream == -1) {
            std::cout << "No Video Stream found in this file" << std::endl;
        }
        if (audioStream == -1) {
            std::cout << "No Audio Stream found in this file";
        }
        return {audioStream, videoStream};
    }

    void FrameGenerator::decode_and_process_vid_frames(int vidIndex) {
        AVRational rationalTimeBase = m_avContext->streams[vidIndex]->time_base;
        double timeBase = av_q2d(rationalTimeBase);

        AVCodecParameters *codecPars = m_avContext->streams[vidIndex]->codecpar;
        const AVCodec *decoder = avcodec_find_decoder(codecPars->codec_id);
        if (!decoder) {
            std::cout << "failed to get any decoder for the stream" << std::endl;
            std::exit(EXIT_FAILURE);
        }
        AVCodecContext *codecContext = avcodec_alloc_context3(decoder);
        avcodec_parameters_to_context(codecContext, codecPars);

        if (avcodec_open2(codecContext, decoder, nullptr) < 0) {
            std::cout << "Failed to open the decoder " << std::endl;
            std::exit(EXIT_FAILURE);
        }

        AVPacket *packet = av_packet_alloc();
        AVFrame *frame = av_frame_alloc();

        while (av_read_frame(m_avContext, packet) >= 0) {
            if (packet->stream_index == vidIndex) {
                if (avcodec_send_packet(codecContext, packet) == 0) {
                    while (avcodec_receive_frame(codecContext, frame) == 0) {
                        double pts = 0.0;
                        if (frame->pts != AV_NOPTS_VALUE) {
                            pts = (double) frame->pts * timeBase;
                        }

                        int w = frame->width;
                        int h = frame->height;
                        uint8_t *yPlane = frame->data[0];
                        uint8_t *uPlane = frame->data[1];
                        uint8_t *vPlane = frame->data[2];

                        int yStride = frame->linesize[0];
                        int uStride = frame->linesize[1];
                        int vStride = frame->linesize[2];

//                        AVPixelFormat format = static_cast<AVPixelFormat>(frame->format);
//                        const char *fmtName = av_get_pix_fmt_name(format);
                        // the plane is deleted at the consumer side;
                        int chromaH = h >> 1;
                        int chromaW = w >> 1;
                        std::unique_ptr plane = std::make_unique<uint8_t[]>(h * w);
                        std::unique_ptr uPlanePtr = std::make_unique<uint8_t[]>(chromaH * chromaW);
                        std::unique_ptr vPlanePtr = std::make_unique<uint8_t[]>(chromaH * chromaW);

                        for (int y = 0; y < h; y++) {
                            memcpy(&plane[y * w], &yPlane[y * yStride], w);
                        }

                        for (int y = 0; y < chromaH; y++) {
                            memcpy(&uPlanePtr[y * chromaW], &uPlane[y * uStride], chromaW);
                            memcpy(&vPlanePtr[y * chromaW], &vPlane[y * vStride], chromaW);
                        }
                        {
                            std::lock_guard<std::mutex> lock{_mutex};
                            VideoFrame videoFrame{std::move(plane), std::move(uPlanePtr), std::move(vPlanePtr), pts};

                            m_frame_queue.push(std::move(videoFrame));
                            isGeneratorReady = true;
                            m_height = h;
                            m_yStride = w;
                            if (!isClockStarted) {
                                frame_start = std::chrono::steady_clock::now();
                                isClockStarted = true;
                            }
                            m_cv.notify_all();
                        }

                        {
                            std::unique_lock<std::mutex> lock{_mutex};
                            m_cv_render.wait(lock, [this]() -> bool { return m_frame_queue.size() <= MAX_FRAMES; });
                        }
                        av_frame_unref(frame);
                    }
                }
            }
            av_packet_unref(packet);
        }
        avcodec_send_packet(codecContext, nullptr);
        while (avcodec_receive_frame(codecContext, frame) == 0) {
            av_frame_unref(frame);
        }
    }

    void FrameGenerator::decode_and_process_audio_frames(int audioIndex) {

    }

    void FrameGenerator::process(const char *vidPath) {
        std::thread threadOne{[this, vidPath]() -> void {
            AvIndex avIndex = load_av_stream(vidPath);
            decode_and_process_vid_frames(avIndex.videoIndex);
        }};
        threadOne.detach();
//        if (avIndex.audioIndex != -1) {
//            std::thread threadAudio{[this, &avIndex]() -> void {
//                decode_and_process_audio_frames(avIndex.audioIndex);
//            }};
//            threadAudio.detach();
//        }
    }

}