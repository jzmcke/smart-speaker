#include <stdlib.h>
#include "Tone.hpp"
#include "math.h"

Tone::Tone(int block_size_ms
          ,int sample_rate_hz
          ,float frequency_hz
          )
{
    m_block_size_samples = block_size_ms * sample_rate_hz  / 1000;
    m_block_size_ms = block_size_ms;
    m_sample_rate_hz = sample_rate_hz;
    m_frequency_hz = frequency_hz;
    m_p_tone = (float*)calloc(sizeof(float), m_block_size_samples);
    m_sample_idx = 0;
}

float *Tone::gen(float vol)
{
    int i;
    for (i=0; i<m_block_size_samples; i++)
    {
        m_p_tone[i] = vol * sinf(2.0f * M_PI * m_frequency_hz * m_sample_idx/m_sample_rate_hz);
        // Will overflow, but will only produce a glitch once per day at 48kHz.
        if (m_sample_idx >= 48000 * 100)
        {
            m_sample_idx = 0;
        }
        else
        {
            m_sample_idx++;
        }
    }
    return m_p_tone;
}


Ramp::Ramp(int block_size_ms
          ,int sample_rate_hz
          ,float step_hz
          ,int step_max
          ,int driver_bitwidth
          )
{
    m_block_size_samples = block_size_ms * sample_rate_hz  / 1000;
    m_block_size_ms = block_size_ms;
    m_sample_rate_hz = sample_rate_hz;
    m_step_hz = step_hz;
    m_n_samples_per_step = sample_rate_hz / step_hz;
    m_p_ramp = (float*)calloc(sizeof(float), m_block_size_samples);
    m_sample_idx = 0;
    m_step_val = 0;
    m_step_max = step_max;

    if (driver_bitwidth == 32)
    {
        m_max_int = 2147483647;
    }
    else if (driver_bitwidth == 16)
    {
        m_max_int = 32767;
    }
}

float *Ramp::gen()
{
    int i;
    for (i=0; i<m_block_size_samples; i++)
    {
        if (m_sample_idx >= m_n_samples_per_step)
        {
            m_sample_idx = 0;
            m_step_val = (m_step_val + 1) % m_step_max;
        }
        m_p_ramp[i] = m_step_val / (float)m_max_int;
        m_sample_idx++;
    }
    return m_p_ramp;
}