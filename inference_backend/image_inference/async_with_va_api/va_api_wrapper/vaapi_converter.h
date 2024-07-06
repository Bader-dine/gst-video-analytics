/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "vaapi_context.h"
#include "vaapi_images.h"

#include <memory>

#include <va/va.h>

namespace InferenceBackend {

class VaApiConverter {
    VaApiContext *_context;

  public:
    explicit VaApiConverter(VaApiContext *context);
    ~VaApiConverter() = default;

    void Convert(const Image &src, InferenceBackend::VaApiImage &va_api_dst);
};

} // namespace InferenceBackend
