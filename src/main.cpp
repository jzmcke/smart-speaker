#include <Arduino.h>

extern "C"
{
#include "wifi_esp.h"
}
#include "blob.hpp"
#include "SPH0645.hpp"
#include "MAX98357A.hpp"

#define I2S_MIC_LRCK 4
#define I2S_MIC_DIN 7
#define I2S_MIC_BCLK 6

#define I2S_SPKR_LRCK 42
#define I2S_SPKR_BCLK 41
#define I2S_SPKR_DOUT 40


#define SAMPLE_RATE_HZ  (32000)
#define LOOP_DELAY_MS   (10)
#define N_SAMPLES_PER_LOOP (SAMPLE_RATE_HZ * LOOP_DELAY_MS / 1000)
#define MAX_POS_INT 2147483647
// #define MAX_POS_INT 32767

unsigned int global_time;

SPH0645 *p_sph0645_mic;
MAX98357A *p_max98357a_spkr;

float a_output_samples[N_SAMPLES_PER_LOOP] = {0};
float a_output_samples_ch2[N_SAMPLES_PER_LOOP] = {0};

void setup() {
    
    
    esp_err_t err;
    char ipaddr[] = "ws://192.168.50.115";
    Serial.begin(115200);
    p_sph0645_mic = new SPH0645(0, I2S_MIC_DIN, I2S_MIC_BCLK, I2S_MIC_LRCK, SAMPLE_RATE_HZ, LOOP_DELAY_MS);
    p_max98357a_spkr = new MAX98357A(1, I2S_SPKR_DOUT, I2S_SPKR_BCLK, I2S_SPKR_LRCK, SAMPLE_RATE_HZ, LOOP_DELAY_MS);
    wifi_connect();
    BLOB_SOCKET_INIT(ipaddr, 8000);
}

void loop() {
    static int i = 0;
    int s;
    int rcv_size, snd_size;
    static int sample_idx = 0;
    static unsigned int old_time_taken = 0;
    static unsigned int time_to_log = 0;
    static unsigned int time_to_flush = 0;
    static unsigned int time_to_read_mic = 0;
    static unsigned int timer = 0;
    unsigned int start_time = micros();
    int delay_us = 0;

    for (s=0; s<N_SAMPLES_PER_LOOP; s++)
    {
        a_output_samples[s] = 0.0 * sinf(2.0f * M_PI * 1000 * sample_idx/SAMPLE_RATE_HZ);
        sample_idx = (sample_idx + 1) % 320000;
    }

    p_sph0645_mic->read();
    p_max98357a_spkr->write(a_output_samples, a_output_samples);
    rcv_size = (int)p_sph0645_mic->m_rcv_size;
    snd_size = (int)p_max98357a_spkr->m_send_size;
    time_to_read_mic = micros() - start_time;
    BLOB_START("main");
    BLOB_INT_A("rcv_bytes", &rcv_size, 1);
    BLOB_FLOAT_A("audio", p_sph0645_mic->m_p_ch1, 1);
    BLOB_INT_A("send_bytes", &snd_size, 1);
    BLOB_UNSIGNED_INT_A("time_to_read_mic_us", &time_to_read_mic, 1);
    BLOB_UNSIGNED_INT_A("time_to_log_us", &time_to_log, 1);
    BLOB_UNSIGNED_INT_A("time_to_flush", &time_to_flush, 1);
    BLOB_UNSIGNED_INT_A("total_time_us", &old_time_taken, 1);
    BLOB_UNSIGNED_INT_A("send_bin_time_us", &global_time, 1);
    time_to_log = micros() - time_to_read_mic - start_time;
    BLOB_FLUSH();
    time_to_flush = micros() - start_time - time_to_read_mic - time_to_log;

    timer = micros() - start_time;
    if (1000 * LOOP_DELAY_MS - (int)timer > 0)
    {
        delay_us = 1000 * LOOP_DELAY_MS - timer;
    }
    else
    {
        delay_us = 0;
    }
    delayMicroseconds(delay_us);
    old_time_taken = micros() - start_time;
}