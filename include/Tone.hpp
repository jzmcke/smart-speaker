#include <stdlib.h>

class Tone
{
    public:
        Tone(int block_size_ms
            ,int sample_rate_hz
            ,float frequency_hz
            );
        
        float *m_p_tone;
        float* gen(float vol);
    private:
       uint32_t m_sample_idx;
       int m_sample_rate_hz;
       int m_block_size_ms;
       int m_block_size_samples;
       float m_frequency_hz;
};


class Ramp
{
    public:
        Ramp(int block_size_ms
            ,int sample_rate_hz
            ,float step_hz
            ,int step_max
            ,int driver_bitwidth
            );
        
        float *m_p_ramp;
        float* gen();
    private:
       uint32_t m_sample_idx;
       int m_sample_rate_hz;
       int m_block_size_ms;
       int m_block_size_samples;
       int m_n_samples_per_step;
       float m_step_hz;
       int m_step_val;
       int m_step_max;
       int m_max_int;
};


