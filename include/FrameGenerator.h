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

namespace fd {
    class FrameGenerator {
    private:
        AVFormatContext *m_avContext = nullptr;

        int load_video_stream(const char *videoPath);

        void decode_and_process_frames(int vidIndex);

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
        std::queue<std::unique_ptr<uint8_t[]>> m_frame_queue;
        std::queue<std::unique_ptr<uint8_t[]>> m_frame_queue_u;
        std::queue<std::unique_ptr<uint8_t []>> m_frame_queue_v;

        void process(const char *vidPath);

        void notify_frame_processed() {
            {
                isFrameProcessed = true;
                m_frame_queue.pop();
                m_frame_queue_v.pop();
                m_frame_queue_u.pop();
                m_cv_render.notify_one();
            }
        }
    };
}
#endif //REALTIMEFRAMEDISPLAY_FRAMEGENERATOR_H
