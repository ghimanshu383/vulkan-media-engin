//
// Created by ghima on 13-01-2026.
//
#include <iostream>
#include "FrameGeneratorTwo.h"
#include "Util.h"
#include <windows.h>
#include <mmdeviceapi.h>
#include <uuids.h>


namespace fd {
    void FrameGeneratorTwo::start_demuxer_thread(const char *videoPath) {
        std::thread demuxer{[this, videoPath]() -> void {

            if (avformat_open_input(&m_av_Context, videoPath, nullptr, nullptr) < 0) {
                std::cout << "failed to load the video from the context" << std::endl;
                std::exit(EXIT_FAILURE);
            }
            if (avformat_find_stream_info(m_av_Context, nullptr) < 0) {
                std::cout << "Failed to load the stream info" << std::endl;
                std::exit(EXIT_FAILURE);
            }
            for (int i = 0; i < m_av_Context->nb_streams; i++) {
                if (m_av_Context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                    _mutex_vid.lock();
                    videoIndex = i;
                    _mutex_vid.unlock();

                } else if (m_av_Context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                    _mutex_aud.lock();
                    audioIndex = i;
                    _mutex_aud.unlock();
                }
            }
            m_cv_aud.notify_all();
            m_cv_vid.notify_all();
            // Setting up the codecs
            bool vidDecoderReady = false;
            bool audioDecoderReady = false;
            AVCodecContext *vidCodecContext = nullptr;
            AVCodecContext *audioContext = nullptr;
            if (videoIndex != -1) {
                AVCodecParameters *videoCodecParams = m_av_Context->streams[videoIndex]->codecpar;
                AVRational timebase = m_av_Context->streams[videoIndex]->time_base;
                m_timebase = av_q2d(timebase);
                const AVCodec *vidDecoder = avcodec_find_decoder(videoCodecParams->codec_id);
                vidCodecContext = avcodec_alloc_context3(vidDecoder);
                avcodec_parameters_to_context(vidCodecContext, videoCodecParams);
                if (avcodec_open2(vidCodecContext, vidDecoder, nullptr) >= 0) {
                    vidDecoderReady = true;
                }
            }
            if (audioIndex != -1) {
                AVCodecParameters *audioCodecParams = m_av_Context->streams[audioIndex]->codecpar;
                const AVCodec *audioDecoder = avcodec_find_decoder(audioCodecParams->codec_id);
                audioContext = avcodec_alloc_context3(audioDecoder);
                avcodec_parameters_to_context(audioContext, audioCodecParams);
                if (avcodec_open2(audioContext, audioDecoder, nullptr) >= 0) {
                    audioDecoderReady = true;
                }
            }
            AVPacket *packet = av_packet_alloc();
            AVFrame *frame = av_frame_alloc();
            AVFrame *frameAud = av_frame_alloc();

            while (av_read_frame(m_av_Context, packet) >= 0) {
                if (vidStop) break;
                if (packet->stream_index == videoIndex && vidDecoderReady) {
                    if (avcodec_send_packet(vidCodecContext, packet) == 0) {
                        while (avcodec_receive_frame(vidCodecContext, frame) == 0) {
                            {
                                std::unique_lock<std::mutex> lock{_mutex_vid};
                                m_cv_vid.wait(lock, [this]() -> bool {
                                    return m_vid_frame_queue.size() <= MAX_FRAMES;
                                });
                            }
                            AVFrame *clone = av_frame_clone(frame);
                            std::unique_ptr<AVFrame, void (*)(AVFrame *)> framePtr(clone,
                                                                                   &FrameGeneratorTwo::free_clone_frame);
                            _mutex_vid.lock();
                            m_vid_decoded_frame_queue.push(std::move(framePtr));
                            _mutex_vid.unlock();
                            m_cv_vid.notify_one();
                            av_frame_unref(frame);
                        }
                    }
                }
                if (packet->stream_index == audioIndex && audioDecoderReady) {
                    if (avcodec_send_packet(audioContext, packet) == 0) {
                        while (avcodec_receive_frame(audioContext, frameAud) == 0) {
                            {
                                std::unique_lock<std::mutex> lock{_mutex_aud};
                                m_cv_aud.wait(lock, [this]() -> bool {
                                    return m_aud_decoded_frame_queue.size() <= MAX_FRAMES;
                                });
                            }
                            AVFrame *clone = av_frame_clone(frameAud);
                            std::unique_ptr<AVFrame, void (*)(AVFrame *)> framePtr(clone,
                                                                                   &FrameGeneratorTwo::free_clone_frame);
                            _mutex_aud.lock();
                            m_aud_decoded_frame_queue.push(std::move(framePtr));
                            _mutex_aud.unlock();
                            m_cv_aud.notify_one();
                            av_frame_unref(frameAud);
                        }
                    }
                }
                av_packet_unref(packet);
            }
            _mutex_vid.lock();
            vidStop = true;
            _mutex_vid.unlock();
        }};
        demuxer.detach();
    }

    void FrameGeneratorTwo::start_video_decoder_thread() {
        std::thread videoDecoder{[this]() -> void {
            {
                std::unique_lock<std::mutex> lock{_mutex_vid};
                m_cv_vid.wait(lock, [this]() -> bool { return videoIndex != -1; });
            }
            LOG_INFO("Starting the video decoder");
            while (true) {
                {
                    std::unique_lock<std::mutex> lock{_mutex_vid};
                    m_cv_vid.wait(lock, [this]() -> bool {
                        return vidStop || !m_vid_decoded_frame_queue.empty();
                    });
                }
                if (vidStop || m_vid_decoded_frame_queue.empty()) break;
                std::unique_ptr<AVFrame, void (*)(AVFrame *)> framePtr = std::move(m_vid_decoded_frame_queue.front());
                m_vid_decoded_frame_queue.pop();
                double pts = 0;
                if (framePtr->pts != AV_NOPTS_VALUE) {
                    pts = (double) framePtr->pts * m_timebase;
                }
                int width = framePtr->width;
                int height = framePtr->height;
                uint8_t *yPlane = framePtr->data[0];
                uint8_t *uPlane = framePtr->data[1];
                uint8_t *vPlane = framePtr->data[2];
                int yStride = framePtr->linesize[0];
                int uStride = framePtr->linesize[1];
                int vStride = framePtr->linesize[2];

                int chromaW = width >> 1;
                int chromaH = height >> 1;
                std::unique_ptr<uint8_t[]> yPlanePtr = std::make_unique<uint8_t[]>(width * height);
                std::unique_ptr<uint8_t[]> uPlanePtr = std::make_unique<uint8_t[]>(chromaW * chromaH);
                std::unique_ptr<uint8_t[]> vPlanePtr = std::make_unique<uint8_t[]>(chromaW * chromaH);

                for (int y = 0; y < height; y++) {
                    memcpy(&yPlanePtr[y * width], &yPlane[y * yStride], width);
                }
                for (int y = 0; y < chromaH; y++) {
                    memcpy(&uPlanePtr[y * chromaW], &uPlane[y * uStride], chromaW);
                    memcpy(&vPlanePtr[y * chromaW], &vPlane[y * vStride], chromaW);
                }
                {
                    std::lock_guard<std::mutex> lock{_mutex_vid};
                    if (!m_isVidGeneratorReady) {
                        m_isVidGeneratorReady = true;
                        frame_start = std::chrono::steady_clock::now();
                    }
                    m_width = width;
                    m_height = height;
                    VideoFrame videoFrame{std::move(yPlanePtr), std::move(uPlanePtr), std::move(vPlanePtr), pts};
                    m_vid_frame_queue.push(std::move(videoFrame));
                }
            }
        }};
        videoDecoder.detach();
    }

    void FrameGeneratorTwo::start_audio_decoder_thread() {
        std::thread audioDecoder{[this]() -> void {
            setup_audio_listener_win();
            {
                std::unique_lock<std::mutex> lock{_mutex_aud};
                m_cv_aud.wait(lock,
                              [this]() -> bool { return audioIndex != -1; });
            }
            LOG_INFO("Starting the audio frame");
            while (true) {
                {
                    std::unique_lock<std::mutex> lock{_mutex_aud};
                    m_cv_aud.wait(lock, [this]() -> bool { return vidStop || !m_aud_decoded_frame_queue.empty(); });
                }
                if (vidStop || m_aud_decoded_frame_queue.empty()) break;
                _mutex_aud.lock();
                std::unique_ptr<AVFrame, void (*)(AVFrame *)> framePtr = std::move(m_aud_decoded_frame_queue.front());
                m_aud_decoded_frame_queue.pop();
                _mutex_aud.unlock();
                m_cv_aud.notify_one();
                const char *fmtName = av_get_sample_fmt_name(static_cast<AVSampleFormat>(framePtr->format));
                LOG_INFO("Audio Frame number samples {}, sampleRate {}, channel Count {}, format {}",
                         framePtr->nb_samples,
                         framePtr->sample_rate, framePtr->ch_layout.nb_channels, fmtName);
                // Preparing the AUDIO PCM
                std::unique_ptr<int16_t[]> interleavedSamples = std::make_unique<int16_t[]>(framePtr->nb_samples * 2);
                float *lChannel = reinterpret_cast<float *>(framePtr->data[0]);
                float *rChannel = reinterpret_cast<float *>(framePtr->data[1]);
                for (int i = 0; i < framePtr->nb_samples; i++) {
                    interleavedSamples[2 * i + 0] = static_cast<int16_t >(clamp_float_audio(lChannel[i]) * 32767);
                    interleavedSamples[2 * i + 1] = static_cast<int16_t >(clamp_float_audio(rChannel[i]) * 32767);
                }
                AudioPCM pcm{std::move(interleavedSamples), framePtr->ch_layout.nb_channels, framePtr->nb_samples, 1};

                // Writing the audio frames 4 bytes to the audio buffer;
                UINT32 padding = 0;
                m_audioClient->GetCurrentPadding(&padding);
                UINT32 frameAvailable = bufferFrameCount - padding;
                UINT32 framesToWrite = frameAvailable > framePtr->nb_samples ? framePtr->nb_samples : frameAvailable;
                BYTE *data = nullptr;
                m_audio_render_client->GetBuffer(framesToWrite, &data);
                memcpy(data, pcm.samples.get(), framesToWrite * 4);
                m_audio_render_client->ReleaseBuffer(framesToWrite, 0);
                m_audio_clock += (double)framesToWrite / framePtr->nb_samples;
            }
        }};
        audioDecoder.detach();
    }

    void FrameGeneratorTwo::process(const char *videoPath) {
        start_video_decoder_thread();
        start_audio_decoder_thread();
        start_demuxer_thread(videoPath);
    }

    FrameGeneratorTwo::~FrameGeneratorTwo() {
        vidStop = true;
    }

    void FrameGeneratorTwo::setup_audio_listener_win() {
        HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(result)) {
            LOG_INFO("The com init failed");
        }
        IMMDeviceEnumerator *enumerator;
        CoCreateInstance(
                __uuidof(MMDeviceEnumerator),
                nullptr,
                CLSCTX_ALL,
                __uuidof(IMMDeviceEnumerator),
                (void **) &enumerator
        );
        IMMDevice *audioDevice;
        enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &audioDevice);
        audioDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void **) &m_audioClient);
        WAVEFORMATEX format{};
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = 2;
        format.nSamplesPerSec = 44100;
        format.wBitsPerSample = 16;
        format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
        format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;


        REFERENCE_TIME bufferDurationTime = 10000000;

        m_audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, bufferDurationTime, 0, &format, nullptr);
        HRESULT clientResult =  m_audioClient->GetService(__uuidof(IAudioRenderClient), (void **) &m_audio_render_client);
        if(FAILED(clientResult) || !m_audio_render_client) {
            LOG_INFO("Failed to get the render client");
            return;
        }
        m_audioClient->GetBufferSize(&bufferFrameCount);
        m_audioClient->Start();
    }
}