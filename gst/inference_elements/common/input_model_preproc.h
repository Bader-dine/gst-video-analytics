/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/gst.h>
#include <memory>
#include <string>

struct ModelInputProcessorInfo {
    using Ptr = std::shared_ptr<ModelInputProcessorInfo>;

    std::string format;
    std::string layer_name;
    GstStructure *params;

    ~ModelInputProcessorInfo() {
        if (params)
            gst_structure_free(params);
    }
};
