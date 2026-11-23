#include <pebble.h>
#include "fonts.h"

#define DEBUG_BBOX (true)
#define DEBUG_TIME (false)
#define FORCE_12H (false)
#define BUFFER_LEN (40)

#define DATE_HEIGHT (40)

static Window* s_window;
static Layer* s_layer;
static char s_buffer[BUFFER_LEN];


static void debug_bbox(GContext* ctx, GRect bbox) {
  if (!DEBUG_BBOX) { return; }
  graphics_context_set_stroke_width(ctx, 1);
  graphics_context_set_stroke_color(ctx, GColorRed);
  graphics_draw_rect(ctx, bbox);
}

static void draw_text_shifted(GContext* ctx, const char* buffer, GRect bbox, GFont font, int shift_up) {
  GRect fixed_bbox = GRect(bbox.origin.x, bbox.origin.y - shift_up, bbox.size.w, bbox.size.h);
  graphics_draw_text(ctx, buffer, font, fixed_bbox, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

static void fill_rect(GContext* ctx, GPoint origin, GRect bbox, GColor col) {
  graphics_context_set_fill_color(ctx, col);
  bbox.origin.x += origin.x;
  bbox.origin.y += origin.y;
  graphics_fill_rect(
    ctx,
    bbox,
    0,
    GCornerNone
  );
}

const int block_size = 6;
const int blocks_per_cross = 11; // needs to be odd

static void one_pdx(GContext* ctx, GRect bbox) {
  const int b = block_size;
  const int g = 1;
  const int s = b - 2 * g;
  // far left dot
  fill_rect(
    ctx,
    bbox.origin,
    GRect(
      0,
      5 * b + g,
      s,
      s
    ),
    GColorOxfordBlue
  );
  // left bar
  fill_rect(
    ctx,
    bbox.origin,
    GRect(
      1 * b,
      5 * b + g,
      3 * b + s + g,
      s
    ),
    GColorOxfordBlue
  );
  // right bar
  fill_rect(
    ctx,
    bbox.origin,
    GRect(
      6 * b + g,
      5 * b + g,
      2 * b + s,
      s
    ),
    GColorOxfordBlue
  );
  // down bar
  fill_rect(
    ctx,
    bbox.origin,
    GRect(
      5 * b + g,
      6 * b + g,
      s,
      4 * b + s
    ),
    GColorOxfordBlue
  );
  // center
  fill_rect(
    ctx,
    bbox.origin,
    GRect(
      5 * b + g,
      5 * b + g,
      s,
      s
    ),
    GColorOxfordBlue
  );
  // red above center
  fill_rect(
    ctx,
    bbox.origin,
    GRect(
      5 * b + g,
      4 * b + g,
      s,
      s
    ),
    GColorRed
  );
  // downright dotted blue
  for (int i = 0; i < 3; i++) {
    int x = (6 + i) * b;
    fill_rect(
      ctx,
      bbox.origin,
      GRect(
        x + g,
        x + g,
        s,
        s
      ),
      GColorOxfordBlue
    );
  }
  // downleft dotted blue
  for (int i = 0; i < 3; i++) {
    int x = i * b;
    int y = 10 * b - x;
    fill_rect(
      ctx,
      bbox.origin,
      GRect(
        x + g,
        y + g,
        s,
        s
      ),
      GColorOxfordBlue
    );
  }
  // right purple dotted
  for (int x = 9 * b; x < 11 * b; x += b) {
    fill_rect(
      ctx,
      bbox.origin,
      GRect(
        x + g,
        5 * b + g,
        s,
        s
      ),
      GColorVividViolet
    );
  }
  // up pink dotted
  for (int y = 0; y < 5 * b; y += b) {
    fill_rect(
      ctx,
      bbox.origin,
      GRect(
        4 * b + g,
        y + g,
        s,
        s
      ),
      GColorBrilliantRose
    );
  }
  // down red dotted
  for (int y = 9 * b; y < 11 * b; y += b) {
    fill_rect(
      ctx,
      bbox.origin,
      GRect(
        6 * b + g,
        y + g,
        s,
        s
      ),
      GColorRed
    );
  }
}

static void many_pdx(GContext* ctx, GRect bbox) {
  graphics_context_set_fill_color(ctx, GColorTiffanyBlue);
  graphics_fill_rect(ctx, bbox, 0, GCornerNone);
  const int border = 4;
  const int cross_size = border + block_size * blocks_per_cross + border;
  int init_x = bbox.origin.x - cross_size;
  for (int y = bbox.origin.y; y < bbox.size.h; y += cross_size) {
    for (int x = init_x; x < bbox.size.w; x += cross_size) {
      one_pdx(
        ctx,
        grect_inset(
          GRect(x, y, cross_size, cross_size),
          GEdgeInsets(border)
        )
      );
    }
    init_x = -cross_size + (init_x + (cross_size * 3 / 8)) % cross_size;
  }
}

static void draw_buf(GContext* ctx, char* buf, GRect bbox) {
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bbox, 10, GCornersAll);

  graphics_context_set_text_color(ctx, GColorBlack);
  GFont font = get_font(bbox.size.h);
  draw_text_shifted(ctx, buf, bbox, font, 5);
}

static void draw_digits(GContext* ctx, int* vals, int len, GRect bbox, int gap) {
  int char_width = (bbox.size.w - (len + 1) * gap) / len;
  GRect char_bbox;
  char_bbox.size.w = char_width;
  char_bbox.size.h = bbox.size.h;

  for (int i = 0; i < len; i++) {
    char_bbox.origin.x = gap + bbox.origin.x + (char_bbox.size.w + gap) * i;
    char_bbox.origin.y = bbox.origin.y;
    snprintf(s_buffer, BUFFER_LEN, "%d", vals[i]);
    draw_buf(ctx, s_buffer, char_bbox);
  }
}

static int get_hours(struct tm* now) {
#ifdef HOUR_OVERRIDE
  return HOUR_OVERRIDE;
#else
  int hours = now->tm_hour;
  if (DEBUG_TIME) {
    hours = now->tm_sec % 24;
  }
  return hours;
#endif
}

static int get_minutes(struct tm* now) {
#ifdef MINUTE_OVERRIDE
  return MINUTE_OVERRIDE;
#else
  int minutes = now->tm_min;
  if (DEBUG_TIME) {
    minutes = now->tm_sec;
  }
  return minutes;
#endif
}

static void update_layer(Layer* layer, GContext* ctx) {
  time_t temp = time(NULL);
  struct tm* now = localtime(&temp);

  GRect bounds = layer_get_bounds(layer);
  many_pdx(ctx, bounds);

  int hours = get_hours(now);
  int minutes = get_minutes(now);
  GRect time_bbox;
  time_bbox.size.w = bounds.size.w * 18 / 20;
  time_bbox.size.h = 80;
  time_bbox.origin.x = bounds.origin.x + (bounds.size.w - time_bbox.size.w) / 2;
  time_bbox.origin.y = bounds.origin.y + (bounds.size.h - time_bbox.size.h) / 2;
  int nums[4] = {
    hours / 10,
    hours % 10,
    minutes / 10,
    minutes % 10,
  };
  draw_digits(
    ctx,
    nums,
    4,
    time_bbox,
    5
  );
}

static void window_load(Window* window) {
  Layer* window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  s_layer = layer_create(bounds);
  layer_set_update_proc(s_layer, update_layer);
  layer_add_child(window_layer, s_layer);
}

static void window_unload(Window* window) {
  if (s_layer) {
    layer_destroy(s_layer);
    s_layer = NULL;
  }
}

static void tick_handler(struct tm* now, TimeUnits unitchanged) {
  if (s_layer) layer_mark_dirty(s_layer);
}

static void init(void) {
  init_fonts();
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
  tick_timer_service_subscribe(DEBUG_TIME ? SECOND_UNIT : MINUTE_UNIT, tick_handler);
}

static void deinit(void) {
  deinit_fonts();
  tick_timer_service_unsubscribe();
  if (s_window) {
    window_destroy(s_window);
    s_window = NULL;
  }
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
