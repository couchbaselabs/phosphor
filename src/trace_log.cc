/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 *     Copyright 2016 Couchbase, Inc
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

#include <exception>

#include "trace_log.h"

namespace phosphor {

    /* Number of bytes in a megabyte */
    constexpr size_t megabyte = 1024 * 1024;

    /*
     * TraceConfig implementation
     */

    TraceConfig::TraceConfig(BufferMode _buffer_mode, size_t _buffer_size)
            : buffer_mode(_buffer_mode),
              buffer_size(_buffer_size) {

        switch (buffer_mode) {
            case BufferMode::fixed:
                buffer_factory = make_fixed_buffer;
                break;
            case BufferMode::ring:
                throw std::invalid_argument("Ring buffer not yet implemented");
                break;
            default:
                throw std::invalid_argument("Invalid buffer mode");
                break;
        }
    }

    TraceConfig::TraceConfig(trace_buffer_factory _buffer_factory,
                             size_t _buffer_size)
            : buffer_mode(BufferMode::custom),
              buffer_size(_buffer_size),
              buffer_factory(_buffer_factory) {
    }

    trace_buffer_factory *TraceConfig::getBufferFactory() const {
        return buffer_factory;
    }

    size_t TraceConfig::getBufferSize() const {
        return buffer_size;
    }

    /*
     * TraceLog implementation
     */

    TraceLog::TraceLog()
            : enabled(false) {
        shared_chunk.sentinel = new Sentinel;
    }

    TraceLog &TraceLog::getInstance() {
        // TODO: Not thread-safe on Windows
        static TraceLog log_instance;
        return log_instance;
    }

    void TraceLog::stop() {
        std::lock_guard<std::mutex> lh(mutex);
        stopUNLOCKED();
    }

    void TraceLog::stopUNLOCKED() {
        if (enabled.exchange(false)) {
            buffer->evictThreads();
        }
    }

    void TraceLog::start(const TraceConfig &_trace_config) {
        std::lock_guard<std::mutex> lh(mutex);
        trace_config = _trace_config;

        size_t buffer_size = (trace_config.getBufferSize() * megabyte) /
                             sizeof(TraceBufferChunk);

        buffer = trace_config.getBufferFactory()(generation++, buffer_size);
        enabled.store(true);
    }

    std::unique_ptr<TraceBuffer> TraceLog::getBuffer() {
        std::lock_guard<std::mutex> lh(mutex);
        if (enabled) {
            throw std::logic_error("Cannot get the current TraceBuffer while "
                                           "logging is enabled");
        }
        return std::move(buffer);
    }

    bool TraceLog::isEnabled() {
        return enabled;
    }

    void TraceLog::registerThread() {
        thread_chunk.sentinel = new Sentinel;
    }

    void TraceLog::deregisterThread(TraceLog &instance) {
        if (!thread_chunk.sentinel) {
            throw std::logic_error(
                    "This thread has not been previously registered");
        }

        if (thread_chunk.chunk) {
            std::lock_guard<std::mutex> lh(instance.mutex);
            if (instance.buffer) {
                instance.buffer->returnChunk(*thread_chunk.chunk);
            }
            instance.buffer->removeSentinel(*thread_chunk.sentinel);
        }
        delete thread_chunk.sentinel;
    }

    void TraceLog::replaceChunk(ChunkTenant &ct) {
        ct.sentinel->release();
        std::lock_guard<std::mutex> lh(mutex);
        ct.sentinel->acquire();
        if (!enabled) {
            ct.chunk = nullptr;
            return;
        }

        if (ct.chunk) {
            buffer->returnChunk(*ct.chunk);
            ct.chunk = nullptr;
        }
        if (buffer && !buffer->isFull()) {
            ct.chunk = &buffer->getChunk(*ct.sentinel);
        } else {
            ct.sentinel->release();
            stopUNLOCKED();
        }
    }

    void TraceLog::resetChunk(ChunkTenant &ct) {
        if (ct.sentinel->reopen()) {
            ct.chunk = nullptr;
            ct.sentinel->release();
        }
    }

    THREAD_LOCAL TraceLog::ChunkTenant TraceLog::thread_chunk;
    TraceLog::ChunkTenant TraceLog::shared_chunk;

}