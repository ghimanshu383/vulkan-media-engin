//
// Created by ghima on 13-01-2026.
//

#ifndef REALTIMEFRAMEDISPLAY_FRAMEGENERATORTWO_H
#define REALTIMEFRAMEDISPLAY_FRAMEGENERATORTWO_H

extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
};

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include "Util.h"
#include <Audioclient.h>
namespace fd {
    class FrameGeneratorTwo {
    private:
        std::mutex _mutex_vid;
        std::mutex _mutex_aud;
        bool vidStop = false;
        bool m_isVidGeneratorReady = false;
        double m_timebase = 0.0;
        int m_width = 0;
        int m_height = 0;
        std::condition_variable m_cv_vid;
        std::condition_variable m_cv_aud;
        int audioIndex = -1;
        int videoIndex = -1;
        AVFormatContext *m_av_Context = nullptr;
        std::queue<std::unique_ptr<AVFrame, void (*)(AVFrame *)>> m_vid_decoded_frame_queue{};
        std::queue<std::unique_ptr<AVFrame, void (*)(AVFrame *)>> m_aud_decoded_frame_queue{};
        std::queue<VideoFrame> m_vid_frame_queue;
        IAudioClient* m_audioClient = nullptr;
        IAudioRenderClient* m_audio_render_client = nullptr;
        UINT32 bufferFrameCount = 0;

        void start_demuxer_thread(const char *videoPath);

        void start_video_decoder_thread();

        void start_audio_decoder_thread();

    public:
        ~FrameGeneratorTwo();

        std::chrono::time_point<std::chrono::steady_clock> frame_start;

        void process(const char *videoPath);

        std::mutex &get_vid_mutex() { return _mutex_vid; }

        std::queue<VideoFrame> &get_vide_frame_queue() { return m_vid_frame_queue; }

        std::condition_variable &get_vid_cv() { return m_cv_vid; };

        int get_vid_frame_height() const { return m_height; }

        int get_vid_frame_width() const { return m_width; }

        bool is_generator_ready() const { return m_isVidGeneratorReady; }

        static void free_clone_frame(AVFrame *clone) {
            av_frame_free(&clone);
        }

        void notify_video_frame_processed() {
            {
                m_vid_frame_queue.pop();
                m_cv_vid.notify_one();
            }
        }
        void setup_audio_listener_win();
    };
}
#endif //REALTIMEFRAMEDISPLAY_FRAMEGENERATORTWO_H
