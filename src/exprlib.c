/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020- sfwbar maintainers
 */

#include <sys/statvfs.h>
#include <glib.h>
#include "sfwbar.h"
#include "module.h"
#include "wintree.h"
#include "expr.h"
#include "basewidget.h"
#include "taskbaritem.h"

/* extract a substring */
static void *expr_lib_mid ( void **params, void *widget, void *event )
{

  gint len, c1, c2;

  if(!params || !params[0] || !params[1] || !params[2])
    return g_strdup("");

  c1 = *((gdouble *)params[1]);
  c2 = *((gdouble *)params[2]);

  len = strlen(params[0]);
  if(c1<0)	/* negative offsets are relative to the end of the string */
    c1+=len;
  if(c2<0)
    c2+=len;
  c1 = CLAMP(c1,0,len-1);
  c2 = CLAMP(c2,0,len-1);
  if(c1>c2)
  {
    c2^=c1;	/* swap the ofsets */
    c1^=c2;
    c2^=c1;
  }

  return g_strndup((gchar *)params[0] + c1*sizeof(gchar),
      (c2-c1+1)*sizeof(gchar));
}

ModuleExpressionHandlerV1 mid_handler = {
  .flags = MODULE_EXPR_DETERMINISTIC,
  .name = "mid",
  .parameters = "SNN",
  .function = expr_lib_mid
};

/* replace a substring within a string */
static void *expr_lib_replace( void **params, void *widget, void *event )
{
  if(!params || !params[0] || !params[1] || !params[2])
    return g_strdup("");

  return str_replace(params[0], params[1], params[2]);
}

ModuleExpressionHandlerV1 replace_handler = {
  .flags = MODULE_EXPR_DETERMINISTIC,
  .name = "replace",
  .parameters = "SSS",
  .function = expr_lib_replace
};

/* Extract substring using regex */
static void *expr_lib_extract( void **params, void *widget, void *event )
{
  gchar *sres=NULL;
  GRegex *regex;
  GMatchInfo *match;

  if(!params || !params[0] || !params[1])
    return g_strdup("");

  regex = g_regex_new(params[1],0,0,NULL);
  if(regex && g_regex_match (regex, params[0], 0, &match) && match)
  {
    sres = g_match_info_fetch (match, 1);
    g_match_info_free (match);
  }
  if(regex)
    g_regex_unref (regex);

  return sres?sres:g_strdup("");
}

ModuleExpressionHandlerV1 extract_handler = {
  .flags = MODULE_EXPR_DETERMINISTIC,
  .name = "extract",
  .parameters = "SS",
  .function = expr_lib_extract
};

static void *expr_lib_pad ( void **params, void *widget, void *event )
{
  gchar *result, *ptr;
  gint n, len, sign;
  gchar padchar;

  if(!params || !params[0] || !params[1])
    return g_strdup("");

  if( params[2] && *((gchar *)params[2]) )
    padchar = *((gchar *)params[2]);
  else
    padchar = ' ';

  len = strlen(params[0]);
  n = (gint)*((gdouble *)params[1]);
  sign = n>=0;
  n = MAX(n>0?n:-n,len);

  result = g_malloc(n+1);
  if(sign)
  {
    memset(result, padchar, n-len);
    strcpy(result+n-len, params[0]);
  }
  else
  {
    ptr = g_stpcpy(result, params[0]);
    memset(ptr, padchar, n-len);
    *(result+n) = '\0';
  }

  return result;
}

ModuleExpressionHandlerV1 pad_handler = {
  .flags = MODULE_EXPR_DETERMINISTIC,
  .name = "pad",
  .parameters = "SNs",
  .function = expr_lib_pad
};

/* Get current time string */
static void *expr_lib_time ( void **params, void *widget, void *event )
{
  GTimeZone *tz;
  GDateTime *time;
  gchar *str;

  if(!params)
    return g_strdup("");

  if(!params[1])
    time = g_date_time_new_now_local();
  else
  {
#if GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION >= 68
    tz = g_time_zone_new_identifier(params[1]);
#else
    tz = g_time_zone_new(params[1]);
#endif
    time = g_date_time_new_now(tz);
    g_time_zone_unref(tz);
  }

  str = g_date_time_format (time, params[0]?params[0]:"%a %b %d %H:%M:%S %Y" );
  g_date_time_unref(time);

  return str;
}

ModuleExpressionHandlerV1 time_handler = {
  .flags = 0,
  .name = "time",
  .parameters = "ss",
  .function = expr_lib_time
};

/* generate disk space utilization for a device */
static void *expr_lib_disk ( void **params, void *widget, void *event )
{
  struct statvfs fs;
  gdouble *result = g_malloc0(sizeof(gdouble));

  if(!params || !params[0] || !params[1])
    return result;

  if(statvfs(params[0],&fs))
    return result;

  if(!g_ascii_strcasecmp(params[1],"total"))
    *result = fs.f_blocks * fs.f_frsize;
  if(!g_ascii_strcasecmp(params[1],"avail"))
    *result = fs.f_bavail * fs.f_bsize;
  if(!g_ascii_strcasecmp(params[1],"free"))
    *result = fs.f_bfree * fs.f_bsize;
  if(!g_ascii_strcasecmp(params[1],"%avail"))
    *result = ((gdouble)(fs.f_bfree*fs.f_bsize) / (gdouble)(fs.f_blocks*fs.f_frsize))*100;
  if(!g_ascii_strcasecmp(params[1],"%used"))
    *result = (1.0 - (gdouble)(fs.f_bfree*fs.f_bsize) / (gdouble)(fs.f_blocks*fs.f_frsize))*100;

  return result;
}

ModuleExpressionHandlerV1 disk_handler = {
  .flags = MODULE_EXPR_NUMERIC,
  .name = "disk",
  .parameters = "SS",
  .function = expr_lib_disk
};

static void *expr_lib_active ( void **params, void *widget, void *event )
{
  return g_strdup(wintree_get_active());
}

ModuleExpressionHandlerV1 active_handler = {
  .flags = 0,
  .name = "ActiveWin",
  .parameters = "",
  .function = expr_lib_active
};

static void *expr_lib_str ( void **params, void *widget, void *event )
{
  if(!params || !params[0])
    return g_strdup("");
  else
    return expr_dtostr(*(gdouble *)params[0],
        params[1]?(gint)*(gdouble *)params[1]:0);
}

ModuleExpressionHandlerV1 str_handler = {
  .flags = MODULE_EXPR_DETERMINISTIC,
  .name = "str",
  .parameters = "Nn",
  .function = expr_lib_str
};

static void *expr_lib_max ( void **params, void *widget, void *event )
{
  gdouble *result;

  if(!params || !params[0] || !params[1])
    return g_malloc0(sizeof(gdouble));

  result = g_malloc(sizeof(gdouble));
  *result = MAX(*(gdouble *)params[0],*(gdouble *)params[1]);
  return result;
}

ModuleExpressionHandlerV1 max_handler = {
  .flags = MODULE_EXPR_DETERMINISTIC | MODULE_EXPR_NUMERIC,
  .name = "max",
  .parameters = "NN",
  .function = expr_lib_max
};

static void *expr_lib_min ( void **params, void *widget, void *event )
{
  gdouble *result;

  if(!params || !params[0] || !params[1])
    return g_malloc0(sizeof(gdouble));

  result = g_malloc(sizeof(gdouble));
  *result = MIN(*(gdouble *)params[0],*(gdouble *)params[1]);
  return result;
}

ModuleExpressionHandlerV1 min_handler = {
  .flags = MODULE_EXPR_DETERMINISTIC | MODULE_EXPR_NUMERIC,
  .name = "min",
  .parameters = "NN",
  .function = expr_lib_min
};

static void *expr_lib_val ( void **params, void *widget, void *event )
{
  gdouble *result;

  if(!params || !params[0])
    return g_malloc0(sizeof(gdouble));

  result = g_malloc(sizeof(gdouble));
  *result = strtod(params[0],NULL);

  return result;
}

ModuleExpressionHandlerV1 val_handler = {
  .flags = MODULE_EXPR_NUMERIC | MODULE_EXPR_DETERMINISTIC,
  .name = "val",
  .parameters = "S",
  .function = expr_lib_val
};

static void *expr_lib_upper ( void **params, void *widget, void *event )
{
  if(!params || !params[0])
    return g_strdup("");
  else
    return g_ascii_strup(params[0],-1);
}

ModuleExpressionHandlerV1 upper_handler = {
  .flags = MODULE_EXPR_DETERMINISTIC,
  .name = "upper",
  .parameters = "S",
  .function = expr_lib_upper
};

static void *expr_lib_lower ( void **params, void *widget, void *event )
{
  if(!params || !params[0])
    return g_strdup("");
  else
    return g_ascii_strdown(params[0],-1);
}

ModuleExpressionHandlerV1 lower_handler = {
  .flags = MODULE_EXPR_DETERMINISTIC,
  .name = "lower",
  .parameters = "S",
  .function = expr_lib_lower
};

static void *expr_lib_gtkevent ( void **params, void *base, void *event )
{
  GtkWidget *widget;
  GdkEventButton  *ev = event;
  GtkAllocation alloc;
  GtkStyleContext *style;
  GtkBorder margin, padding, border;
  gint x,y,w,h,dir;
  gdouble *result;

  if(!params || !params[0])
    return g_malloc0(sizeof(gdouble));

  if(GTK_IS_BIN(base))
  {
    widget = gtk_bin_get_child(base);
    gtk_widget_translate_coordinates(base,widget,ev->x,ev->y,&x,&y);
  }
  else
  {
    widget = base;
    x = ev->x;
    y = ev->y;
  }

  if(!g_ascii_strcasecmp(params[0],"x"))
    dir = GTK_POS_RIGHT;
  else if(!g_ascii_strcasecmp(params[0],"y"))
    dir = GTK_POS_BOTTOM;
  else if(!g_ascii_strcasecmp(params[0],"dir"))
    gtk_widget_style_get(widget,"direction",&dir,NULL);
  else
    return g_malloc0(sizeof(gdouble));

  gtk_widget_get_allocation( widget, &alloc );
  style = gtk_widget_get_style_context(widget);
  gtk_style_context_get_margin(style,gtk_style_context_get_state(style),&margin);
  gtk_style_context_get_padding(style,gtk_style_context_get_state(style),&padding);
  gtk_style_context_get_border(style,gtk_style_context_get_state(style),&border);
  w = alloc.width - margin.left - margin.right - padding.left -
    padding.right - border.left - border.right;
  h = alloc.height - margin.top - margin.bottom - padding.top -
    padding.bottom - border.top - border.bottom;

  x = x - margin.left - padding.left - border.left;
  y = y - margin.top - padding.top - border.top;

  result = g_malloc0(sizeof(gdouble));
  if(dir==GTK_POS_RIGHT || dir==GTK_POS_LEFT)
    *result = CLAMP((gdouble)x / w,0,1);
  else
    *result = CLAMP((gdouble)y / h,0,1);
  if(dir==GTK_POS_LEFT || dir==GTK_POS_TOP)
    *result = 1.0 - *result;

  return result;
}

ModuleExpressionHandlerV1 gtkevent_handler = {
  .flags = MODULE_EXPR_NUMERIC,
  .name = "gtkevent",
  .parameters = "S",
  .function = expr_lib_gtkevent
};

static void *expr_lib_widget_id ( void **params, void *widget, void *event )
{
  gchar *id = base_widget_get_id(widget);

  return g_strdup(id?id:"");
}

ModuleExpressionHandlerV1 widget_id_handler = {
  .name = "widgetid",
  .parameters = "",
  .function = expr_lib_widget_id
};

static void *expr_lib_window_info ( void **params, void *widget, void *event )
{
  window_t *win;
  if(!params || !params[0] || !IS_TASKBAR_ITEM(widget))
    return g_strdup("");

  if( (win = flow_item_get_source(widget)) )
  {
    if(!g_ascii_strcasecmp(params[0], "appid"))
      return g_strdup(win->appid);
    if(!g_ascii_strcasecmp(params[0], "title"))
      return g_strdup(win->title);
    if(!g_ascii_strcasecmp(params[0], "minimized"))
      return g_strdup_printf("%d", !!(win->state & WS_MINIMIZED));
    if(!g_ascii_strcasecmp(params[0], "maximized"))
      return g_strdup_printf("%d", !!(win->state & WS_MAXIMIZED));
    if(!g_ascii_strcasecmp(params[0], "fullscreen"))
      return g_strdup_printf("%d", !!(win->state & WS_FULLSCREEN));
    if(!g_ascii_strcasecmp(params[0], "focused"))
      return g_strdup_printf("%d", wintree_is_focused(win->uid));
  }

  return g_strdup("");
}

ModuleExpressionHandlerV1 window_info_handler = {
  .name = "windowinfo",
  .parameters = "S",
  .function = expr_lib_window_info
};

static void *expr_lib_escape ( void **params, void *widget, void *event )
{
  if(!params || !params[0])
    return g_strdup("");
  return g_markup_escape_text(params[0], -1);
}

ModuleExpressionHandlerV1 escape_handler = {
  .flags = MODULE_EXPR_DETERMINISTIC,
  .name = "escape",
  .parameters = "S",
  .function = expr_lib_escape
};

static void *expr_lib_read ( void **params, void *widget, void *event )
{
  gchar *fname, *result;
  GIOChannel *in;

  if(!params || !params[0])
    return g_strdup("");

  if( !(fname = get_xdg_config_file(params[0], NULL)) )
    return g_strdup_printf("Read: file not found '%s'", (gchar *)params[0]);

  if( (in = g_io_channel_new_file(params[0], "r", NULL)) )
    g_io_channel_read_to_end(in, &result, NULL, NULL);
  else
    result = NULL;


  if(!result)
    result = g_strdup_printf("Read: can't open file '%s'", fname);

  g_free(fname);
  return result;
}

ModuleExpressionHandlerV1 read_handler = {
  .name = "read",
  .parameters = "S",
  .function = expr_lib_read
};

static ModuleExpressionHandlerV1 *expr_lib_handlers[] = {
  &mid_handler,
  &replace_handler,
  &pad_handler,
  &extract_handler,
  &time_handler,
  &disk_handler,
  &active_handler,
  &min_handler,
  &max_handler,
  &str_handler,
  &val_handler,
  &upper_handler,
  &lower_handler,
  &gtkevent_handler,
  &widget_id_handler,
  &window_info_handler,
  &escape_handler,
  &read_handler,
  NULL
};

void expr_lib_init ( void )
{
  module_expr_funcs_add(expr_lib_handlers,"expression library");
}
