//
// Created by ghima on 07-01-2026.
//

#ifndef REALTIMEFRAMEDISPLAY_FRAMEGENERATOR_H
#define REALTIMEFRAMEDISPLAY_FRAMEGENERATOR_H
extern "C" {
#include "libavformat/avformat.h"
};

#include "Util.h"
#include <condition_variable>
#include <queue>
#include <memory>
#include <chrono>

namespace fd {
    class FrameGenerator {
    private:
        AVFormatContext *m_avContext = nullptr;

        AvIndex load_av_stream(const char *videoPath);

        void decode_and_process_vid_frames(int vidIndex);
        void decode_and_process_audio_frames(int audioIndex);

    public:
        bool isGeneratorReady = false;
        bool isFrameProcessed = false;
        bool isFirstRender = true;
        uint8_t *m_yPlane = nullptr;
        uint32_t m_height = 0;
        uint32_t m_yStride = 0;
        std::condition_variable m_cv;
        std::condition_variable m_cv_render;
        std::mutex _mutex;
        std::queue<VideoFrame> m_frame_queue;
        std::chrono::time_point<std::chrono::steady_clock> frame_start;
        bool isClockStarted = false;


        void process(const char *vidPath);

        void notify_frame_processed() {

            isFrameProcessed = true;
            m_frame_queue.pop();
            m_cv_render.notify_one();
        }
    };
}
#endif //REALTIMEFRAMEDISPLAY_FRAMEGENERATOR_H
