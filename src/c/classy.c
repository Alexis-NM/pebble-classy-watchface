#include <pebble.h>

#define DISPLAY_SECONDS        true
#define DISPLAY_TIME           false

static Window      *s_window;
static Layer       *s_second_layer;
static Layer       *s_center_layer;
static Layer       *s_minute_layer;
static Layer       *s_hour_layer;
static BitmapLayer *s_bg_layer;
static GBitmap     *s_bg_bitmap;
static TextLayer   *s_weekday_layer;
static TextLayer   *s_date_layer;
#if DISPLAY_TIME
static TextLayer   *s_time_layer;
#endif
static GFont        s_font;

// Chemins pour aiguilles
static GPath *s_minute_path;
static GPath *s_hour_path;

// GPathInfo definitions (écran rectangulaire)
static const GPathInfo MINUTE_HAND_PATH_POINTS = {
  4, (GPoint[]) { {0, 15}, {5, 0}, {0, -58}, {-5, 0} }
};
static const GPathInfo HOUR_HAND_PATH_POINTS = {
  4, (GPoint[]) { {0, 15}, {6, 0}, {0, -48}, {-6, 0} }
};

// Met à jour le jour (FR 3 lettres)
static void update_weekday() {
  static char buf[4];
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  const char *days[7] = {"dim","lun","mar","mer","jeu","ven","sam"};
  snprintf(buf, sizeof(buf), "%s", days[t->tm_wday]);
  text_layer_set_text(s_weekday_layer, buf);
}

// Met à jour la date
static void update_date() {
  static char buf[3];
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  strftime(buf, sizeof(buf), "%d", t);
  text_layer_set_text(s_date_layer, buf);
}

#if DISPLAY_TIME
static void update_time_text() {
  static char buf[6];
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  const char *fmt = clock_is_24h_style() ? "%R" : "%I:%M";
  strftime(buf, sizeof(buf), fmt, t);
  text_layer_set_text(s_time_layer, buf);
}
#endif

// Trotteuse (ligne fine en noir)
static void second_layer_update(Layer *layer, GContext *ctx) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  int32_t angle = t->tm_sec * TRIG_MAX_ANGLE / 60;

  GRect bounds = layer_get_bounds(layer);
  GPoint c = grect_center_point(&bounds);
  GPoint end = GPoint(
    c.x + 70 * sin_lookup(angle) / TRIG_MAX_RATIO,
    c.y - 70 * cos_lookup(angle) / TRIG_MAX_RATIO
  );

  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_line(ctx, c, end);
}

// Centre du cadran : anneau blanc épais autour d'un point noir
static void center_layer_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GPoint c = grect_center_point(&bounds);

  // 1) Cercle plein blanc (rayon = 2px)
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, c, 2);

  // 2) Cercle plein noir (rayon = 1px)
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, c, 1);
}

// Aiguille des minutes (solide noir, sans contour)
static void minute_layer_update(Layer *layer, GContext *ctx) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  int32_t angle = TRIG_MAX_ANGLE * t->tm_min / 60;
  gpath_rotate_to(s_minute_path, angle);

  graphics_context_set_fill_color(ctx, GColorBlack);
  gpath_draw_filled(ctx, s_minute_path);
}

// Aiguille des heures (solide noir, sans contour)
static void hour_layer_update(Layer *layer, GContext *ctx) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  int32_t angle = TRIG_MAX_ANGLE * (t->tm_hour * 60 + t->tm_min) / 720;
  gpath_rotate_to(s_hour_path, angle);

  graphics_context_set_fill_color(ctx, GColorBlack);
  gpath_draw_filled(ctx, s_hour_path);
}

// Gestion des ticks
static void tick_handler(struct tm *t, TimeUnits units) {
#if DISPLAY_SECONDS
  layer_mark_dirty(s_second_layer);
#endif
  layer_mark_dirty(s_minute_layer);
  if (t->tm_sec == 0) {
    layer_mark_dirty(s_hour_layer);
    update_date();
    update_weekday();
#if DISPLAY_TIME
    update_time_text();
#endif
  }
}

// Chargement de la fenêtre
static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  // Fond PNG
  s_bg_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND);
  s_bg_layer  = bitmap_layer_create(bounds);
  bitmap_layer_set_bitmap(s_bg_layer, s_bg_bitmap);
  layer_add_child(root, bitmap_layer_get_layer(s_bg_layer));

  // Police
  s_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_THIN_11));

  // Calques jour et date
  s_weekday_layer = text_layer_create(GRect(1, 42, 144, 14));
  text_layer_set_font(s_weekday_layer, s_font);
  text_layer_set_text_alignment(s_weekday_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_weekday_layer, GColorClear);
  text_layer_set_text_color(s_weekday_layer, GColorBlack);
  layer_add_child(root, text_layer_get_layer(s_weekday_layer));
  update_weekday();

  s_date_layer = text_layer_create(GRect(64, 109, 16, 14));
  text_layer_set_font(s_date_layer, s_font);
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorBlack);
  layer_add_child(root, text_layer_get_layer(s_date_layer));
  update_date();

#if DISPLAY_TIME
  s_time_layer = text_layer_create(GRect(58, 42, 29, 14));
  text_layer_set_font(s_time_layer, s_font);
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  layer_add_child(root, text_layer_get_layer(s_time_layer));
  update_time_text();
#endif

  // Création et position des mains
  s_minute_path = gpath_create(&MINUTE_HAND_PATH_POINTS);
  s_hour_path   = gpath_create(&HOUR_HAND_PATH_POINTS);
  GPoint center = grect_center_point(&bounds);
  gpath_move_to(s_minute_path, center);
  gpath_move_to(s_hour_path,   center);

  // Ajout des calques des aiguilles
  s_minute_layer = layer_create(bounds);
  layer_set_update_proc(s_minute_layer, minute_layer_update);
  layer_add_child(root, s_minute_layer);

  s_hour_layer = layer_create(bounds);
  layer_set_update_proc(s_hour_layer, hour_layer_update);
  layer_add_child(root, s_hour_layer);

#if DISPLAY_SECONDS
  // Trotteuse AU-DESSUS des aiguilles
  s_second_layer = layer_create(bounds);
  layer_set_update_proc(s_second_layer, second_layer_update);
  layer_add_child(root, s_second_layer);
#endif

  // Centre du cadran
  s_center_layer = layer_create(bounds);
  layer_set_update_proc(s_center_layer, center_layer_update);
  layer_add_child(root, s_center_layer);
}

// Déchargement de la fenêtre
static void window_unload(Window *window) {
  gbitmap_destroy(s_bg_bitmap);
  bitmap_layer_destroy(s_bg_layer);
  fonts_unload_custom_font(s_font);
  text_layer_destroy(s_weekday_layer);
  text_layer_destroy(s_date_layer);
#if DISPLAY_TIME
  text_layer_destroy(s_time_layer);
#endif
  layer_destroy(s_minute_layer);
  layer_destroy(s_hour_layer);
  layer_destroy(s_second_layer);
  layer_destroy(s_center_layer);
  gpath_destroy(s_minute_path);
  gpath_destroy(s_hour_path);
}

// Initialisation
static void init(void) {
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){ .load = window_load, .unload = window_unload });
  window_stack_push(s_window, true);
  tick_timer_service_subscribe(DISPLAY_SECONDS ? SECOND_UNIT : MINUTE_UNIT, tick_handler);
}

// Désinitialisation
static void deinit(void) {
  tick_timer_service_unsubscribe();
  window_destroy(s_window);
}

// Main
int main(void) {
  init();
  app_event_loop();
  deinit();
  return 0;
}