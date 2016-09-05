#include <pebble.h>

# define KEY_ITEM_NO 1
# define KEY_ITEM_HEAD 2
# define KEY_ITEM_PAGE 3
# define KEY_ITEM_TEXT 4
# define KEY_REQUEST_TYPE 5

# define REQUEST_TYPE_HEADLINE 1
# define REQUEST_TYPE_STORY 2
# define REQUEST_TYPE_SECTION 3
# define ALL_HEADLINES_RECEIVED -1

# define MAX_NO_HEADLINES 10
# define TOP_HEAD_CHARS 17
# define BOTTOM_HEAD_CHARS 23

 
static Window* window;
static Window* article_window;
static Window* wait_window;
static MenuLayer *menu_layer;
static ScrollLayer *s_scroll_layer;
static TextLayer *s_text_layer;
static TextLayer *wait_text_layer;
static char headline[MAX_NO_HEADLINES][TOP_HEAD_CHARS+BOTTOM_HEAD_CHARS];
static char headline_top[MAX_NO_HEADLINES][TOP_HEAD_CHARS+5];
static char headline_bottom[MAX_NO_HEADLINES][BOTTOM_HEAD_CHARS+5];
static bool headline_received[MAX_NO_HEADLINES];
static char wait_text[] = "Wait...";
static char s_scroll_text[1000]; 
static int currentPage, currentStory;
static bool flickMode = false;
static bool nightMode = false;
static bool section_menu_mode = true;

// Prototype
static void article_window_set_text();

void setHeadlines() {
  int i;
  for( i=0 ; i < MAX_NO_HEADLINES ; i++ ) {
    snprintf(headline[i], sizeof(headline[i]), "%03d waiting", i+1);
    headline_received[i] = false;
  }
}

/*
static void send_int(int key, int value) {
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_int(iter, key, &value, sizeof(int), true);
  app_message_outbox_send();
}
*/


static void request_headline(int item_no) {
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_int(iter, KEY_ITEM_NO, &item_no, sizeof(int), true);
  int requestType = REQUEST_TYPE_HEADLINE;
  dict_write_int(iter, KEY_REQUEST_TYPE, &requestType, sizeof(int), true);
  app_message_outbox_send();
}

static void request_story(int item_no, int page) {
  window_stack_push(wait_window, true);
//  window_stack_remove(window, false);
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_int(iter, KEY_ITEM_NO, &item_no, sizeof(int), true);
  dict_write_int(iter, KEY_ITEM_PAGE, &page, sizeof(int), true);
  int requestType = REQUEST_TYPE_STORY;
  dict_write_int(iter, KEY_REQUEST_TYPE, &requestType, sizeof(int), true);
  app_message_outbox_send();
}

static void request_section(int section_id) {
  snprintf(wait_text, sizeof(wait_text), "%s", headline[section_id]);
  window_stack_push(wait_window, true);
  window_stack_remove(window, false);
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_int(iter, KEY_ITEM_NO, &section_id, sizeof(int), true);
  int requestType = REQUEST_TYPE_SECTION;
  dict_write_int(iter, KEY_REQUEST_TYPE, &requestType, sizeof(int), true);
  section_menu_mode = false;
  app_message_outbox_send();
  
  menu_layer_set_selected_index(menu_layer, (MenuIndex){.section=0, .row=0}, MenuRowAlignTop, false);
}

static int request_next_headline() {
  int i;
  int return_value = ALL_HEADLINES_RECEIVED;
  for( i=0 ; i < MAX_NO_HEADLINES ; i++ ) {
    if( !headline_received[i] ) {
      request_headline(i);
      return_value = i;
      i = MAX_NO_HEADLINES * 2;
    }
  }
  return return_value;
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  // Read tuples for data
  Tuple *item_no_tuple = dict_find(iterator, KEY_ITEM_NO);
  Tuple *item_head_tuple = dict_find(iterator, KEY_ITEM_HEAD);
  Tuple *story_tuple = dict_find(iterator, KEY_ITEM_TEXT);

  // If all data is available, use it
  if(item_no_tuple) {
    int item_no = (int)item_no_tuple->value->int32;
    if( item_no >= 0 && item_no < MAX_NO_HEADLINES && item_head_tuple) {
      snprintf(headline[item_no], sizeof(headline[item_no]), "%s", item_head_tuple->value->cstring);
      headline_received[item_no] = true;
      snprintf(wait_text, sizeof(wait_text), "Wait %d", item_no);
      text_layer_set_text(wait_text_layer, wait_text);

      int i;
      for( i=0 ; i < (TOP_HEAD_CHARS + BOTTOM_HEAD_CHARS) ; i++ ) {
        if( i < TOP_HEAD_CHARS ) {
          headline_top[item_no][i] = headline[item_no][i];
        }else{
          headline_bottom[item_no][i - TOP_HEAD_CHARS] = headline[item_no][i];
        }
      }
    }
    if (item_no == (MAX_NO_HEADLINES - 1)) {
//      layer_mark_dirty(window_get_root_layer(window));
      window_stack_push(window, true);
      window_stack_remove(wait_window, false);
      snprintf(wait_text, sizeof(wait_text), "Wait...");
    }
  }
  
  if( story_tuple ) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Received a story");
    snprintf(s_scroll_text, sizeof(s_scroll_text), "%s", story_tuple->value->cstring);
    APP_LOG(APP_LOG_LEVEL_INFO, "Put the story in a buffer");
   // article_window_set_text();
   // layer_mark_dirty(scroll_layer_get_layer(s_scroll_layer));
    window_stack_push(article_window, true);
    window_stack_remove(wait_window, false);

  }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
  //request_next_headline();
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
  //request_next_headline();
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}


static void article_window_set_text() {
  text_layer_set_text(s_text_layer, s_scroll_text);
  // Trim text layer and scroll content to fit text box
  GSize max_size = text_layer_get_content_size(s_text_layer);
 // text_layer_set_size(s_text_layer, GSize(max_size.w, max_size.h + 4));
  GRect bounds = layer_get_frame(window_get_root_layer(article_window));
  text_layer_set_size(s_text_layer, GSize(bounds.size.w, max_size.h + 4));
  scroll_layer_set_content_size(s_scroll_layer, GSize(bounds.size.w, max_size.h + 4));
}

static void wait_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_frame(window_layer);
  wait_text_layer = text_layer_create(GRect(0, 52, bounds.size.w, 50));
  text_layer_set_font(wait_text_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(wait_text_layer, GTextAlignmentCenter);
  text_layer_set_text(wait_text_layer, wait_text);
  window_set_background_color(wait_window, nightMode ? GColorBlack : GColorWhite);
  text_layer_set_background_color(wait_text_layer, GColorClear);
  text_layer_set_text_color(wait_text_layer, nightMode ? GColorWhite : GColorBlack);
  layer_add_child(window_layer, text_layer_get_layer(wait_text_layer));
}


static void tap_handler(AccelAxisType axis, int32_t direct) {
// vibes_double_pulse();
  // if( ! (data->did_vibrate) ) {
    GPoint text_pos = scroll_layer_get_content_offset(s_scroll_layer);
    scroll_layer_set_content_offset(s_scroll_layer, GPoint(0, text_pos.y - 168), true);
  // }
}

static void set_backlight_mode() {
  if( flickMode & nightMode ) {
    light_enable(true);
  } else {
    light_enable(false);
    light_enable_interaction();
  }
}

static void set_day_night_mode() {
  text_layer_set_background_color(s_text_layer, nightMode ? GColorBlack : GColorClear);
  window_set_background_color(article_window, nightMode ? GColorBlack : GColorWhite);
  text_layer_set_text_color(s_text_layer, nightMode ? GColorWhite : GColorBlack );
  set_backlight_mode();
}

static void toggle_night_mode() {
  nightMode = !nightMode;
  set_day_night_mode();
}

static void set_flick_mode() {
  if( flickMode ) {
    accel_tap_service_subscribe((AccelTapHandler) tap_handler);
  } else {
    accel_tap_service_unsubscribe();
  }
  set_backlight_mode();
}

static void toggle_flick_mode() {
  flickMode = !flickMode;
  set_flick_mode();
}

void select_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  // vibes_double_pulse();
  vibes_short_pulse();
  toggle_flick_mode();
}

void select_single_click_handler(ClickRecognizerRef recognizer, void *context) {
  snprintf(wait_text, sizeof(wait_text), "Page %d", 2 + currentPage);
  window_stack_push(wait_window, true);
  window_stack_remove(article_window, false);
  currentPage++;
  request_story(currentStory, currentPage);
}

static void select_double_click_handler(ClickRecognizerRef recognizer, void *context) {
  toggle_night_mode();
}

void config_provider(Window *window) {
  // long click config:
  window_long_click_subscribe(BUTTON_ID_SELECT, 700, select_long_click_handler, NULL);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_single_click_handler);
  window_multi_click_subscribe(BUTTON_ID_SELECT, 2, 0, 0, true, select_double_click_handler);
}

// Setup the scroll layer on window load
// We do this here in order to be able to get the max used text size
static void article_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_frame(window_layer);
  GRect max_text_bounds = GRect(0, -5, bounds.size.w, 3000);

  // Initialize the scroll layer
  s_scroll_layer = scroll_layer_create(bounds);

  // This binds the scroll layer to the window so that up and down map to scrolling
  // You may use scroll_layer_set_callbacks to add or override interactivity
  scroll_layer_set_click_config_onto_window(s_scroll_layer, window);

  // Initialize the text layer
  s_text_layer = text_layer_create(max_text_bounds);
  text_layer_set_text(s_text_layer, s_scroll_text);
  
  // Change the font to a nice readable one
  // This is system font; you can inspect pebble_fonts.h for all system fonts
  // or you can take a look at feature_custom_font to add your own font
  text_layer_set_font(s_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));

  article_window_set_text();
  scroll_layer_set_paging(s_scroll_layer, true);

  ScrollLayerCallbacks scb = { .click_config_provider = (ClickConfigProvider) config_provider,};
  scroll_layer_set_callbacks(s_scroll_layer, scb);

  // Add the layers for display
  scroll_layer_add_child(s_scroll_layer, text_layer_get_layer(s_text_layer));

  layer_add_child(window_layer, scroll_layer_get_layer(s_scroll_layer));
  set_flick_mode();
  set_day_night_mode();
}


// Draw the items in the menu
void draw_row_callback(GContext *ctx, Layer *cell_layer, MenuIndex *cell_index, void *callback_context)
{ 
//  int top_chars = 5;
//  int bottom_chars = 5;
  int headline_no = cell_index->row;
/*  int i;
  char top_str[top_chars];
  char bottom_str[bottom_chars];
  char this_headline[top_chars+bottom_chars];
  snprintf(this_headline, sizeof(this_headline), "%s", headline[headline_no]);
  
  for( i=0 ; i < (top_chars + bottom_chars) ; i++ ) {
    if( i < top_chars ) {
      top_str[i] = this_headline[i];
    } else {
      bottom_str[i - top_chars] = this_headline[i];
    }
  }
  */
  menu_cell_basic_draw(ctx, cell_layer, headline_top[headline_no], headline_bottom[headline_no], NULL);
}

uint16_t num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *callback_context)
{
	return MAX_NO_HEADLINES;
}



void select_click_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context)
{
	//Get which row
	currentStory = cell_index->row;
  currentPage = 0;
  if( section_menu_mode ) {
    request_section(cell_index->row);
  } else {
	   request_story(currentStory, currentPage);
     snprintf(wait_text, sizeof(wait_text), "Page %d", currentPage + 1);
  };
}
 
void window_load(Window *window)
{
	//Create it - 12 is approx height of the top bar
	menu_layer = menu_layer_create(GRect(0, 0, 144, 168));
	
	//Let it receive clicks
	menu_layer_set_click_config_onto_window(menu_layer, window);
	
	//Give it its callbacks
	MenuLayerCallbacks callbacks = {
		.draw_row = (MenuLayerDrawRowCallback) draw_row_callback,
		.get_num_rows = (MenuLayerGetNumberOfRowsInSectionsCallback) num_rows_callback,
		.select_click = (MenuLayerSelectCallback) select_click_callback
	};
	menu_layer_set_callbacks(menu_layer, NULL, callbacks);
	
	//Add to Window
	layer_add_child(window_get_root_layer(window), menu_layer_get_layer(menu_layer));
}
 
void article_window_unload(Window *window)
{
  text_layer_destroy(s_text_layer);
  scroll_layer_destroy(s_scroll_layer);
  if( flickMode ) {
    accel_tap_service_unsubscribe();
  }
  light_enable(false);
  light_enable_interaction();
}
void window_unload(Window *window)
{
	menu_layer_destroy(menu_layer);
}
void wait_window_unload(Window *window) {
  text_layer_destroy(wait_text_layer);
}
 




void init()
{
  setHeadlines();
	window = window_create();
	WindowHandlers handlers = {
		.load = window_load,
		.unload = window_unload
	};
	window_set_window_handlers(window, (WindowHandlers) handlers);
  
  article_window = window_create();
  WindowHandlers article_handlers = {
    .load = article_window_load,
    .unload = article_window_unload
  };
  window_set_window_handlers(article_window, (WindowHandlers) article_handlers);
  //window_set_click_config_provider(article_window, (ClickConfigProvider) config_provider);
  
  wait_window = window_create();
  WindowHandlers wait_handlers = {
    .load = wait_window_load,
    .unload = wait_window_unload
  };
  window_set_window_handlers(wait_window, (WindowHandlers) wait_handlers);
  window_stack_push(wait_window, true);  
  
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  // Open AppMessage
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
}
 
void deinit()
{
	window_destroy(window);
	window_destroy(article_window);
  window_destroy(wait_window);
}
 
int main(void)
{
	init();
	app_event_loop();
	deinit();
}