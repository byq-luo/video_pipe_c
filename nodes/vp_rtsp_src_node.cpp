

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include "vp_rtsp_src_node.h"
#include "../utils/vp_utils.h"

namespace vp_nodes {
        
    vp_rtsp_src_node::vp_rtsp_src_node(std::string node_name, 
                                        int channel_index, 
                                        std::string rtsp_url, 
                                        float resize_ratio): 
                                        vp_src_node(node_name, channel_index, resize_ratio),
                                        rtsp_url(rtsp_url) {
        this->gst_template = vp_utils::string_format(this->gst_template, rtsp_url.c_str());
        std::cout << this->gst_template << std::endl;
        this->initialized();
    }
    
    vp_rtsp_src_node::~vp_rtsp_src_node() {

    }
    
    // define how to read video from rtsp stream, create frame meta etc.
    // please refer to the implementation of vp_node::handle_run.
    void vp_rtsp_src_node::handle_run() {
        cv::Mat frame;
        int video_width = 0;
        int video_height = 0;
        int fps = 0;
        
        while(true) {
            if (this->active) {
                // open capture
                if (!rtsp_capture.isOpened()) {
                    assert(rtsp_capture.open(this->gst_template, cv::CAP_GSTREAMER));
                }

                // video properties
                if (video_width == 0 || video_height == 0 || fps == 0) {
                    video_width = rtsp_capture.get(cv::CAP_PROP_FRAME_WIDTH);
                    video_height = rtsp_capture.get(cv::CAP_PROP_FRAME_HEIGHT);
                    fps = rtsp_capture.get(cv::CAP_PROP_FPS);
                    
                    original_fps = fps;
                    original_width = video_width;
                    original_height = video_height;

                    // stream_info_hooker activated if need
                    if (stream_info_hooker) {
                        vp_stream_info stream_info {channel_index, fps, video_width, video_height, rtsp_url};
                        stream_info_hooker(node_name, stream_info);
                    }
                }

                rtsp_capture >> frame;
                if(frame.empty()) {
                    std::cout << "frame is empty, total frame==>" << this->frame_index << std::endl;
                    continue;
                }

                cv::Mat resize_frame;
                if (this->resize_ratio != 1.0f) {                 
                    cv::resize(frame, resize_frame, cv::Size(), resize_ratio, resize_ratio);
                }
                else {
                    resize_frame = frame.clone(); // clone!;
                }

                this->frame_index++;
                // create frame meta
                auto out_meta = 
                    std::make_shared<vp_objects::vp_frame_meta>(resize_frame, this->frame_index, this->channel_index, video_width, video_height, fps);

                if (out_meta != nullptr) {
                    //std::cout << this->node_name << ", handle meta, before out_queue.size()==>" << this->out_queue.size() << std::endl;
                    this->out_queue.push(out_meta);
                    
                    // handled hooker activated if need
                    if (this->meta_handled_hooker) {
                        meta_handled_hooker(node_name, out_queue.size(), out_meta);
                    }

                    // important! notify consumer of out_queue in case it is waiting.
                    this->out_queue_semaphore.signal();
                    std::cout << this->node_name << ", handle meta, after out_queue.size()==>" << this->out_queue.size() << std::endl;
                }
            }
            else {
                std::this_thread::sleep_for(std::chrono::microseconds{1000});
            }        
        }
    }

    // return stream url
    std::string vp_rtsp_src_node::to_string() {
        return rtsp_url;
    }
}