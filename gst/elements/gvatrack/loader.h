/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "inference_backend/logger.h"
#include <dlfcn.h>
#include <stdexcept>
#include <string>

using Handle = void *;

class Loader {
    Handle handle;

  public:
    Loader(const std::string &path) {
        if (path.empty()) {
            throw std::runtime_error("Loader: Library path is empty");
        }
        handle = dlopen(path.c_str(), RTLD_NOW);
        if (!handle) {
            auto dlerror_mess = dlerror();
            std::string message = (dlerror_mess) ? std::string("dlopen() failed: ") + std::string(dlerror_mess)
                                                 : std::string("dlopen() failed");
            throw std::runtime_error(message);
        }
    }

    ~Loader() {
        // FIXME: workaround related to crash during dlclose(). Remove when crash is fixed.
        g_usleep(1000000);
        if (dlclose(handle) != 0) {
            auto dlerror_mess = dlerror();
            std::string message = (dlerror_mess) ? std::string("dlclose() failed: ") + std::string(dlerror_mess)
                                                 : std::string("dlclose() failed");
            GVA_WARNING(message.c_str());
        }
    }

    template <typename Type>
    Type load(const std::string &name) {
        Handle symbol = dlsym(handle, name.c_str());
        if (dlerror()) {
            std::string message = std::string("Error during symbol loading: ") + name + std::string("\n") + dlerror();
            throw std::runtime_error(message);
        }
        return reinterpret_cast<Type>(symbol);
    }
};
