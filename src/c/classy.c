#include <pebble.h>
#include "message_keys.auto.h"

#define SETTINGS_KEY 1

typedef struct {
  bool SecondTick;
  bool InvertColors;
} ClaySettings;

static ClaySettings s_settings;
static Window *s_window;
static Layer *s_second_layer, *s_center_layer, *s_minute_layer, *s_hour_layer;
static BitmapLayer *s_bg_layer;
static GBitmap *s_bg_bitmap;
static TextLayer *s_weekday_layer, *s_date_layer;
static GFont s_font;
static GPath *s_minute_path, *s_hour_path;

static const GPathInfo MINUTE_HAND_PATH_POINTS = {
  4, (GPoint[]){{0,15},{5,0},{0,-58},{-5,0}}
};
static const GPathInfo HOUR_HAND_PATH_POINTS = {
  4, (GPoint[]){{0,15},{6,0},{0,-48},{-6,0}}
};

// Persistence
static void prv_save_settings() {
  persist_write_data(SETTINGS_KEY, &s_settings, sizeof(s_settings));
}

static void prv_load_settings() {
  if (persist_exists(SETTINGS_KEY)) {
    persist_read_data(SETTINGS_KEY, &s_settings, sizeof(s_settings));
  } else {
    s_settings.SecondTick = true;
    s_settings.InvertColors = false;
  }
}

// Update date and weekday
static void update_weekday() {
  static char buf[4];
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  const char *days[] = {"dim","lun","mar","mer","jeu","ven","sam"};
  snprintf(buf, sizeof(buf), "%s", days[t->tm_wday]);
  text_layer_set_text(s_weekday_layer, buf);
}

static void update_date() {
  static char buf[3];
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  strftime(buf, sizeof(buf), "%d", t);
  text_layer_set_text(s_date_layer, buf);
}

// Colors based on inversion
static GColor fg_color() {
  return s_settings.InvertColors ? GColorWhite : GColorBlack;
}
static GColor bg_color() {
  return s_settings.InvertColors ? GColorBlack : GColorWhite;
}

// Second hand
static void second_layer_update(Layer *layer, GContext *ctx) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  int32_t angle = t->tm_sec * TRIG_MAX_ANGLE / 60;
  GRect b = layer_get_bounds(layer);
  GPoint c = grect_center_point(&b);
  GPoint end = GPoint(
    c.x + 70 * sin_lookup(angle)/TRIG_MAX_RATIO,
    c.y - 70 * cos_lookup(angle)/TRIG_MAX_RATIO
  );
  graphics_context_set_stroke_color(ctx, fg_color());
  graphics_draw_line(ctx, c, end);
}

// Center dot
static void center_layer_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  GPoint c = grect_center_point(&b);
  graphics_context_set_fill_color(ctx, bg_color());
  graphics_fill_circle(ctx, c, 2);
  graphics_context_set_fill_color(ctx, fg_color());
  graphics_fill_circle(ctx, c, 1);
}

// Minute & hour hands
static void minute_layer_update(Layer *layer, GContext *ctx) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  int32_t angle = TRIG_MAX_ANGLE * t->tm_min / 60;
  gpath_rotate_to(s_minute_path, angle);
  graphics_context_set_fill_color(ctx, fg_color());
  gpath_draw_filled(ctx, s_minute_path);
}
static void hour_layer_update(Layer *layer, GContext *ctx) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  int32_t angle = TRIG_MAX_ANGLE * (t->tm_hour * 60 + t->tm_min) / 720;
  gpath_rotate_to(s_hour_path, angle);
  graphics_context_set_fill_color(ctx, fg_color());
  gpath_draw_filled(ctx, s_hour_path);
}

// Tick handler
static void tick_handler(struct tm *t, TimeUnits u) {
  if (s_settings.SecondTick) layer_mark_dirty(s_second_layer);
  layer_mark_dirty(s_minute_layer);
  if (t->tm_sec == 0) {
    layer_mark_dirty(s_hour_layer);
    update_date();
    update_weekday();
  }
}

// AppMessage inbox received handler
static void prv_inbox_received_handler(DictionaryIterator *iter, void *ctx) {
  Tuple *st = dict_find(iter, MESSAGE_KEY_SecondTick);
  if (st) s_settings.SecondTick = st->value->int32;
  Tuple *inv = dict_find(iter, MESSAGE_KEY_InvertColors);
  if (inv) s_settings.InvertColors = inv->value->int32;
  tick_timer_service_subscribe(
    s_settings.SecondTick ? SECOND_UNIT : MINUTE_UNIT,
    tick_handler
  );
  prv_save_settings();
}

// Window load
static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);

  // Background image based on inversion
  s_bg_bitmap = gbitmap_create_with_resource(
    s_settings.InvertColors
      ? RESOURCE_ID_IMAGE_BACKGROUND_INVERTED
      : RESOURCE_ID_IMAGE_BACKGROUND
  );
  s_bg_layer = bitmap_layer_create(b);
  bitmap_layer_set_bitmap(s_bg_layer, s_bg_bitmap);
  layer_add_child(root, bitmap_layer_get_layer(s_bg_layer));

  // Font
  s_font = fonts_load_custom_font(
    resource_get_handle(RESOURCE_ID_FONT_ROBOTO_THIN_11)
  );

  // Weekday layer
  s_weekday_layer = text_layer_create(GRect(1, 42, 144, 14));
  text_layer_set_font(s_weekday_layer, s_font);
  text_layer_set_text_alignment(s_weekday_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_weekday_layer, GColorClear);
  text_layer_set_text_color(s_weekday_layer, fg_color());
  layer_add_child(root, text_layer_get_layer(s_weekday_layer));
  update_weekday();

  // Date layer
  s_date_layer = text_layer_create(GRect(64, 109, 16, 14));
  text_layer_set_font(s_date_layer, s_font);
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, fg_color());
  layer_add_child(root, text_layer_get_layer(s_date_layer));
  update_date();

  // Create hand paths
  s_minute_path = gpath_create(&MINUTE_HAND_PATH_POINTS);
  s_hour_path   = gpath_create(&HOUR_HAND_PATH_POINTS);
  GPoint center = grect_center_point(&b);
  gpath_move_to(s_minute_path, center);
  gpath_move_to(s_hour_path, center);

  // Minute hand layer
  s_minute_layer = layer_create(b);
  layer_set_update_proc(s_minute_layer, minute_layer_update);
  layer_add_child(root, s_minute_layer);
  // Hour hand layer
  s_hour_layer = layer_create(b);
  layer_set_update_proc(s_hour_layer, hour_layer_update);
  layer_add_child(root, s_hour_layer);

  // Second hand layer if enabled
  if (s_settings.SecondTick) {
    s_second_layer = layer_create(b);
    layer_set_update_proc(s_second_layer, second_layer_update);
    layer_add_child(root, s_second_layer);
  }

  // Center dot layer
  s_center_layer = layer_create(b);
  layer_set_update_proc(s_center_layer, center_layer_update);
  layer_add_child(root, s_center_layer);
}

// Window unload
static void window_unload(Window *window) {
  gbitmap_destroy(s_bg_bitmap);
  bitmap_layer_destroy(s_bg_layer);
  fonts_unload_custom_font(s_font);
  text_layer_destroy(s_weekday_layer);
  text_layer_destroy(s_date_layer);
  if (s_settings.SecondTick) layer_destroy(s_second_layer);
  layer_destroy(s_minute_layer);
  layer_destroy(s_hour_layer);
  layer_destroy(s_center_layer);
  gpath_destroy(s_minute_path);
  gpath_destroy(s_hour_path);
}

// Initialize
static void init(void) {
  prv_load_settings();
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload
  });
  window_stack_push(s_window, true);
  app_message_register_inbox_received(prv_inbox_received_handler);
  app_message_open(app_message_inbox_size_maximum(),
                   app_message_outbox_size_maximum());
  tick_timer_service_subscribe(
    s_settings.SecondTick ? SECOND_UNIT : MINUTE_UNIT,
    tick_handler
  );
}

// Deinitialize
static void deinit(void) {
  tick_timer_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
  return 0;
}