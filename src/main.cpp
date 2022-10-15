#include <Arduino.h>

extern "C"
{
#include "wifi_esp.h"
#include <blob.h>
}

#include "SPH0645.hpp"
#include "MAX98357A.hpp"
#include "Tone.hpp"

#define I2S_MIC_LRCK 4
#define I2S_MIC_DIN 7
#define I2S_MIC_BCLK 6

#define I2S_SPKR_LRCK 42
#define I2S_SPKR_BCLK 41
#define I2S_SPKR_DOUT 40


#define SAMPLE_RATE_HZ  (32000)
#define LOOP_DELAY_MS   (10)
#define N_SAMPLES_PER_LOOP (SAMPLE_RATE_HZ * LOOP_DELAY_MS / 1000)

#define TONE_FREQUENCY_HZ (330.0f)
#define TONE_VOL        (0.01)

SPH0645 *p_sph0645_mic;
MAX98357A *p_max98357a_spkr;
Tone *p_tone;


void setup() {
    Serial.begin(115200);
    p_sph0645_mic = new SPH0645(0, I2S_MIC_DIN, I2S_MIC_BCLK, I2S_MIC_LRCK, SAMPLE_RATE_HZ, LOOP_DELAY_MS);
    p_max98357a_spkr = new MAX98357A(1, I2S_SPKR_DOUT, I2S_SPKR_BCLK, I2S_SPKR_LRCK, SAMPLE_RATE_HZ, LOOP_DELAY_MS);
    p_tone = new Tone(LOOP_DELAY_MS, SAMPLE_RATE_HZ, TONE_FREQUENCY_HZ);

    wifi_connect();
    BLOB_INIT("ws://192.168.50.115", 8000);
}

void loop() {
    int rcv_size, snd_size;
    static int sample_idx = 0;
    static unsigned int old_time_taken = 0;
    static unsigned int time_to_log = 0;
    static unsigned int time_to_flush = 0;
    static unsigned int timer = 0;
    unsigned int start_time = micros();
    int delay_us = 0;
    float *p_tone_samples;

    p_tone_samples = p_tone->gen(TONE_VOL);
    p_sph0645_mic->read();
    p_max98357a_spkr->write(p_tone_samples, p_tone_samples);

    rcv_size = (int)p_sph0645_mic->m_rcv_size;
    snd_size = (int)p_max98357a_spkr->m_send_size;
    BLOB_START("main");
    BLOB_INT_A("rcv_bytes", &rcv_size, 1);
    BLOB_FLOAT_A("audio", p_sph0645_mic->m_p_ch1, 1);
    BLOB_INT_A("send_bytes", &snd_size, 1);
    BLOB_UNSIGNED_INT_A("total_time_us", &old_time_taken, 1);
    BLOB_UNSIGNED_INT_A("send_bin_time_us", &time_to_flush, 1);
    time_to_log = micros() - start_time;
    BLOB_FLUSH();
    time_to_flush = micros() - start_time - time_to_log;

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