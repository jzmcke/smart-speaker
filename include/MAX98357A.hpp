#include <stdlib.h>
#include "FreeRTOS.h"
#include "freertos/queue.h"
#define MAX98357A_ERR (-1)
#define MAX98357A_OK  (0)


class MAX98357A
{
    public:
        MAX98357A(int i2s_port_num
                 ,int pin_sdi
                 ,int pin_bclk
                 ,int pin_lrck
                 ,int sample_rate_hz
                 ,int spkr_write_cadence_ms);

        int write(float *p_data_ch1, float *p_data_ch2);
        bool m_b_configured = false;
        size_t m_send_size;
        int m_n_samples_per_ch;
        bool b_is_output_dma_empty(void);

    private:
        int m_pin_sdo;
        int m_pin_bclk;
        int m_pin_lrck;
        int m_sample_rate_hz;
        int m_n_target_bytes_write;
        int m_n_dma_buffers;
        int m_i2s_port_num;
        bool m_b_ready_to_fill;
        unsigned char *m_p_audio_send_bytes;
        QueueHandle_t m_evt_queue;
};
