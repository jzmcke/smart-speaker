#include <Arduino.h>
#include <I2S.h>

extern "C"
{
#include "wifi_esp.h"
}
// #include "setup_wifi.h"
#include "blob.hpp"

#define I2S_LRCK 4
#define I2S_DOUT 7
#define I2S_BCLK 6

#define SAMPLE_RATE_HZ  (16000)
#define LOOP_DELAY_MS   (10)

#define BITWIDTH        (32)
#define BYTES_PER_SAMPLE (BITWIDTH / 8)

#define N_BYTES  (4 * 2 * 10 * SAMPLE_RATE_HZ / 1000 * (BITWIDTH / 8))
#define N_INTS (N_BYTES / 4)
#define N_SAMPLES_PER_CHANNEL (N_INTS/2)

// I2SClass i2s(0, 0, I2S_DOUT, I2S_BCLK, I2S_LRCK);

int b_overflow = 0;
unsigned char a_audio_rcv[N_BYTES]; // 960 on average, 1920 to be safe
int a_audio_corrected_ch1[N_SAMPLES_PER_CHANNEL];
int a_audio_corrected_ch2[N_SAMPLES_PER_CHANNEL];
size_t rcv_size = 0;
unsigned int global_time;

void setup() {
    bool b_success = true;
    char ipaddr[] = "ws://192.168.50.115";
    // int n = WiFi.scanNetworks();
    // Serial.println("Found this many networks: ");
    // Serial.println(n);
    // put your setup code here, to run once:
    Serial.begin(115200);

    wifi_connect();
    b_success &= I2S.setFsPin(I2S_LRCK);
    b_success &= I2S.setSckPin(I2S_BCLK);
    b_success &= I2S.setDataPin(I2S_DOUT);
    b_success &= I2S.begin(I2S_PHILIPS_MODE, SAMPLE_RATE_HZ, BITWIDTH);
    if (~b_success)
    {
        Serial.println("I2S initialisation unsuccessful!");
    }
    else
    {
        Serial.println("I2S initialisation successful!");
    }

    BLOB_SOCKET_INIT(ipaddr, 8000);

}

void read_mic()
{
    int j = 0;
    int i = 0;
    int foo=0;
    rcv_size = I2S.available();
    b_overflow = 0;
    if (rcv_size < N_BYTES)
    {
        I2S.read((void*)a_audio_rcv, rcv_size);
    }
    else if (rcv_size >= N_BYTES)
    {
        I2S.read((void*)a_audio_rcv, N_BYTES);
        rcv_size = N_BYTES;
        b_overflow = 1;
    }
    foo=1;
    for (i=0; i<(rcv_size/BYTES_PER_SAMPLE); i+=2)
    {
        // Throw away the 2 LSBs, no info there anyway
        // u_int16_t least_sig_byte = (u_int16_t)(*((u_int16_t*)&a_audio_rcv[i*4]));
        int16_t most_sig_byte = (int16_t)(*((int16_t*)&a_audio_rcv[i*BYTES_PER_SAMPLE + 2]));
        a_audio_corrected_ch1[i/2] = (int)(most_sig_byte);

        most_sig_byte = (int16_t)(*((int16_t*)&a_audio_rcv[(i+1)*BYTES_PER_SAMPLE + 2]));
        a_audio_corrected_ch2[i/2] = (int)(most_sig_byte);
    }
    j = i;
    for (j=i; j<N_INTS/2; j++)
    {
        a_audio_corrected_ch1[j] = 0;
        a_audio_corrected_ch2[j] = 0;
    }
}

void loop() {
    static int i = 0;
    int rcv_bytes = (int)rcv_size;

    static unsigned int old_time_taken = 0;
    static unsigned int time_to_log = 0;
    static unsigned int time_to_flush = 0;
    static unsigned int time_to_read_mic = 0;
    static unsigned int timer = 0;
    unsigned int start_time = micros();
    int delay_us = 0;
    // put your main code here, to run repeatedly

    read_mic();
    time_to_read_mic = micros() - start_time;
    BLOB_START("main");
    BLOB_INT_A("rcv_bytes", &rcv_bytes, 1);
    BLOB_INT_A("b_overflow", &b_overflow, 1);
    BLOB_INT_A("audio", a_audio_corrected_ch1, 320);
    BLOB_INT_A("audio_samp2", &a_audio_corrected_ch1[1], 1);
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