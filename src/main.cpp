#include <Arduino.h>

extern "C"
{
#include "wifi_esp.h"
}

#include <Blob.hpp>

#include "SPH0645.hpp"
#include "MAX98357A.hpp"
#include "Tone.hpp"

#define I2S_MIC_LRCK 4
#define I2S_MIC_DIN 7
#define I2S_MIC_BCLK 6

#define I2S_SPKR_LRCK 42
#define I2S_SPKR_BCLK 41
#define I2S_SPKR_DOUT 40

#define SAMPLE_RATE_HZ  (16000)
#define LOOP_DELAY_MS   (10)
#define N_SAMPLES_PER_LOOP (SAMPLE_RATE_HZ * LOOP_DELAY_MS / 1000)
#define TONE_FREQUENCY_HZ (1000.0f)
// #define TONE_VOL        (0.001)
#define TONE_VOL        (0.0f)
SPH0645 *p_sph0645_mic;
MAX98357A *p_max98357a_spkr;
Tone *p_tone;

hw_timer_t *p_timer = NULL;

volatile float *p_tone_samples = NULL;

void onTimer()
{
    int n = 1;
    p_sph0645_mic->read();
    p_tone_samples = p_tone->gen(TONE_VOL);
    p_max98357a_spkr->write((float*)p_tone_samples, (float*)p_tone_samples);
    BLOB_START("main");
    // BLOB_UNSIGNED_INT_A("rcv_bytes", &p_sph0645_mic->m_rcv_size, 1);
    BLOB_FLOAT_A("audio", (float*)p_sph0645_mic->m_p_ch1, N_SAMPLES_PER_LOOP);
    // BLOB_UNSIGNED_INT_A("send_bytes", &p_max98357a_spkr->m_send_size, 1);
    BLOB_INT_A("n", &n, 1);
    BLOB_FLUSH();

}

void setup() {
    Serial.begin(115200);
    p_sph0645_mic = new SPH0645(0, I2S_MIC_DIN, I2S_MIC_BCLK, I2S_MIC_LRCK, SAMPLE_RATE_HZ, LOOP_DELAY_MS);
    p_max98357a_spkr = new MAX98357A(1, I2S_SPKR_DOUT, I2S_SPKR_BCLK, I2S_SPKR_LRCK, SAMPLE_RATE_HZ, LOOP_DELAY_MS);
    p_tone = new Tone(LOOP_DELAY_MS, SAMPLE_RATE_HZ, TONE_FREQUENCY_HZ);

    wifi_connect();

    // BLOB_INIT("ws://192.168.50.115", 8000);
    BLOB_INIT(192, 168, 50, 115, 1234, 2);

    // p_timer = timerBegin(0, 80, true);
    // timerAttachInterrupt(p_timer, &onTimer, true);
    // timerAlarmWrite(p_timer, 1000 * LOOP_DELAY_MS, true);
    // timerAlarmEnable(p_timer);

}

void loop()
{
    unsigned int time_taken = 0;
    unsigned int start_time = micros();
    int delay_us = 0;
    int n = 1;

    start_time = micros();
    onTimer();
    time_taken = micros() - start_time;
    // Delay the loop
    delay_us = LOOP_DELAY_MS * 1000 - time_taken;
    if (delay_us > 0)
    {
        delayMicroseconds(delay_us);
    }
}
