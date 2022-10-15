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
        m_sample_idx = (m_sample_idx + 1) % 320000;
    }
    return m_p_tone;
}