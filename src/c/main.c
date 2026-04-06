#include <pebble.h>

// --- Layout constants (emery: 200x228) ---
#define SCREEN_W 200
#define TIME_Y    10
#define TIME_H    65
#define DATE_Y    78
#define DATE_H    22
#define LEVEL_Y   116
#define LEVEL_H   22
#define TEMPS_Y   140
#define TEMPS_H   22
#define RAIN_Y    172
#define RAIN_H    22

// --- Global state ---
static Window    *s_main_window;
static TextLayer *s_time_layer;
static TextLayer *s_date_layer;
static TextLayer *s_level_layer;
static TextLayer *s_temps_layer;
static TextLayer *s_rain_layer;

static char s_time_buffer[8];
static char s_date_buffer[24];
static char s_level_buffer[16];
static char s_temps_buffer[32];
static char s_rain_buffer[32];

// --- Time update ---
static void update_time(void) {
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  strftime(s_time_buffer, sizeof(s_time_buffer),
           clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);
  text_layer_set_text(s_time_layer, s_time_buffer);

  strftime(s_date_buffer, sizeof(s_date_buffer), "%a, %b %d", tick_time);
  text_layer_set_text(s_date_layer, s_date_buffer);
}

// --- AppMessage: send weather request ---
static void request_weather(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
    dict_write_uint8(iter, MESSAGE_KEY_REQUEST_WEATHER, 1);
    app_message_outbox_send();
  }
}

// --- Tick handler ---
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
  if (tick_time->tm_min % 30 == 0) {
    request_weather();
  }
}

// --- AppMessage callbacks ---
static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  Tuple *label_t       = dict_find(iterator, MESSAGE_KEY_COMFORT_LABEL);
  Tuple *level_t       = dict_find(iterator, MESSAGE_KEY_COMFORT_LEVEL);
  Tuple *temps_t       = dict_find(iterator, MESSAGE_KEY_COMFORT_TEMPS);
  Tuple *rain_eta_t    = dict_find(iterator, MESSAGE_KEY_RAIN_ETA);
  Tuple *rain_urgent_t = dict_find(iterator, MESSAGE_KEY_RAIN_URGENT);

  if (label_t) {
    snprintf(s_level_buffer, sizeof(s_level_buffer),
             "%s", label_t->value->cstring);
    text_layer_set_text(s_level_layer, s_level_buffer);
  }

  if (level_t) {
    GColor comfort_color;
    switch (level_t->value->uint8) {
      case 0:  comfort_color = GColorMediumAquamarine; break; // No dramas
      case 1:  comfort_color = GColorChromeYellow;     break; // Sweating / Crikey
      case 2:  comfort_color = GColorOrange;           break; // Cooked
      default: comfort_color = GColorWhite;            break;
    }
    text_layer_set_text_color(s_level_layer, comfort_color);
    layer_mark_dirty(text_layer_get_layer(s_level_layer));
  }

  if (temps_t) {
    snprintf(s_temps_buffer, sizeof(s_temps_buffer),
             "%s", temps_t->value->cstring);
    text_layer_set_text(s_temps_layer, s_temps_buffer);
  }

  if (rain_eta_t) {
    snprintf(s_rain_buffer, sizeof(s_rain_buffer),
             "%s", rain_eta_t->value->cstring);
    text_layer_set_text(s_rain_layer, s_rain_buffer);
  }

  if (rain_urgent_t) {
    GColor rain_color = (rain_urgent_t->value->uint8 == 1)
        ? GColorOrange
        : GColorVividCerulean;
    text_layer_set_text_color(s_rain_layer, rain_color);
    layer_mark_dirty(text_layer_get_layer(s_rain_layer));
  }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Inbox dropped: %d", (int)reason);
}

static void outbox_failed_callback(DictionaryIterator *iterator,
                                   AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox failed: %d", (int)reason);
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Outbox sent OK");
}

// --- Window lifecycle ---
static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);

  // Time — ROBOTO_BOLD_SUBSET_49, white, centered
  s_time_layer = text_layer_create(GRect(0, TIME_Y, SCREEN_W, TIME_H));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_font(s_time_layer,
                      fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));

  // Date — GOTHIC_18, muted gray, centered
  s_date_layer = text_layer_create(GRect(0, DATE_Y, SCREEN_W, DATE_H));
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorLightGray);
  text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));

  // Comfort level name — GOTHIC_18_BOLD, colour set dynamically
  s_level_layer = text_layer_create(GRect(0, LEVEL_Y, SCREEN_W, LEVEL_H));
  text_layer_set_background_color(s_level_layer, GColorClear);
  text_layer_set_text_color(s_level_layer, GColorWhite);
  text_layer_set_font(s_level_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_level_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_level_layer, GTextOverflowModeTrailingEllipsis);
  text_layer_set_text(s_level_layer, "Loading...");
  layer_add_child(window_layer, text_layer_get_layer(s_level_layer));

  // Temps — GOTHIC_18, white, centered
  s_temps_layer = text_layer_create(GRect(0, TEMPS_Y, SCREEN_W, TEMPS_H));
  text_layer_set_background_color(s_temps_layer, GColorClear);
  text_layer_set_text_color(s_temps_layer, GColorWhite);
  text_layer_set_font(s_temps_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_temps_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_temps_layer, GTextOverflowModeTrailingEllipsis);
  layer_add_child(window_layer, text_layer_get_layer(s_temps_layer));

  // Rain ETA — GOTHIC_18_BOLD, colour set dynamically
  s_rain_layer = text_layer_create(GRect(0, RAIN_Y, SCREEN_W, RAIN_H));
  text_layer_set_background_color(s_rain_layer, GColorClear);
  text_layer_set_text_color(s_rain_layer, GColorWhite);
  text_layer_set_font(s_rain_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_rain_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_rain_layer, GTextOverflowModeTrailingEllipsis);
  text_layer_set_text(s_rain_layer, "Loading...");
  layer_add_child(window_layer, text_layer_get_layer(s_rain_layer));

  update_time();
}

static void main_window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_date_layer);
  text_layer_destroy(s_level_layer);
  text_layer_destroy(s_temps_layer);
  text_layer_destroy(s_rain_layer);
  s_time_layer = s_date_layer = s_level_layer = s_temps_layer = s_rain_layer = NULL;
}

// --- App lifecycle ---
static void init(void) {
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorBlack);

  WindowHandlers handlers = {
    .load   = main_window_load,
    .unload = main_window_unload,
  };
  window_set_window_handlers(s_main_window, handlers);
  window_stack_push(s_main_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  app_message_open(256, 64);

  request_weather();
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
  return 0;
}
