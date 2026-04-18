#include <pebble.h>

// --- Layout constants (emery: 200x228) ---
#define SCREEN_W 200
#define SCREEN_H 228

// Solid banner bar at top
#define BANNER_Y  0
#define BANNER_H  30

// Time — big and bold, centred
#define TIME_Y    36
#define TIME_H    55

// Date — just below time
#define DATE_Y    90
#define DATE_H    22

// Temperature — bold
#define TEMPS_Y   120
#define TEMPS_H   28

// Rain ETA — bottom area
#define RAIN_Y    158
#define RAIN_H    28

// --- Global state ---
static Window    *s_main_window;
static Layer     *s_banner_layer;   // custom drawn colour bar
static TextLayer *s_label_layer;    // sweat label text (on the banner)
static TextLayer *s_dp_layer;       // dew point text (on the banner)
static TextLayer *s_time_layer;
static TextLayer *s_date_layer;
static TextLayer *s_temps_layer;
static TextLayer *s_rain_layer;

static char s_time_buffer[8];
static char s_date_buffer[24];
static char s_level_buffer[16];
static char s_dp_buffer[8];
static char s_temps_buffer[32];
static char s_rain_buffer[32];

// Current comfort colour for the banner
static GColor s_banner_color;

// --- Time update ---
static void update_time(void) {
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  strftime(s_time_buffer, sizeof(s_time_buffer),
           clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);
  text_layer_set_text(s_time_layer, s_time_buffer);

  strftime(s_date_buffer, sizeof(s_date_buffer), "%a %d %b", tick_time);
  // Uppercase the date string
  for (int i = 0; s_date_buffer[i]; i++) {
    if (s_date_buffer[i] >= 'a' && s_date_buffer[i] <= 'z') {
      s_date_buffer[i] -= 32;
    }
  }
  text_layer_set_text(s_date_layer, s_date_buffer);
}

// --- Banner draw proc ---
static void banner_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, s_banner_color);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
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
  Tuple *dp_t          = dict_find(iterator, MESSAGE_KEY_DEW_POINT);

  if (label_t) {
    snprintf(s_level_buffer, sizeof(s_level_buffer),
             "%s", label_t->value->cstring);
    text_layer_set_text(s_label_layer, s_level_buffer);
  }

  if (level_t) {
    switch (level_t->value->uint8) {
      case 0:  s_banner_color = GColorMediumAquamarine; break;
      case 1:  s_banner_color = GColorChromeYellow;     break;
      case 2:  s_banner_color = GColorOrange;           break;
      default: s_banner_color = GColorDarkGray;         break;
    }
    layer_mark_dirty(s_banner_layer);
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

  if (dp_t) {
    snprintf(s_dp_buffer, sizeof(s_dp_buffer),
             "%s", dp_t->value->cstring);
    text_layer_set_text(s_dp_layer, s_dp_buffer);
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

  // Default banner colour before data arrives
  s_banner_color = GColorDarkGray;

  // --- Banner: custom drawn solid colour rectangle ---
  s_banner_layer = layer_create(GRect(0, BANNER_Y, SCREEN_W, BANNER_H));
  layer_set_update_proc(s_banner_layer, banner_update_proc);
  layer_add_child(window_layer, s_banner_layer);

  // --- Sweat label: bold black text ON the banner ---
  s_label_layer = text_layer_create(GRect(16, BANNER_Y + 1, SCREEN_W - 74, BANNER_H));
  text_layer_set_background_color(s_label_layer, GColorClear);
  text_layer_set_text_color(s_label_layer, GColorBlack);
  text_layer_set_font(s_label_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_label_layer, GTextAlignmentLeft);
  text_layer_set_overflow_mode(s_label_layer, GTextOverflowModeTrailingEllipsis);
  text_layer_set_text(s_label_layer, "Loading...");
  layer_add_child(window_layer, text_layer_get_layer(s_label_layer));

  // --- Dew point: right side of banner ---
  s_dp_layer = text_layer_create(GRect(SCREEN_W - 62, BANNER_Y + 4, 46, BANNER_H));
  text_layer_set_background_color(s_dp_layer, GColorClear);
  text_layer_set_text_color(s_dp_layer, GColorBlack);
  text_layer_set_font(s_dp_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_dp_layer, GTextAlignmentRight);
  text_layer_set_text(s_dp_layer, "");
  layer_add_child(window_layer, text_layer_get_layer(s_dp_layer));

  // --- Time: biggest and boldest, pure white, centred ---
  s_time_layer = text_layer_create(GRect(0, TIME_Y, SCREEN_W, TIME_H));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_font(s_time_layer,
                      fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));

  // --- Date: bold, light grey, centred ---
  s_date_layer = text_layer_create(GRect(0, DATE_Y, SCREEN_W, DATE_H));
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorLightGray);
  text_layer_set_font(s_date_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));

  // --- Temperature: bold, white, centred ---
  s_temps_layer = text_layer_create(GRect(0, TEMPS_Y, SCREEN_W, TEMPS_H));
  text_layer_set_background_color(s_temps_layer, GColorClear);
  text_layer_set_text_color(s_temps_layer, GColorWhite);
  text_layer_set_font(s_temps_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_temps_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_temps_layer, GTextOverflowModeTrailingEllipsis);
  layer_add_child(window_layer, text_layer_get_layer(s_temps_layer));

  // --- Rain ETA: bold, colour-coded, centred ---
  s_rain_layer = text_layer_create(GRect(0, RAIN_Y, SCREEN_W, RAIN_H));
  text_layer_set_background_color(s_rain_layer, GColorClear);
  text_layer_set_text_color(s_rain_layer, GColorVividCerulean);
  text_layer_set_font(s_rain_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_rain_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_rain_layer, GTextOverflowModeTrailingEllipsis);
  text_layer_set_text(s_rain_layer, "Loading...");
  layer_add_child(window_layer, text_layer_get_layer(s_rain_layer));

  update_time();
}

static void main_window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_date_layer);
  text_layer_destroy(s_label_layer);
  text_layer_destroy(s_dp_layer);
  text_layer_destroy(s_temps_layer);
  text_layer_destroy(s_rain_layer);
  layer_destroy(s_banner_layer);
  s_time_layer = s_date_layer = s_label_layer = s_dp_layer = NULL;
  s_temps_layer = s_rain_layer = NULL;
  s_banner_layer = NULL;
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
