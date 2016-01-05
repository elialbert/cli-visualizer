/*
 * PulseAudioSource.cpp
 *
 * Created on: Jul 30, 2015
 *     Author: dpayne
 */

#include "Source/PulseAudioSource.h"
#include "Utils/Logger.h"
#include <string.h>
#include <iostream>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _LINUX
#include <unistd.h>
#endif

namespace
{
#ifdef _ENABLE_PULSE
static const char k_record_stream_name[] = "vis";
static const char k_record_stream_description[] = "vis stream";

static const int32_t k_sample_rate = 44100;
static const int32_t k_channels = 2;
#endif
}

vis::PulseAudioSource::PulseAudioSource(const Settings *const settings)
#ifdef _ENABLE_PULSE
    : m_settings{settings}, m_pulseaudio_simple
{
    nullptr
}
#endif
{
    settings->get_sampling_frequency();
}

bool vis::PulseAudioSource::open_pulseaudio_source(
    const uint32_t max_buffer_size)
{
#ifdef _ENABLE_PULSE
    int32_t error_code;

    static const pa_sample_spec sample_spec = {PA_SAMPLE_S16LE, k_sample_rate,
                                               k_channels};

    static const pa_buffer_attr buffer_attr = {max_buffer_size, 0, 0, 0,
                                               (max_buffer_size / 2)};

    auto audio_device = m_settings->get_pulse_audio_source();

    if (audio_device.empty())
    {
        m_pulseaudio_simple =
            pa_simple_new(nullptr, k_record_stream_name, PA_STREAM_RECORD,
                          nullptr, k_record_stream_description, &sample_spec,
                          nullptr, &buffer_attr, &error_code);

        // if using default still did not work, try again with a common device
        // name
        if (m_pulseaudio_simple == nullptr)
        {
            m_pulseaudio_simple =
                pa_simple_new(nullptr, k_record_stream_name, PA_STREAM_RECORD,
                              "0", k_record_stream_description, &sample_spec,
                              nullptr, &buffer_attr, &error_code);
        }
    }
    else
    {
        m_pulseaudio_simple =
            pa_simple_new(nullptr, k_record_stream_name, PA_STREAM_RECORD,
                          audio_device.c_str(), k_record_stream_description,
                          &sample_spec, nullptr, &buffer_attr, &error_code);
    }

    if (m_pulseaudio_simple != nullptr)
    {
        return true;
    }

    VIS_LOG(vis::LogLevel::ERROR, "Could not open pulseaudio source %s: %s",
            audio_device.c_str(), pa_strerror(error_code));

    return false;
#else
    // needed to make the compiler happy
    return max_buffer_size == 0;
#endif
}

bool vis::PulseAudioSource::read(pcm_stereo_sample *buffer,
                                 const uint32_t buffer_size)
{
    size_t buffer_size_bytes =
        static_cast<size_t>(sizeof(pcm_stereo_sample) * buffer_size);

#ifdef _ENABLE_PULSE

    if (m_pulseaudio_simple == nullptr)
    {
        open_pulseaudio_source(static_cast<uint32_t>(buffer_size_bytes));
    }

    if (m_pulseaudio_simple != nullptr)
    {
        int32_t error_code;
        /* Record some data ... */
        auto return_code = pa_simple_read(m_pulseaudio_simple, buffer,
                                          buffer_size_bytes, &error_code);

        if (return_code < 0)
        {
            VIS_LOG(vis::LogLevel::WARN, "Could not finish reading pulse audio "
                                         "stream buffer, bytes read: %d buffer "
                                         "size: ",
                    return_code, buffer_size_bytes);

            // zero out buffer
            memset(buffer, 0, buffer_size_bytes);

            pa_simple_free(m_pulseaudio_simple);
            m_pulseaudio_simple = nullptr;

            return false;
        }

        // Success fully read entire buffer
        return true;
    }
#endif
    // zero out buffer
    memset(buffer, 0, buffer_size_bytes);

    return false;
}

vis::PulseAudioSource::~PulseAudioSource()
{
#ifdef _ENABLE_PULSE
    if (m_pulseaudio_simple != nullptr)
    {
        pa_simple_free(m_pulseaudio_simple);
    }
#endif
}
