#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>
#include <NTC_Thermistor.h>
#include <AverageThermistor.h>
#include "./NewEncoder.h"
#include "./images.h"

const int ENCODER_DATA_PIN = 3;
const int ENCODER_CLICK_PIN = 2;
const int ENCODER_SWITCH_PIN = 4;

const int SSR_PIN = 5;

const int PULSE_BUTTON_PIN = 13;
int pulse_button = LOW;

const int THERMISTOR_PIN = A3;
const double max_temperature = 40.0;

const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 32;
const int SCREEN_RESET_PIN = -1;
const int SCREEN_ADDRESS = 0x3C;

const int YELLOW_LED_PIN = 7;

struct NumericHudItem {
  unsigned int default_value;
  unsigned int value;
  unsigned int min;
  unsigned int max;
  unsigned int multiple;
  unsigned int eeprom_address;
};

unsigned long time = 0;
unsigned long last_display_update_time = 0;
unsigned long last_control_event = 0;
unsigned long last_temperature_check_time = 0;
unsigned long last_pulse_button_time = 0;

NewEncoder encoder(ENCODER_DATA_PIN, ENCODER_CLICK_PIN, -20, 20, 0, FULL_PULSE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, SCREEN_RESET_PIN);
AverageThermistor thermistor(new NTC_Thermistor(THERMISTOR_PIN, 19500, 10000, 25, 3950), 10, 10);
double temperature = 25.0;

NumericHudItem first_pulse_item = {
  default_value: 10,
  value: 10,
  min: 0,
  max: 999,
  multiple: 10,
  eeprom_address: 0
};

NumericHudItem pause_item = {
  default_value: 10,
  value: 20,
  min: 0,
  max: 999,
  multiple: 10,
  eeprom_address: 16
};

NumericHudItem second_pulse_item = {
  default_value: 10,
  value: 30,
  min: 0,
  max: 999,
  multiple: 10,
  eeprom_address: 32
};

const int ITEMS_COUNT = 3;
int active_hud_item = 0;
bool edit_active_hud_item = false;

int16_t previous_encoder_value;
int previous_switch_value = HIGH;

bool started_first_pulse = false;
bool started_pause = false;
bool start_second_pulse = false;
bool ended_pulse = false;

NumericHudItem items[ITEMS_COUNT] = {
  first_pulse_item,
  pause_item,
  second_pulse_item
};

void setup() {
  delay(500);

  Serial.begin(9600);
  Serial.println("Starting up...");
  
  restore_save();

  pinMode(SSR_PIN, OUTPUT);

  pinMode(PULSE_BUTTON_PIN, INPUT);
  pulse_button = digitalRead(PULSE_BUTTON_PIN);

  if (!encoder.begin()) {
    Serial.println("Encoder Failed to Start. Check pin assignments and available interrupts. Aborting.");
    for(;;);
  }
  
  pinMode(ENCODER_SWITCH_PIN, INPUT);

  delay(200);

  setup_display();

  Serial.println("Startup done!");
}

void setup_display() {
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
      Serial.println(F("SSD1306 allocation failed"));
      for(;;);
  }

  display.clearDisplay();
  display.drawBitmap(0, 0, bitmap_splashscreen, SCREEN_WIDTH, SCREEN_HEIGHT, 1);
  display.display();

  display.setTextColor(1);
  display.setTextSize(1);

  delay(2000);

  encoder.newSettings(0, ITEMS_COUNT - 1, 0);
}

void loop() {
  time = millis();

  if (time - last_temperature_check_time > 1500) {
    last_temperature_check_time = time;
    temperature = thermistor.readCelsius();

    display.clearDisplay();
    draw_hud(time);
    display.display();
  }

  int new_pulse_button = digitalRead(PULSE_BUTTON_PIN);

  if (new_pulse_button != pulse_button) {
    if (new_pulse_button == HIGH && !is_welding(time) && !is_too_hot()) {
      last_pulse_button_time = time;

      display.clearDisplay();
      draw_hud(time);
      display.display();

      encoder.end();

      started_first_pulse = false;
      started_pause = false;
      start_second_pulse = false;
      ended_pulse = false;
    }

    pulse_button = new_pulse_button;
  }

  if (!is_welding(time)) {
    update_controls(time);
    update_display(time);
  } else {
    unsigned long time_since_pulse = time - last_pulse_button_time;

    int pause_time = items[0].value * items[0].multiple;
    int second_pulse_time = pause_time + items[1].value * items[1].multiple;
    int end_time = second_pulse_time + items[2].value * items[2].multiple;

    if (time_since_pulse >= end_time) {
      digitalWrite(SSR_PIN, LOW);
      digitalWrite(YELLOW_LED_PIN, LOW);
      encoder.begin();
    } else if (time_since_pulse >= second_pulse_time) {
      digitalWrite(SSR_PIN, HIGH);
      digitalWrite(YELLOW_LED_PIN, HIGH);
    } else if (time_since_pulse >= pause_time) {
      digitalWrite(SSR_PIN, LOW);
      digitalWrite(YELLOW_LED_PIN, LOW);
    } else {
      digitalWrite(SSR_PIN, HIGH);
      digitalWrite(YELLOW_LED_PIN, HIGH);
    }
  }
}

void update_controls(const unsigned long time) {
  int16_t current_value;
  NewEncoder::EncoderState state;

  if (encoder.getState(state)) {
    current_value = state.currentValue;
    if (current_value != previous_encoder_value) {
      Serial.print("Encoder: ");
      Serial.println(current_value);

      if (edit_active_hud_item) {
        items[active_hud_item].value = current_value;
        Serial.print("Edit item ");
        Serial.print(active_hud_item);
        Serial.print(" with value ");
        Serial.println(current_value);
      } else {
        active_hud_item = current_value;
      }

      previous_encoder_value = current_value;
      last_control_event = time;
    }
  }

  int switch_value = digitalRead(ENCODER_SWITCH_PIN);

  if (previous_switch_value != switch_value) {
    previous_switch_value = switch_value;

    if (switch_value == HIGH) {
      on_encoder_switch_clicked();
      last_control_event = time;
    }
  }
}

void on_encoder_switch_clicked() {
  if (edit_active_hud_item) {
    edit_active_hud_item = false;
    save_items();
    encoder.newSettings(0, ITEMS_COUNT - 1, active_hud_item);
  } else {
    edit_active_hud_item = true;
    encoder.newSettings(items[active_hud_item].min, items[active_hud_item].max, items[active_hud_item].value);
  }
}

void update_display(const unsigned long time) {
  if (!is_too_hot() && !is_welding(time) && time - last_display_update_time > 10) {
    display.clearDisplay();
    draw_hud(time);
    display.display();

    last_display_update_time = millis();
  }
}

unsigned int text_positions[3][2] = {
  {31, 21},
  {71, 21},
  {114, 21}
};

void draw_hud(const unsigned long time) {
  if (is_too_hot()) {
    display.drawBitmap(0, 0, bitmap_too_hot, SCREEN_WIDTH, SCREEN_HEIGHT, 1);
    print_text_from_right(String(round(temperature)), 107, 20);
    return;
  }

  if (is_welding(time)) {
    display.drawBitmap(0, 0, bitmap_welding, SCREEN_WIDTH, SCREEN_HEIGHT, 1);
    return;
  }

  display.drawBitmap(0, 0, bitmap_hud, SCREEN_WIDTH, SCREEN_HEIGHT, 1);
  const int blink = (time / 200) % 2 > 0;

  switch (active_hud_item) {
    case 0:
      if (!edit_active_hud_item || blink) {
        display.drawRect(0, 0, 45, 32, 1);
        display.drawRect(1, 1, 43, 30, 1);
      }
      break;
    
    case 1:
      if (!edit_active_hud_item || blink) {
        display.drawRect(43, 0, 42, 32, 1);
        display.drawRect(44, 1, 40, 30, 1);
      };
      break;
    
    case 2:
      if (!edit_active_hud_item || blink) {
        display.drawRect(83, 0, 45, 32, 1);
        display.drawRect(84, 1, 43, 30, 1);
      }
      break;
  }
  
  for (int i = 0; i < ITEMS_COUNT; i++) {
    print_text_from_right(
      get_readable_item_value(i),
      text_positions[i][0], text_positions[i][1]
    );
  }
}

void print_text_from_right(String text, int x, int y) {
  int16_t  x1, y1;
  uint16_t w, h;

  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  display.setCursor(x - w, y);
  display.print(text);
}

int get_total_pulses_duration() {
  int total = 0;

  for (int i = 0; i < ITEMS_COUNT; i++) {
    total += items[i].value * items[i].multiple;
  }

  return total;
}

bool is_welding(const unsigned long time) {
  return time - last_pulse_button_time < get_total_pulses_duration() + 1000;
}

bool is_too_hot() {
  return temperature >= max_temperature;
}

void restore_save() {
  for (int i = 0; i < 3; i++) {
    EEPROM.get(items[i].eeprom_address, items[i].value);

    if (items[i].value < items[i].min || items[i].value > items[i].max) {
      items[i].value = items[i].default_value;
      Serial.println("Couldn't restore, using default value: " + get_readable_item_value(i) + "ms");
    } else {
      Serial.println("Restored value: " + get_readable_item_value(i) + "ms");
    }
  }
}

String get_readable_item_value(int i) {
  return String(items[i].value * items[i].multiple);
}

void save_items() {
  Serial.println("Saving items: " + get_readable_item_value(0) + "ms, " + get_readable_item_value(1) + "ms, " + get_readable_item_value(2) + "ms.");
  EEPROM.put(items[0].eeprom_address, items[0].value);
  EEPROM.put(items[1].eeprom_address, items[1].value);
  EEPROM.put(items[2].eeprom_address, items[2].value);
}