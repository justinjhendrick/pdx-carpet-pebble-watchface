#include <pebble.h>
#include "fonts.h"

#define DEBUG_TIME (false)
#define FORCE_12H (false)
#define BUFFER_LEN (40)

#define DATE_HEIGHT (40)
#define GAP (5)

static Window* s_window;
static Layer* s_layer;
static char s_buffer[BUFFER_LEN];

static void draw_text_shifted(GContext* ctx, const char* buffer, GRect bbox, GFont font, int shift_up) {
  GRect fixed_bbox = GRect(bbox.origin.x, bbox.origin.y - shift_up, bbox.size.w, bbox.size.h);
  graphics_draw_text(ctx, buffer, font, fixed_bbox, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

static void fill_rect(GContext* ctx, GPoint origin, int x, int y, int w, int h) {
  graphics_fill_rect(
    ctx,
    GRect(
      origin.x + w * x,
      origin.y + h * y,
      w,
      h
    ),
    0,
    GCornerNone
  );
}

static void fill_dither(GContext* ctx, GRect bbox, GColor* cols, int num_cols) {
  int w = 5;
  int h = 5;
  for (int y = 0; y * h < bbox.size.h; y++) {
    int col_idx = y % num_cols;
    for (int x = 0; x * w < bbox.size.w;) {
      graphics_context_set_fill_color(ctx, cols[col_idx]);

      if (y % 3 == 0) {
        fill_rect(ctx, bbox.origin, x++, y, w, h);
      }
      if (y % 2 == 0) {
        fill_rect(ctx, bbox.origin, x++, y, w, h);
      }
      fill_rect(ctx, bbox.origin, x++, y, w, h);

      col_idx = (col_idx + 1) % num_cols;
    }
  }
}

static void draw_vertical(GContext* ctx, char* buf, GRect bbox) {
  GRect inset = grect_inset(bbox, GEdgeInsets(GAP));
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, inset, 10, GCornersAll);

  graphics_context_set_text_color(ctx, GColorBlack);
  int len = strlen(buf);
  if (len == 0) { return; }
  GFont font = get_font(inset.size.h / len);
  char t[80];
  for (int i = 0; i < len; i++) {
    t[2 * i] = buf[i];
    if (i == len - 1) {
      t[2 * i + 1] = '\0';
    } else {
      t[2 * i + 1] = '\n';
    }
  }
  draw_text_shifted(ctx, t, inset, font, 0);
}

static void draw_buf(GContext* ctx, char* buf, GRect bbox) {
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bbox, 10, GCornersAll);

  graphics_context_set_text_color(ctx, GColorBlack);
  GFont font = get_font(bbox.size.h);
  draw_text_shifted(ctx, buf, bbox, font, 4);
}

static void stack_digits(GContext* ctx, int c1, int c2, GRect bbox, int gap) {
  // G c G c G
  int char_height = (bbox.size.h - 3 * gap) / 2;
  GRect upper;
  upper.origin.y = bbox.origin.y + gap;
  upper.origin.x = bbox.origin.x;
  upper.size.h = char_height;
  upper.size.w = bbox.size.w;
  snprintf(s_buffer, BUFFER_LEN, "%d", c1);
  draw_buf(ctx, s_buffer, upper);

  GRect lower;
  lower.origin.y = upper.origin.y + char_height + gap;
  lower.origin.x = upper.origin.x;
  lower.size.h = char_height;
  lower.size.w = bbox.size.w;
  snprintf(s_buffer, BUFFER_LEN, "%d", c2);
  draw_buf(ctx, s_buffer, lower);
}

static void draw_digits(GContext* ctx, int c1, int c2, GRect bbox, int gap) {
  // G c G c G
  int char_width = (bbox.size.w - 3 * gap) / 2;
  GRect left;
  left.origin.x = bbox.origin.x + gap;
  left.origin.y = bbox.origin.y;
  left.size.w = char_width;
  left.size.h = bbox.size.h;
  snprintf(s_buffer, BUFFER_LEN, "%d", c1);
  draw_buf(ctx, s_buffer, left);

  GRect right;
  right.origin.x = left.origin.x + char_width + gap;
  right.origin.y = left.origin.y;
  right.size.w = char_width;
  right.size.h = bbox.size.h;
  snprintf(s_buffer, BUFFER_LEN, "%d", c2);
  draw_buf(ctx, s_buffer, right);
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
  if (DEBUG_TIME) {
    now->tm_mday = (now->tm_sec % 31) + 1;
    now->tm_mon = (now->tm_sec % 12);
    now->tm_wday = (now->tm_sec % 7);
  }

  GRect bounds = layer_get_bounds(layer);
  //static GColor cols[2] = {GColorCyan, GColorMediumSpringGreen};
  static GColor cols[2] = {GColorCyan, GColorOxfordBlue};
  fill_dither(ctx, bounds, cols, 2);

  // Date
  GRect date_bbox = GRect(
    bounds.origin.x,
    bounds.origin.y,
    bounds.size.w * 4 / 20,
    bounds.size.h
  );

  strftime(s_buffer, BUFFER_LEN, "%a %d %b", now);
  draw_vertical(ctx, s_buffer, date_bbox);

  // hours
  GRect hour_bbox;
  hour_bbox.origin.x = date_bbox.origin.x + date_bbox.size.w;
  hour_bbox.origin.y = bounds.origin.y;
  hour_bbox.size.w = bounds.size.w * 6 / 20;
  hour_bbox.size.h = bounds.size.h;
  int hours = get_hours(now);
  graphics_context_set_text_color(ctx, GColorBlack);
  stack_digits(
    ctx,
    hours / 10,
    hours % 10,
    hour_bbox,
    GAP
  );

  // minutes
  GRect minute_bbox;
  int minutes = get_minutes(now);
  minute_bbox.size.w = bounds.size.w * 10 / 20;
  minute_bbox.size.h = hour_bbox.size.h * 3 / 10;
  minute_bbox.origin.x = hour_bbox.origin.x + hour_bbox.size.w;
  minute_bbox.origin.y = hour_bbox.origin.y + minutes * (hour_bbox.size.h - minute_bbox.size.h) / 60;
  graphics_context_set_text_color(ctx, GColorBlack);
  draw_digits(
    ctx,
    minutes / 10,
    minutes % 10,
    minute_bbox,
    GAP
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
