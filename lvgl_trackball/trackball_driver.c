/**
 * @file lv_port_indev_templ.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "sdkconfig.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "trackball_driver.h"

/*********************
 *      DEFINES
 *********************/

#define TAG "trackball"

// Bypass configuration
#if CONFIG_LV_TRACKBALL
    #define LV_TRACKBALL_UP CONFIG_LV_TRACKBALL_UP
    #define LV_TRACKBALL_RIGHT CONFIG_LV_TRACKBALL_RIGHT
    #define LV_TRACKBALL_DOWN CONFIG_LV_TRACKBALL_DOWN
    #define LV_TRACKBALL_LEFT CONFIG_LV_TRACKBALL_LEFT
    #define LV_TRACKBALL_BUTTON CONFIG_LV_TRACKBALL_BUTTON
#else
    #error "Trackball is not configured"
#endif

/// Duration of silence before `Release` event, read cycle units
#define TRACKBALL_REL_THRESHOLD 5

/// Number of signal lines in trackball
#define TRACKBALL_LINE_MAX 5

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void trackball_init(void);
static void trackball_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);

/**********************
 *  STATIC VARIABLES
 **********************/
/// Pointer to instance of input device
static lv_indev_t * indev_trackball;

/// Instance of trackball driver
static lv_indev_drv_t trackball_drv;

/// Titles for trackball lines
static const char *line_name[] = {
    "Up", "Right", "Down", "Left", "Button"
};

/// List of GPIOs related to trackball
static const int trackball_gpio[] = {
    LV_TRACKBALL_UP,
    LV_TRACKBALL_RIGHT,
    LV_TRACKBALL_DOWN,
    LV_TRACKBALL_LEFT,
    LV_TRACKBALL_BUTTON
};

/// List of keys that are mapped to trackball lines (GPIOs)
static const uint32_t line_to_key_map[] = {
    LV_KEY_PREV,
    LV_KEY_RIGHT,
    LV_KEY_NEXT,
    LV_KEY_LEFT,
    LV_KEY_ENTER
};

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_trackball_init(void)
{    
    /* Initialize your trackball or keypad if you have */
    trackball_init();

    /* Register a trackball input device */
    lv_indev_drv_init(&trackball_drv);
    trackball_drv.type = LV_INDEV_TYPE_KEYPAD;
    trackball_drv.read_cb = trackball_read;
    indev_trackball = lv_indev_drv_register(&trackball_drv);

    if (NULL == indev_trackball) {
        ESP_LOGE(TAG, "Error during creating of trackball input device.");
    }
}

const lv_indev_t* lv_port_trackball_get_indev(void)
{
    return indev_trackball;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * Initialize your trackball hardware.
 */
static void trackball_init(void)
{
    /** @todo Your code comes here*/
    ESP_LOGI(TAG, "Initialization of trackball ...");

    ESP_LOGI(TAG, "Trackball GPIOs: Up <%d>, Right <%d>, Down <%d>, Left <%d>, Button <%d>", \
                LV_TRACKBALL_UP, LV_TRACKBALL_RIGHT, LV_TRACKBALL_DOWN, LV_TRACKBALL_LEFT, \
                LV_TRACKBALL_BUTTON);

    gpio_reset_pin(LV_TRACKBALL_UP);
    gpio_set_direction(LV_TRACKBALL_UP, GPIO_MODE_INPUT);

    gpio_reset_pin(LV_TRACKBALL_RIGHT);
    gpio_set_direction(LV_TRACKBALL_RIGHT, GPIO_MODE_INPUT);

    gpio_reset_pin(LV_TRACKBALL_DOWN);
    gpio_set_direction(LV_TRACKBALL_DOWN, GPIO_MODE_INPUT);

    gpio_reset_pin(LV_TRACKBALL_LEFT);
    gpio_set_direction(LV_TRACKBALL_LEFT, GPIO_MODE_INPUT);

    gpio_reset_pin(LV_TRACKBALL_BUTTON);
    gpio_set_direction(LV_TRACKBALL_BUTTON, GPIO_MODE_INPUT);
}

// static int key_state2[] = {0, 0, 0, 0, 0};

/**
 * Will be called by the library to read the trackball
 */
static void trackball_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    /** @note As a potential improvement, may be added some accumulating from toggle event
     * and provoking release event. Timeout still needed for cleaning gauge and filtering tails.
     */

    /** @bug There is an issue with initialization of values (fake pressing and releasing)
     * It would be great init in global init function.
     */

    static int line_state[] = {0, 0, 0, 0, 0};
    static int line_silence_cnt[] = {0, 0, 0, 0, 0};
    static int key_state[] = {0, 0, 0, 0, 0};

    static int i = 0;

    data->key = 0;
    data->state = LV_INDEV_STATE_REL;

    if (i < TRACKBALL_LINE_MAX) {
        int new_state = gpio_get_level(trackball_gpio[i]);

        if (new_state != line_state[i]) {
            // there is line toggling (rotations)
            line_state[i] = new_state;
            line_silence_cnt[i] = 0;        // reset silence counter once there is an event

            if (0 == key_state[i]) {
                ESP_LOGI(TAG, "Key <%s> pressed", line_name[i]);

                data->state = LV_INDEV_STATE_PR;
                data->key = line_to_key_map[i];
                
                key_state[i] = 1;
            }
        } else {
            // no toggling
            if (1 == key_state[i]) {
                if (line_silence_cnt[i] > TRACKBALL_REL_THRESHOLD) {
                    key_state[i] = 0;

                    // released state is not reported here because
                    // an one press event is expected for move on trackball
                    // (series of togglings are squeshed to one key press event)

                    ESP_LOGI(TAG, "Key <%s> released", line_name[i]);
                } else {
                    line_silence_cnt[i]++;
                }
            }
        }
        // there are pending lines for processing
        if (0 == key_state[i]) {
            // hold reading of other lines until releasing of already pressed
            data->continue_reading = true;
            i++;
        }
    }
    if (i >= TRACKBALL_LINE_MAX) {
        // all lines are processed
        data->continue_reading = false;
        i = 0;
    }

    /** @bug Add exclusive handling of Button case
     * to prevent double click in case of long pressing
     */
}
