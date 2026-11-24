#include <pebble.h>
#include "fonts.h"
#include "settings.h"

#define DEBUG_BBOX (false)
#define DEBUG_TIME (false)
#define BUFFER_LEN (40)
#define ROUND (PBL_IF_ROUND_ELSE(true, false))

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
    color_carpet_blue
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
    color_carpet_blue
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
    color_carpet_blue
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
    color_carpet_blue
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
    color_carpet_blue
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
    color_carpet_red
  );
  // downright dotted blue
  for (int i = 0; i < 2; i++) {
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
      color_carpet_blue
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
      color_carpet_blue
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
      color_carpet_purple
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
      color_carpet_pink
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
      color_carpet_red
    );
  }
}

static void many_pdx(GContext* ctx, GRect bbox) {
  graphics_context_set_fill_color(ctx, color_carpet_teal);
  graphics_fill_rect(ctx, bbox, 0, GCornerNone);
  const int border = 4;
  const int cross_size = border + block_size * blocks_per_cross + border;
  int i = 0;
  for (int y = bbox.origin.y - 2 * cross_size; y < bbox.size.h; y += cross_size) {
    int j = 0;
    for (int x = bbox.origin.x - 2 * cross_size; x < bbox.size.w; x += cross_size) {
      one_pdx(
        ctx,
        grect_inset(
          GRect(
            x + i * cross_size * 3 / 8,
            y + j * cross_size * 3 / 8,
            cross_size,
            cross_size
          ),
          GEdgeInsets(border)
        )
      );
      j++;
    }
    i++;
  }
}

static void draw_buf(GContext* ctx, char* buf, GRect bbox) {
  graphics_context_set_fill_color(ctx, color_text_bg);
  graphics_fill_rect(ctx, bbox, 10, GCornersAll);

  graphics_context_set_text_color(ctx, color_text_fg);
  GFont font = get_font(bbox.size.h - 4);
  draw_text_shifted(ctx, buf, bbox, font, bbox.size.h / 20);
}

static void draw_digits(GContext* ctx, int* vals, int len, GRect bbox, int gap) {
  int char_width = (bbox.size.w - (len + 1) * gap) / len;
  GRect char_bbox;
  char_bbox.size.w = char_width;
  char_bbox.size.h = bbox.size.h;

  for (int i = 0; i < len; i++) {
    char_bbox.origin.x = gap + bbox.origin.x + (char_bbox.size.w + gap) * i;
    char_bbox.origin.y = bbox.origin.y;
    int val = vals[i];
    if (val < 0 || val >= 10) { continue; }
    snprintf(s_buffer, BUFFER_LEN, "%d", val);
    draw_buf(ctx, s_buffer, char_bbox);
  }
}

static int get_hours(struct tm* now) {
#ifdef HOUR_OVERRIDE
  return HOUR_OVERRIDE;
#else
  return now->tm_hour;
#endif
}

static int get_minutes(struct tm* now) {
#ifdef MINUTE_OVERRIDE
  return MINUTE_OVERRIDE;
#else
  return now->tm_min;
#endif
}

static void update_layer(Layer* layer, GContext* ctx) {
  time_t timestamp = time(NULL);
  if (DEBUG_TIME) {
    static time_t extra = 0;
    extra += 59;
    timestamp += extra;
  }
  struct tm* now = localtime(&timestamp);

  GRect full_bounds = layer_get_bounds(layer);
  many_pdx(ctx, full_bounds);

  GRect bounds = layer_get_unobstructed_bounds(layer);
#if ROUND
  int top = 35;
  int right = 35;
  int bot = 35;
  int left = 35;
  bool timeline_quick_view = bounds.size.h < full_bounds.size.h;
  if (timeline_quick_view) {
    bot = 5;
  }
  GRect inset = grect_inset(bounds, GEdgeInsets(top, right, bot, left));
#else
  GRect inset = grect_inset(bounds, GEdgeInsets(5));
#endif

  debug_bbox(ctx, inset);

  GSize sz = GSize(inset.size.w / 2, 80);
  int vertical_space = inset.size.h - sz.h;

  int hours = get_hours(now);
  GRect hour_bbox;
  hour_bbox.size = sz;
  hour_bbox.origin.x = inset.origin.x;
  if (enable_hour_movement) {
    hour_bbox.origin.y = inset.origin.y + vertical_space * hours / 23;
  } else {
    hour_bbox.origin.y = inset.origin.y + vertical_space / 2;
  }

  // do this after the vertical space calculation because we still want noon in the middle
  // and midnight on top & bottom.
  bool include_tens_hour = true;
  if (!clock_is_24h_style()) {
    hours = hours % 12;
    if (hours == 0) {
      hours = 12;
    }
    if (hours < 10) {
      include_tens_hour = false;
    }
  }
  int hour_vals[2] = {
    include_tens_hour ? hours / 10 : -1,
    hours % 10,
  };
  draw_digits(
    ctx,
    hour_vals,
    2,
    hour_bbox,
    5
  );

  int minutes = get_minutes(now);
  GRect minute_bbox;
  minute_bbox.size = sz;
  minute_bbox.origin.x = inset.origin.x + hour_bbox.size.w;
  if (enable_minute_movement) {
    minute_bbox.origin.y = inset.origin.y + vertical_space * minutes / 59;
  } else {
    minute_bbox.origin.y = inset.origin.y + vertical_space / 2;
  }
  int minute_vals[2] = {
    minutes / 10,
    minutes % 10,
  };
  draw_digits(
    ctx,
    minute_vals,
    2,
    minute_bbox,
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

void mark_dirty() {
  if (s_layer) layer_mark_dirty(s_layer);
}

static void tick_handler(struct tm* now, TimeUnits unitchanged) {
  mark_dirty();
}

static void init(void) {
  app_message_set_context(&mark_dirty);
  init_settings();
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
