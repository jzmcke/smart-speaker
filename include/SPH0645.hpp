#include <stdlib.h>

#define SPH0645_ERR (-1)
#define SPH0645_OK  (0)


class SPH0645
{
    public:
        SPH0645(int i2s_port_num
               ,int pin_sdi
               ,int pin_bclk
               ,int pin_lrck
               ,int sample_rate_hz
               ,int mic_read_cadence_ms);
        
        int read();
        bool m_b_configured = false;
        float *m_p_ch1;
        float *m_p_ch2;
        size_t m_rcv_size;
        int m_n_samples_per_ch;
    private:
        int m_pin_sdi;
        int m_pin_bclk;
        int m_pin_lrck;
        int m_sample_rate_hz;
        int m_n_target_bytes_read;
        int m_n_dma_buffers;
        int m_i2s_port_num;
        unsigned char *m_p_audio_rcv_bytes;
        float *m_p_audio_rcv_float;
};
