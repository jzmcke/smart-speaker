#include "MAX98357A.hpp"
#include <stdint.h>
// #include <functional>
#include "hal/i2s_types.h"
#include "driver/i2s.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"


// #define BITWIDTH32
#define BITWIDTH16

#ifdef BITWIDTH32
    #define BITWIDTH (32)
    #define MAX_POS_INT 2147483647
#else
    #define BITWIDTH (16)
    #define MAX_POS_INT 32767
#endif

#define BITS_PER_BYTE   (8)
#define BYTES_PER_SAMPLE (BITWIDTH / BITS_PER_BYTE)

#define N_DMA 2

#define QUEUE_SIZE (10)


MAX98357A::MAX98357A(int i2s_port_num
                    ,int pin_sdo
                    ,int pin_bclk
                    ,int pin_lrck
                    ,int sample_rate_hz
                    ,int spkr_write_cadence_ms)
{
    esp_err_t err = ESP_OK;
    int b_err = true;
    i2s_pin_config_t i2s_pin_cfg = {0};
    i2s_config_t i2s_cfg;
    m_pin_sdo = pin_sdo;
    m_pin_bclk = pin_bclk;
    m_pin_lrck = pin_lrck;
    m_sample_rate_hz = sample_rate_hz;
    m_i2s_port_num = i2s_port_num;
    m_b_configured = false;
    m_send_size = 0;

    // Calculate the DMA buffer size based on the expected calling cadence
    m_n_samples_per_ch = spkr_write_cadence_ms * m_sample_rate_hz / 1000;
    m_n_dma_buffers = N_DMA;

    // This needs to be 2x m_n_samples_per_ch when there are 2 channels
    m_n_target_bytes_write = 2 * m_n_samples_per_ch * BYTES_PER_SAMPLE;

    // Configure the relevant parameters for the pin usage of the I2S port.
    i2s_pin_cfg.bck_io_num = m_pin_bclk;
    i2s_pin_cfg.ws_io_num = m_pin_lrck;
    i2s_pin_cfg.data_in_num = 39;
    i2s_pin_cfg.data_out_num = m_pin_sdo;

    // Configure the given I2S bus to operate at the given sampling parameters
    i2s_cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    i2s_cfg.sample_rate = m_sample_rate_hz;
    i2s_cfg.bits_per_sample = (i2s_bits_per_sample_t)BITWIDTH;
    i2s_cfg.bits_per_chan = (i2s_bits_per_chan_t)BITWIDTH;
    i2s_cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    i2s_cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    i2s_cfg.intr_alloc_flags = 0; // default interrupt priority
    i2s_cfg.dma_buf_count = m_n_dma_buffers;
    i2s_cfg.dma_buf_len = m_n_samples_per_ch;
    i2s_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_DEFAULT;
    i2s_cfg.use_apll = false;

    err |= i2s_driver_install((i2s_port_t)m_i2s_port_num, &i2s_cfg, QUEUE_SIZE, &m_evt_queue);
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
    m_p_audio_send_bytes = (unsigned char*)calloc(m_n_target_bytes_write, sizeof(unsigned char));
}

bool MAX98357A::b_is_output_dma_empty()
{
    i2s_event_t evt;
    bool ret = false;
    if (xQueueReceive(m_evt_queue, (void*)&evt, 0) == pdTRUE)
    {
        if (evt.type == I2S_EVENT_DMA_ERROR)
        {
            printf("TX DMA err!\n");
        }
        else if (evt.type == I2S_EVENT_TX_DONE)
        {
            ret = true;
        }
        else if (evt.type == I2S_EVENT_TX_Q_OVF)
        {
            ret = true;
        }
    }
    return ret;
}

int MAX98357A::write(float *p_data_ch1, float *p_data_ch2)
{
    int i;

    for (i=0; i<(2*m_n_samples_per_ch); i+=2)
    {
        // i2s_write is little endian, meaning, the least significant byte should be written to the lowest address
        #ifdef BITWIDTH32
            int32_t int_samp_ch1 = (int32_t)(p_data_ch1[i/2] * MAX_POS_INT);
            int32_t int_samp_ch2 = (int32_t)(p_data_ch2[i/2] * MAX_POS_INT);
            m_p_audio_send_bytes[i*BYTES_PER_SAMPLE + 3] = (unsigned char)(((uint32_t)int_samp_ch1 >> 24) & 0xff);
            m_p_audio_send_bytes[i*BYTES_PER_SAMPLE + 2] = (unsigned char)(((uint32_t)int_samp_ch1 >> 16) & 0xff);
            m_p_audio_send_bytes[i*BYTES_PER_SAMPLE + 1] = (unsigned char)(((uint32_t)int_samp_ch1 >> 8) & 0xff);
            m_p_audio_send_bytes[i*BYTES_PER_SAMPLE + 0] = (unsigned char)(((uint32_t)int_samp_ch1 >> 0) & 0xff);
        
            m_p_audio_send_bytes[(i+1)*BYTES_PER_SAMPLE + 3] = (unsigned char)(((uint32_t)int_samp_ch2 >> 24) & 0xff);
            m_p_audio_send_bytes[(i+1)*BYTES_PER_SAMPLE + 2] = (unsigned char)(((uint32_t)int_samp_ch2 >> 16) & 0xff);
            m_p_audio_send_bytes[(i+1)*BYTES_PER_SAMPLE + 1] = (unsigned char)(((uint32_t)int_samp_ch2 >> 8) & 0xff);
            m_p_audio_send_bytes[(i+1)*BYTES_PER_SAMPLE + 0] = (unsigned char)(((uint32_t)int_samp_ch2 >> 0) & 0xff);
        #else
            int16_t int_samp_ch1 = (int16_t)(p_data_ch1[i/2] * MAX_POS_INT);
            int16_t int_samp_ch2 = (int16_t)(p_data_ch2[i/2] * MAX_POS_INT);
            m_p_audio_send_bytes[i*BYTES_PER_SAMPLE + 1] = (unsigned char)(((uint16_t)int_samp_ch1 >> 8) & 0xff);
            m_p_audio_send_bytes[i*BYTES_PER_SAMPLE + 0] = (unsigned char)(((uint16_t)int_samp_ch1 >> 0) & 0xff);
            m_p_audio_send_bytes[(i+1)*BYTES_PER_SAMPLE + 1] = (unsigned char)(((uint16_t)int_samp_ch2 >> 8) & 0xff);
            m_p_audio_send_bytes[(i+1)*BYTES_PER_SAMPLE + 0] = (unsigned char)(((uint16_t)int_samp_ch2 >> 0) & 0xff);
        #endif

    }
    i2s_write((i2s_port_t)m_i2s_port_num, (void*)m_p_audio_send_bytes, m_n_target_bytes_write, &m_send_size, portMAX_DELAY);
    m_b_ready_to_fill = false;
    return MAX98357A_OK;
}