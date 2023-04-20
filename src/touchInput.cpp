#include "touchInput.h"
#include "touch_slider.h"
#include "touch_button.h"
#include "driver/touch_sensor.h"
#include <Arduino.h>

bool touch_detected = false;
volatile unsigned long msSinceLastTouch = 0;

// Variables to track the minimum and maximum touch readings
int minTouchReading = 0;
int maxTouchReading = 0;

#define TOUCH_SLIDER_CHANNEL_NUM 6 // Touch slider channels number

static touch_slider_handle_t slider_handle; // Touch slider handle

static const touch_pad_t slider_channel_array[TOUCH_SLIDER_CHANNEL_NUM] = {
    // Touch slider channels array
    TOUCH_PAD_NUM9,
    TOUCH_PAD_NUM10,
    TOUCH_PAD_NUM11,
    TOUCH_PAD_NUM12,
    TOUCH_PAD_NUM13,
    TOUCH_PAD_NUM14,
};

static const float slider_channel_sens_array[TOUCH_SLIDER_CHANNEL_NUM] = {
    // Touch slider channels sensitivity array
    0.1F,
    0.1F,
    0.1F,
    0.1F,
    0.1F,
    0.1F,
};

int sliderPos = 100;

int getSliderPos(void)
{
  return sliderPos;
}

Slider::Slider()
{
  m_state = IDLE;
  m_startPos = 50;
  m_startTime = 0;
}

void Slider::handleTouchEvent(touch_elem_message_t element_message, slider_cb_cfg_t *p_cfg)
{
  const touch_slider_message_t *slider_message = touch_slider_get_message(&element_message);
  switch (m_state)
  {
  case IDLE:
    if (slider_message->event == TOUCH_SLIDER_EVT_ON_PRESS)
    {
      m_startTime = millis();
      m_startPos = slider_message->position;
      m_state = PRESSED;
      printf("startPos %d\n", m_startPos);
    }
    break;
  case PRESSED:
    if (slider_message->event == TOUCH_SLIDER_EVT_ON_CALCULATION)
    {
      int pos = slider_message->position;
      if (pos > m_startPos + 10 || pos < m_startPos - 10)
      {
        m_state = SLIDING;
        
      }
      else if (millis() - m_startTime > 1000)
      {
        m_state = HOLDING;
      }
    }
    else if (slider_message->event == TOUCH_SLIDER_EVT_ON_RELEASE)
    {
      int endPos = slider_message->position;
      if (endPos < m_startPos + 10 && endPos > m_startPos - 10)
      {
        printf("almost finished pressing\n");
        
        p_cfg->on_press(slider_message->position);
      }
      m_state = IDLE;
    }
    break;
  case HOLDING:
    if (slider_message->event == TOUCH_SLIDER_EVT_ON_CALCULATION)
    {
      p_cfg->on_hold(slider_message->position);
    }
    else if (slider_message->event == TOUCH_SLIDER_EVT_ON_RELEASE)
    {
      p_cfg->on_hold_release(slider_message->position);
      m_state = IDLE;
    }
    break;
  case SLIDING:
    if (slider_message->event == TOUCH_SLIDER_EVT_ON_CALCULATION)
    {
      p_cfg->on_slide_move(slider_message->position);
    }
    else if (slider_message->event == TOUCH_SLIDER_EVT_ON_RELEASE)
    {
      p_cfg->on_slide_release(slider_message->position);
      m_state = IDLE;
    }
    break;
  }
}

typedef struct handler_args
{
  slider_cb_cfg_t *p_cfg;
  Slider *p_slider;
} slider_handler_args_t;

slider_handler_args_t handler_args; // global for now

void Slider::init(slider_cb_cfg_t *p_cfg)
{
  touch_elem_global_config_t elem_global_config = TOUCH_ELEM_GLOBAL_DEFAULT_CONFIG();
  ESP_ERROR_CHECK(touch_element_install(&elem_global_config));
  printf("Touch element library installed\n");

  touch_slider_global_config_t slider_global_config = TOUCH_SLIDER_GLOBAL_DEFAULT_CONFIG();

  ESP_ERROR_CHECK(touch_slider_install(&slider_global_config));
  printf("Touch slider installed\n");
  /* Create Touch slider */
  touch_slider_config_t slider_config = {
      .channel_array = slider_channel_array,
      .sensitivity_array = slider_channel_sens_array,
      .channel_num = (sizeof(slider_channel_array) / sizeof(slider_channel_array[0])),
      .position_range = 100};

  handler_args.p_cfg = p_cfg;
  handler_args.p_slider = this;

  ESP_ERROR_CHECK(touch_slider_create(&slider_config, &slider_handle));
  /* Subscribe touch slider events (On Press, On Release, On Calculation) */
  ESP_ERROR_CHECK(touch_slider_subscribe_event(slider_handle, TOUCH_ELEM_EVENT_ON_PRESS | TOUCH_ELEM_EVENT_ON_RELEASE | TOUCH_ELEM_EVENT_ON_CALCULATION, NULL));
  /* Set EVENT as the dispatch method */
  ESP_ERROR_CHECK(touch_slider_set_dispatch_method(slider_handle, TOUCH_ELEM_DISP_EVENT));

  touch_element_start();
  /* Create a handler task to handle event messages */
  xTaskCreate(&Slider::event_handler_task, "touch_event_handler_task", 4 * 1024, (void*)&handler_args, 5, NULL);
}

void Slider::event_handler_task(void *arg)
{
  slider_handler_args_t *p_args = (slider_handler_args_t*)arg;
  touch_elem_message_t element_message;
  for (;;)
  {
    /* Waiting for touch element messages */
    touch_element_message_receive(&element_message, portMAX_DELAY);
    switch (element_message.element_type)
    {
    case TOUCH_ELEM_TYPE_SLIDER:
      p_args->p_slider->handleTouchEvent(element_message, p_args->p_cfg);
      break;
    default:
      printf("Unknown element message");
      break;
    }

    vTaskDelay(4 / portTICK_PERIOD_MS);
  }
}
