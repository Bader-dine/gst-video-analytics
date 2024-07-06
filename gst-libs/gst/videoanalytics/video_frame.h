/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

/**
 * @file video_frame.h
 * @brief This file contains GVA::VideoFrame class to control particular inferenced frame and attached
 * GVA::RegionOfInterest and GVA::Tensor instances.
 */

#pragma once

#include <algorithm>
#include <assert.h>
#include <functional>
#include <gst/gstbuffer.h>
#include <gst/video/gstvideometa.h>
#include <gst/video/video.h>
#include <memory>
#include <opencv2/opencv.hpp>
#include <stdexcept>
#include <string>
#include <vector>

#include "metadata/gva_json_meta.h"
#include "metadata/gva_tensor_meta.h"
#include "region_of_interest.h"

namespace GVA {

/**
 * @brief This class represents video frame - object for working with RegionOfInterest and Tensor objects which
 * belong to this video frame (image). RegionOfInterest describes detected object (bounding boxes) and its Tensor
 * objects (inference results on RegionOfInterest level). Tensor describes inference results on VideoFrame level.
 * VideoFrame also provides access to underlying GstBuffer and GstVideoInfo describing frame's video information (such
 * as image width, height, channels, strides, etc.). You also can get cv::Mat object representing this video frame.
 */
class VideoFrame {
  protected:
    /**
     * @brief GstBuffer with inference results metadata attached (Gstreamer pipeline's GstBuffer, which is output of GVA
     * inference elements, such as gvadetect, gvainference, gvaclassify)
     */
    GstBuffer *buffer;

    /**
     * @brief GstVideoInfo containing actual video information for this VideoFrame
     */
    std::unique_ptr<GstVideoInfo, std::function<void(GstVideoInfo *)>> info;

  public:
    /**
     * @brief Construct VideoFrame instance from GstBuffer and GstVideoInfo. This is preferred way of creating
     * VideoFrame
     * @param buffer GstBuffer* to which metadata is attached and retrieved
     * @param info GstVideoInfo* containing video information
     */
    VideoFrame(GstBuffer *buffer, GstVideoInfo *info)
        : buffer(buffer), info(gst_video_info_copy(info), gst_video_info_free) {
        if (not buffer or not info) {
            throw std::invalid_argument("GVA::VideoFrame: buffer or info nullptr");
        }
    }

    /**
     * @brief Construct VideoFrame instance from GstBuffer and GstCaps
     * @param buffer GstBuffer* to which metadata is attached and retrieved
     * @param caps GstCaps* from which video information is obtained
     */
    VideoFrame(GstBuffer *buffer, const GstCaps *caps) : buffer(buffer) {
        if (not buffer or not caps) {
            throw std::invalid_argument("GVA::VideoFrame: buffer or caps nullptr");
        }
        info = std::unique_ptr<GstVideoInfo, std::function<void(GstVideoInfo *)>>(gst_video_info_new(),
                                                                                  gst_video_info_free);
        if (!gst_video_info_from_caps(info.get(), caps)) {
            throw std::runtime_error("GVA::VideoFrame: gst_video_info_from_caps failed");
        }
    }

    /**
     * @brief Construct VideoFrame instance from GstBuffer. Video information will be obtained from buffer. This is
     * not recommended way of creating VideoFrame, because it relies on GstVideoMeta which can be absent for the
     * buffer
     * @param buffer GstBuffer* to which metadata is attached and retrieved
     */
    VideoFrame(GstBuffer *buffer) : buffer(buffer) {
        if (not buffer)
            throw std::invalid_argument("GVA::VideoFrame: buffer is nullptr");

        GstVideoMeta *meta = video_meta();
        if (not meta)
            throw std::logic_error("GVA::VideoFrame: video_meta() is nullptr");

        info = std::unique_ptr<GstVideoInfo, std::function<void(GstVideoInfo *)>>(gst_video_info_new(),
                                                                                  gst_video_info_free);
        if (not info.get())
            throw std::logic_error("GVA::VideoFrame: gst_video_info_new() failed");

        info->width = meta->width;
        info->height = meta->height;

        // Perform secure assignment of buffer similar to memcpy_s
        if (meta->stride == NULL || sizeof(info->stride) < sizeof(meta->stride)) {
            memset(info->stride, 0, sizeof(info->stride));
            throw std::logic_error("GVA::VideoFrame: stride copy failed");
        }

        memcpy(info->stride, meta->stride, sizeof(meta->stride));
    }

    /**
     * @brief Get video metadata of buffer
     * @return GstVideoMeta of buffer, nullptr if no GstVideoMeta available
     */
    GstVideoMeta *video_meta() {
        return gst_buffer_get_video_meta(buffer);
    }

    /**
     * @brief Get GstVideoInfo of this VideoFrame. This is preferrable way of getting any image information
     * @return GstVideoInfo of this VideoFrame
     */
    GstVideoInfo *video_info() {
        return info.get();
    }

    /**
     * @brief Get RegionOfInterest objects attached to VideoFrame
     * @return vector of RegionOfInterest objects attached to VideoFrame
     */
    std::vector<RegionOfInterest> regions() {
        return get_regions();
    }

    /**
     * @brief Get RegionOfInterest objects attached to VideoFrame
     * @return vector of RegionOfInterest objects attached to VideoFrame
     */
    const std::vector<RegionOfInterest> regions() const {
        return get_regions();
    }

    /**
     * @brief Get Tensor objects attached to VideoFrame
     * @return vector of Tensor objects attached to VideoFrame
     */
    std::vector<Tensor> tensors() {
        return get_tensors();
    }

    /**
     * @brief Get Tensor objects attached to VideoFrame
     * @return vector of Tensor objects attached to VideoFrame
     */
    const std::vector<Tensor> tensors() const {
        return get_tensors();
    }

    /**
     * @brief Get messages attached to this VideoFrame
     * @return messages attached to this VideoFrame
     */
    std::vector<std::string> messages() {
        std::vector<std::string> json_messages;
        GstGVAJSONMeta *meta = NULL;
        gpointer state = NULL;
        GType meta_api_type = g_type_from_name(GVA_JSON_META_API_NAME);
        while ((meta = (GstGVAJSONMeta *)gst_buffer_iterate_meta_filtered(buffer, &state, meta_api_type))) {
            json_messages.emplace_back(meta->message);
        }
        return json_messages;
    }

    /**
     * @brief Attach RegionOfInterest to this VideoFrame. This function takes ownership of region_tensor, if passed
     * @param x x coordinate of the upper left corner of bounding box
     * @param y y coordinate of the upper left corner of bounding box
     * @param w width of the bounding box
     * @param h height of the bounding box
     * @param label object label
     * @param confidence detection confidence
     * @param normalized if False, bounding box coordinates are pixel coordinates in range from 0 to image width/height.
    if True, bounding box coordinates normalized to [0,1] range.
     * @return new RegionOfInterest instance
     */
    RegionOfInterest add_region(double x, double y, double w, double h, std::string label = std::string(),
                                double confidence = 0.0, bool normalized = false) {
        if (!normalized) {
            if (info->width == 0 or info->height == 0) {
                throw std::logic_error("Failed to normalize coordinates width/height equal to 0");
            }
            x /= info->width;
            y /= info->height;
            w /= info->width;
            h /= info->height;
        }

        clip_normalized_rect(x, y, w, h);

        // absolute coordinates
        double _x = x * info->width + 0.5;
        double _y = y * info->height + 0.5;
        double _w = w * info->width + 0.5;
        double _h = h * info->height + 0.5;
        GstVideoRegionOfInterestMeta *meta = gst_buffer_add_video_region_of_interest_meta(
            buffer, label.c_str(), double_to_uint(_x), double_to_uint(_y), double_to_uint(_w), double_to_uint(_h));

        // Add detection tensor
        GstStructure *detection =
            gst_structure_new("detection", "x_min", G_TYPE_DOUBLE, x, "x_max", G_TYPE_DOUBLE, x + w, "y_min",
                              G_TYPE_DOUBLE, y, "y_max", G_TYPE_DOUBLE, y + h, NULL);
        if (confidence) {
            gst_structure_set(detection, "confidence", G_TYPE_DOUBLE, confidence, NULL);
        }
        gst_video_region_of_interest_meta_add_param(meta, detection);

        return RegionOfInterest(meta);
    }

    /**
     * @brief Attach empty Tensor to this VideoFrame
     * @return new Tensor instance
     */
    Tensor add_tensor() {
        const GstMetaInfo *meta_info = gst_meta_get_info(GVA_TENSOR_META_IMPL_NAME);
        GstGVATensorMeta *tensor_meta = (GstGVATensorMeta *)gst_buffer_add_meta(buffer, meta_info, NULL);

        return Tensor(tensor_meta->data);
    }

    /**
     * @brief Attach message to this VideoFrame
     * @param message message to attach to this VideoFrame
     */
    void add_message(const std::string &message) {
        const GstMetaInfo *meta_info = gst_meta_get_info(GVA_JSON_META_IMPL_NAME);
        GstGVAJSONMeta *json_meta = (GstGVAJSONMeta *)gst_buffer_add_meta(buffer, meta_info, NULL);
        json_meta->message = g_strdup(message.c_str());
    }

    /**
     * @brief Remove RegionOfInterest
     * @param roi the RegionOfInterest to remove
     */
    void remove_region(const RegionOfInterest &roi) {
        if (!gst_buffer_remove_meta(buffer, (GstMeta *)roi._meta())) {
            throw std::out_of_range("GVA::VideoFrame: RegionOfInterest doesn't belong to this frame");
        }
    }

    /**
     * @brief Remove Tensor
     * @param tensor the Tensor to remove
     */
    void remove_tensor(const Tensor &tensor) {
        GstGVATensorMeta *meta = NULL;
        gpointer state = NULL;
        while ((meta = GST_GVA_TENSOR_META_ITERATE(buffer, &state))) {
            if (meta->data == tensor._structure) {
                if (gst_buffer_remove_meta(buffer, (GstMeta *)meta))
                    return;
            }
        }
        throw std::out_of_range("GVA::VideoFrame: Tensor doesn't belong to this frame");
    }

  private:
    void clip_normalized_rect(double &x, double &y, double &w, double &h) {
        if (!((x >= 0) && (y >= 0) && (w >= 0) && (h >= 0) && (x + w <= 1) && (y + h <= 1))) {
            GST_DEBUG("ROI coordinates x=[%.5f, %.5f], y=[%.5f, %.5f] are out of range [0,1] and will be clipped", x,
                      x + w, y, y + h);

            x = (x < 0) ? 0 : (x > 1) ? 1 : x;
            y = (y < 0) ? 0 : (y > 1) ? 1 : y;
            w = (w < 0) ? 0 : (w > 1 - x) ? 1 - x : w;
            h = (h < 0) ? 0 : (h > 1 - y) ? 1 - y : h;
        }
    }

    unsigned int double_to_uint(double val) {
        unsigned int max = std::numeric_limits<unsigned int>::max();
        unsigned int min = std::numeric_limits<unsigned int>::min();
        return (val < min) ? min : ((val > max) ? max : static_cast<unsigned int>(val));
    }

    std::vector<RegionOfInterest> get_regions() const {
        std::vector<RegionOfInterest> regions;
        GstMeta *meta = NULL;
        gpointer state = NULL;

        while ((meta = gst_buffer_iterate_meta_filtered(buffer, &state, GST_VIDEO_REGION_OF_INTEREST_META_API_TYPE)))
            regions.emplace_back((GstVideoRegionOfInterestMeta *)meta);
        return regions;
    }

    std::vector<Tensor> get_tensors() const {
        std::vector<Tensor> tensors;
        GstGVATensorMeta *meta = NULL;
        gpointer state = NULL;
        GType meta_api_type = g_type_from_name("GstGVATensorMetaAPI");
        while ((meta = (GstGVATensorMeta *)gst_buffer_iterate_meta_filtered(buffer, &state, meta_api_type)))
            tensors.emplace_back(meta->data);
        return tensors;
    }
};

} // namespace GVA
