#include <LilyGoLib.h>
#include <LV_Helper.h>

void setup() {
    Serial.begin(115200);
    instance.begin();
    beginLvglHelper(instance);
    instance.setBrightness(200);
    
    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text_fmt(label, 
        "Battery: %d%%\nTouch: OK\nHaptic: OK", 
        instance.pmu.getBatteryPercent());
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_center(label);
    
    instance.setHapticEffects(14);
    instance.vibrator();
    
    instance.onEvent([](DeviceEvent_t event, void *params, void *user_data) {
        if (event == BUTTON_EVENT) {
            auto btn = instance.getButtonEventType(params);
            if (btn == BUTTON_EVENT_CLICK) {
                Serial.println("Crown clicked!");
                instance.setHapticEffects(1);
                instance.vibrator();
            }
        } else if (event == POWER_EVENT) {
            auto pmu = instance.getPMUEventType(params);
            if (pmu == PMU_EVENT_KEY_CLICKED) {
                Serial.println("PWR clicked!");
                instance.setHapticEffects(14);
                instance.vibrator();
            }
        }
    }, NULL, ALL_EVENT_MAX);
    
    Serial.println("Init OK!");
}

void loop() {
    instance.loop();
    lv_task_handler();
    
    if (instance.getTouched()) {
        int16_t x, y;
        instance.getPoint(&x, &y);
        Serial.printf("Touch: %d, %d\n", x, y);
    }
    
    delay(5);
}