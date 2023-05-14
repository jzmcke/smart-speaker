#include <Arduino.h>

extern "C"
{
#include "wifi_esp.h"
}

#include <Blob.hpp>

#include "SPH0645.hpp"
#include "MAX98357A.hpp"
#include "Tone.hpp"
#include "touchInput.h"
#include <math.h>

#define I2S_MIC_LRCK 4
#define I2S_MIC_DIN 7
#define I2S_MIC_BCLK 6

#define I2S_SPKR_LRCK 42
#define I2S_SPKR_BCLK 41
#define I2S_SPKR_DOUT 40

#define SAMPLE_RATE_HZ  (16000) 
#define LOOP_DELAY_MS   (10)

#define N_SAMPLES_PER_LOOP (SAMPLE_RATE_HZ * LOOP_DELAY_MS / 1000)
#define TONE_FREQUENCY_HZ (3000.0f)
#define TONE_VOL        (0.1f)
SPH0645 *p_sph0645_mic;
MAX98357A *p_max98357a_spkr;
Slider *p_slider;

Tone *p_ch1;
Tone *p_ch2;

// Ramp *p_ch1;
// Ramp *p_ch2;


float a_zeros[N_SAMPLES_PER_LOOP] = {0};
int b_wakeword_event = 0;
float gain = 1.0f;

void
mix(float *p_out, const float *p_arr1, const float *p_arr2, int n)
{
    for (int i = 0; i < n; i++)
    {
        p_out[i] = p_arr1[i] + p_arr2[i];
    }
}


void
amplify(float *p_in_out, float gain, int n)
{
    for (int i = 0; i < n; i++)
    {
        p_in_out[i] = gain *p_in_out[i];
    }
}

float
rms_calc(float *p_in_out, int n)
{
    float rms = 0;
    for (int i = 0; i < n; i++)
    {
        rms += p_in_out[i] * p_in_out[i];
    }
    return sqrtf(rms / n);
}

void onTimer()
{
    static const float *p_forward = NULL;
    float forward = 0;
    float a_out[N_SAMPLES_PER_LOOP] = {0};
    int n = 0;
    bool b_received_mic = false;
    float *p_ch1_tone, *p_ch2_tone;
    float rms = 0;
    if (p_sph0645_mic->b_is_input_dma_full())
    {
        b_received_mic = true;
        p_sph0645_mic->read();
    }    

    if (p_max98357a_spkr->b_is_output_dma_empty())
    {
        float *p_ch1_play, *p_ch2_play;
        BLOB_RECEIVE_START("main");
        BLOB_RECEIVE_FLOAT_A("forward", &p_forward, &n, 0);
        BLOB_RECEIVE_FLUSH();

        p_ch1_tone = p_ch1->gen(TONE_VOL);
        p_ch2_tone = p_ch2->gen(TONE_VOL);
        if (NULL != p_forward)
        {
            amplify((float*)p_forward, gain, N_SAMPLES_PER_LOOP);
            p_ch1_play = (float*)p_forward;
            p_ch2_play = (float*)p_forward;
        }
        else
        {
            p_ch1_play = a_zeros;
            p_ch2_play = a_zeros;
            // amplify((float*)p_ch1_tone, gain, N_SAMPLES_PER_LOOP);
            // amplify((float*)p_ch2_tone, gain, N_SAMPLES_PER_LOOP);
            // p_max98357a_spkr->write((float*)p_ch1_tone, (float*)p_ch2_tone);
        }
        p_max98357a_spkr->write(p_ch1_play, p_ch2_play);
        rms = rms_calc(p_ch1_play, N_SAMPLES_PER_LOOP);
    }

    
    if (b_received_mic)
    {
        /* This network write operation is tied to the microphone DMA tick (which should be the same as the speaker DMA tick
           if the same underlying timer is used */
        BLOB_START("main");
        BLOB_UNSIGNED_INT_A("rcv_bytes", &p_sph0645_mic->m_rcv_size, 1);
        BLOB_INT_A("n_rcved", (int*)&n, 1);
        BLOB_FLOAT_A("audio_ch1", (float*)p_sph0645_mic->m_p_ch1, N_SAMPLES_PER_LOOP);
        BLOB_FLOAT_A("audio_ch2", (float*)p_sph0645_mic->m_p_ch2, N_SAMPLES_PER_LOOP);
        BLOB_FLOAT_A("rms", &rms, 1);
        BLOB_FLUSH();
    }
}

void on_slide_release(int n)
{
    printf("on_slide_release: %d\n", n);
}

#define SLIDER_DB_RANGE (40.0f)
void on_slide_move(int n)
{
    gain = pow10f(((n / 100.0f) * SLIDER_DB_RANGE - SLIDER_DB_RANGE) / 20.0f);
}

void on_hold(int n)
{
    b_wakeword_event = 1;
    printf("on_hold: %d\n", n);
}

void on_hold_release(int n)
{
    b_wakeword_event = 0;
    printf("on_hold_release: %d\n", n);
}

void on_press(int n)
{
    printf("on_press: %d\n", n);
}

slider_cb_cfg_t slider_cb_cfg = {
    .on_slide_release = on_slide_release,
    .on_slide_move = on_slide_move,
    .on_hold = on_hold,
    .on_hold_release = on_hold_release,
    .on_press = on_press
};

void setup() {
    Serial.begin(115200);
    p_sph0645_mic = new SPH0645(0, I2S_MIC_DIN, I2S_MIC_BCLK, I2S_MIC_LRCK, SAMPLE_RATE_HZ, LOOP_DELAY_MS);
    p_max98357a_spkr = new MAX98357A(1, I2S_SPKR_DOUT, I2S_SPKR_BCLK, I2S_SPKR_LRCK, SAMPLE_RATE_HZ, LOOP_DELAY_MS);
    p_ch1 = new Tone(LOOP_DELAY_MS, SAMPLE_RATE_HZ, 300);
    p_ch2 = new Tone(LOOP_DELAY_MS, SAMPLE_RATE_HZ, 1000);

    wifi_connect();
    p_slider = new Slider();
    p_slider->init(&slider_cb_cfg);
    BLOB_INIT(192, 168, 0, 2, 3456, 8);
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

}
