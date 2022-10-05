#include "SPH0645.hpp"
#include "driver/i2s.h"

#define BITWIDTH        (32)
#define BITS_PER_BYTE   (8)
#define BYTES_PER_SAMPLE (BITWIDTH / BITS_PER_BYTE)

#define N_DMA 2


SPH0645::SPH0645(int i2s_port_num
                ,int pin_sdi
                ,int pin_bclk
                ,int pin_lrck
                ,int sample_rate_hz
                ,int mic_read_cadence_ms)
{
    esp_err_t err = ESP_OK;
    i2s_pin_config_t i2s_pin_cfg = {0};
    i2s_config_t i2s_cfg;
    m_pin_sdi = pin_sdi;
    m_pin_bclk = pin_bclk;
    m_pin_lrck = pin_lrck;
    m_sample_rate_hz = sample_rate_hz;
    m_i2s_port_num = i2s_port_num;
    m_b_configured = false;
    m_rcv_size = 0;

    // Calculate the DMA buffer size based on the expected calling cadence
    m_n_samples_per_ch = mic_read_cadence_ms * m_sample_rate_hz / 1000;
    m_n_dma_buffers = N_DMA;
    m_n_target_bytes_read = 2 * m_n_samples_per_ch * BYTES_PER_SAMPLE;

    // Configure the relevant parameters for the pin usage of the I2S port.
    i2s_pin_cfg.bck_io_num = m_pin_bclk;
    i2s_pin_cfg.ws_io_num = m_pin_lrck;
    i2s_pin_cfg.data_in_num = m_pin_sdi;
    i2s_pin_cfg.data_out_num = m_pin_sdi;

    // Configure the given I2S bus to operate at the given sampling parameters
    i2s_cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
    i2s_cfg.sample_rate = m_sample_rate_hz;
    i2s_cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
    i2s_cfg.bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT;
    i2s_cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    i2s_cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    i2s_cfg.intr_alloc_flags = 0; // default interrupt priority
    i2s_cfg.dma_buf_count = m_n_dma_buffers;
    i2s_cfg.dma_buf_len = m_n_samples_per_ch;
    i2s_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_DEFAULT;
    i2s_cfg.use_apll = false;

    err |= i2s_driver_install((i2s_port_t)m_i2s_port_num, &i2s_cfg, 0, NULL);
    err |= i2s_set_pin((i2s_port_t)m_i2s_port_num, &i2s_pin_cfg);

    if (err == ESP_OK)
    {
        printf("Successfully configured i2s\n");
        m_b_configured = true;
    }
    else
    {
        printf("Failed to set up i2s\n");
    }

    m_p_audio_rcv_bytes = (unsigned char*)calloc(m_n_target_bytes_read, sizeof(unsigned char));
    m_p_audio_rcv_float = (float*)calloc(2 * m_n_samples_per_ch, sizeof(float));
    m_p_ch1 = &m_p_audio_rcv_float[0];
    m_p_ch2 =  &m_p_audio_rcv_float[m_n_samples_per_ch];
}

int SPH0645::read()
{
    int j = 0;
    int i = 0;
    int foo=0;

    i2s_read(I2S_NUM_0, (void*)m_p_audio_rcv_bytes, m_n_target_bytes_read, &m_rcv_size, portMAX_DELAY);
 
    for (i=0; i<(m_rcv_size/BYTES_PER_SAMPLE); i+=2)
    {
        // Throw away the 2 LSBs, no info there anyway
        // u_int16_t least_sig_byte = (u_int16_t)(*((u_int16_t*)&a_audio_rcv[i*4]));
        int16_t most_sig_byte = (int16_t)(*((int16_t*)&m_p_audio_rcv_bytes[i*BYTES_PER_SAMPLE + 2]));

        m_p_ch1[i/2] = 1.0 * (float)(most_sig_byte) / 32768;

        most_sig_byte = (int16_t)(*((int16_t*)&m_p_audio_rcv_bytes[(i+1)*BYTES_PER_SAMPLE + 2]));
        m_p_ch2[i/2] = 1.0 * (float)(most_sig_byte) / 32768;
    }

    for (j=i/2; j<m_n_samples_per_ch; j++)
    {
        m_p_ch1[j] = 0;
        m_p_ch2[j] = 0;
    }
    return SPH0645_OK;
}