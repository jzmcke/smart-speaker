#ifndef TOUCHINPUT_H
#define TOUCHINPUT_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "touch_element.h"
#include <functional>

#define TOUCH_SWITCH_PIN 11
#define TOUCH_THRESHOLD 2500
#define MAX_TOUCH_THRESHOLD_RATIO 0.9 
#define MAX_TOUCH_THRESHOLD_DECAY_TIME 1000
#define MAX_TOUCH_THRESHOLD_DECAY_RATE 0.2

typedef struct slider_cb_cfg_s
{
  void (*on_slide_release)(int);
  void (*on_slide_move)(int);
  void (*on_hold)(int);
  void (*on_hold_release)(int);
  void (*on_press)(int);
} slider_cb_cfg_t;


class Slider {
public:
    enum State {
        IDLE,
        PRESSED,
        HOLDING,
        RELEASED,
        SLIDING
    };

    Slider();
    void handleTouchEvent(touch_elem_message_t element_message, slider_cb_cfg_t *p_cfg);
    static void event_handler_task(void *arg);
    void init(slider_cb_cfg_t *p_cfg);

private:
    State m_state;
    int m_startPos;
    long m_startTime;
};



void touch_init();

#endif