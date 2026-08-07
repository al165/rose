#include "string.h"

#include "m_pd.h"

#include "g_canvas.h"

#include "rose_common.h"
#include <m_pd.h>

#define DISPLAYTRACE_PAD 3
#define DISPLAYTRACE_MINCHR 4
#define DISPLAYTRACE_MINLINE 1		   /* never shorter than this */
#define DISPLAYTRACE_DEFMAX 16		   /* default scrollback, 0 == unlimited */
#define DISPLAYTRACE_DEFTIMEOUT 1000.0 /* default timeout in ms */

#define DISPLAYTRACE_FILL 0xFCFCFC
#define DISPLAYTRACE_BORDER 0x000000
#define DISPLAYTRACE_BORDER_SELECTED 0xFF0000

static t_class *displaytrace_class;

typedef struct _displaytrace
{
	t_object x_obj;
	t_glist *x_glist;
	t_symbol **x_lines;
	int x_n;
	int x_alloc;
	int x_max;
	int x_selected;
	double x_settime;
	double x_timeout;
	t_outlet *x_out;
} t_displaytrace;

static void displaytrace_trim(t_displaytrace *x)
{
	if (x->x_max > 0 && x->x_n > x->x_max)
	{
		int drop = x->x_n - x->x_max;
		memmove(x->x_lines, x->x_lines + drop, (x->x_n - drop) * sizeof(t_symbol *));
		x->x_n -= drop;
	}
}

static void displaytrace_append(t_displaytrace *x, t_symbol *s)
{
	if (clock_gettimesince(x->x_settime) > x->x_timeout)
	{
		// clear
		x->x_n = 0;
		displaytrace_trim(x);
	}

	if (x->x_n == x->x_alloc)
	{
		int newalloc = (x->x_alloc ? x->x_alloc * 2 : 8);
		x->x_lines =
			(t_symbol **)resizebytes(x->x_lines, x->x_alloc * sizeof(t_symbol *), newalloc * sizeof(t_symbol *));
		x->x_alloc = newalloc;
	}
	x->x_lines[x->x_n++] = s;
	x->x_settime = clock_getlogicaltime();
	displaytrace_trim(x);
}

/* Join the symbols with newlines. Caller must freebytes() the result,
 * passing back the length written to *lenp. */
static char *displaytrace_buildtext(t_displaytrace *x, int *lenp)
{
	int i, total = 1; /* room for the trailing NUL */
	char *buf, *p;

	for (i = 0; i < x->x_n; i++)
		total += (int)strlen(x->x_lines[i]->s_name) + 1; /* +1 for '\n' */

	buf = (char *)getbytes(total);
	p = buf;
	for (i = 0; i < x->x_n; i++)
	{
		int len = (int)strlen(x->x_lines[i]->s_name);
		if (i)
			*p++ = '\n';
		memcpy(p, x->x_lines[i]->s_name, len);
		p += len;
	}
	*p = '\0';
	*lenp = total;
	return buf;
}

static void displaytrace_textsize(t_displaytrace *x, t_glist *glist, int *w, int *h)
{
	int fontsize = glist_getfont(glist);
	int zoom = glist_getzoom(glist);
	int i, cols = DISPLAYTRACE_MINCHR;
	int rows = (x->x_n > DISPLAYTRACE_MINLINE ? x->x_n : DISPLAYTRACE_MINLINE);

	for (i = 0; i < x->x_n; i++)
	{
		int len = (int)strlen(x->x_lines[i]->s_name);
		if (len > cols)
			cols = len;
	}

	*w = cols * sys_fontwidth(fontsize) * zoom + 2 * DISPLAYTRACE_PAD * zoom;
	*h = rows * sys_fontheight(fontsize) * zoom + 2 * DISPLAYTRACE_PAD * zoom;
}

static void displaytrace_draw_config(t_displaytrace *x, t_glist *glist)
{
	t_canvas *canvas = glist_getcanvas(glist);
	int fontsize = glist_getfont(glist);
	int zoom = glist_getzoom(glist);
	int x1 = text_xpix(&x->x_obj, glist);
	int y1 = text_ypix(&x->x_obj, glist);
	int w, h, buflen;
	char tag[255], *buf;
	t_atom fontatoms[3];

	displaytrace_textsize(x, glist, &w, &h);

	rose_maketag(x, "BASE", tag, sizeof(tag));
	pdgui_vmess(0, "crs iiii", canvas, "coords", tag, x1, y1, x1 + w, y1 + h);

	unsigned int outline = x->x_selected ? DISPLAYTRACE_BORDER_SELECTED : DISPLAYTRACE_BORDER;
	pdgui_vmess(0, "crs rk rk", canvas, "itemconfigure", tag, "-fill", (unsigned int)DISPLAYTRACE_FILL, "-outline",
				(unsigned int)outline);

	SETSYMBOL(fontatoms + 0, gensym(sys_font));
	SETFLOAT(fontatoms + 1, -fontsize * zoom);
	SETSYMBOL(fontatoms + 2, gensym(sys_fontweight));

	rose_maketag(x, "TEXT", tag, sizeof(tag));
	pdgui_vmess(0, "crs ii", canvas, "coords", tag, x1 + DISPLAYTRACE_PAD * zoom, y1 + DISPLAYTRACE_PAD * zoom);
	pdgui_vmess(0, "crs rA rk", canvas, "itemconfigure", tag, "-font", 3, fontatoms, "-fill", (unsigned int)outline);

	buf = displaytrace_buildtext(x, &buflen);
	pdgui_vmess(0, "crs rs", canvas, "itemconfigure", tag, "-text", buf);
	freebytes(buf, buflen);
}

static void displaytrace_draw_new(t_displaytrace *x, t_glist *glist)
{
	t_canvas *canvas = glist_getcanvas(glist);
	char tag_object[128], tag_item[128];
	char *tags[] = {tag_object, tag_item};

	rose_maketag(x, "", tag_object, sizeof(tag_object));

	rose_maketag(x, "BASE", tag_item, sizeof(tag_item));
	pdgui_vmess(0, "crr iiii rS", canvas, "create", "rectangle", 0, 0, 0, 0, "-tags", 2, tags);

	rose_maketag(x, "TEXT", tag_item, sizeof(tag_item));
	pdgui_vmess(0, "crr ii rs rs rS", canvas, "create", "text", 0, 0, "-anchor", "nw", "-justify", "left", "-tags", 2,
				tags);

	displaytrace_draw_config(x, glist);
}

static void displaytrace_update(t_displaytrace *x)
{
	if (!glist_isvisible(x->x_glist))
		return;
	displaytrace_draw_config(x, x->x_glist);
	canvas_fixlinesfor(x->x_glist, (t_text *)x);
}

static void displaytrace_getrect(t_gobj *z, t_glist *glist, int *xp1, int *yp1, int *xp2, int *yp2)
{
	t_displaytrace *x = (t_displaytrace *)z;
	int w, h;
	displaytrace_textsize(x, glist, &w, &h);
	*xp1 = text_xpix(&x->x_obj, glist);
	*yp1 = text_ypix(&x->x_obj, glist);
	*xp2 = *xp1 + w;
	*yp2 = *yp1 + h;
}

static void displaytrace_displace(t_gobj *z, t_glist *glist, int dx, int dy)
{
	t_displaytrace *x = (t_displaytrace *)z;
	char tag_object[128];
	x->x_obj.te_xpix += dx;
	x->x_obj.te_ypix += dy;
	if (glist_isvisible(glist))
	{
		rose_maketag(x, "", tag_object, sizeof(tag_object));
		pdgui_vmess(0, "crs ii", glist_getcanvas(glist), "move", tag_object, dx, dy);
		canvas_fixlinesfor(glist, (t_text *)x);
	}
}

static void displaytrace_select(t_gobj *z, t_glist *glist, int state)
{
	t_displaytrace *x = (t_displaytrace *)z;
	x->x_selected = state;
	if (glist_isvisible(glist))
		displaytrace_draw_config(x, glist);
}

static void displaytrace_delete(t_gobj *z, t_glist *glist)
{
	canvas_deletelinesfor(glist, (t_text *)z);
}

static void displaytrace_vis(t_gobj *z, t_glist *glist, int vis)
{
	t_displaytrace *x = (t_displaytrace *)z;
	if (vis)
		displaytrace_draw_new(x, glist);
	else
	{
		char tag_object[128];
		rose_maketag(x, "", tag_object, sizeof(tag_object));
		pdgui_vmess(0, "crs", glist_getcanvas(glist), "delete", tag_object);
	}
}

static void displaytrace_symbol(t_displaytrace *x, t_symbol *s)
{
	displaytrace_append(x, (s ? s : gensym("")));
	displaytrace_update(x);
	outlet_symbol(x->x_out, x->x_lines[x->x_n - 1]);
}

static void displaytrace_float(t_displaytrace *x, t_floatarg f)
{
	char buf[MAXPDSTRING];
	t_atom a;
	SETFLOAT(&a, f);
	atom_string(&a, buf, MAXPDSTRING);
	displaytrace_symbol(x, gensym(buf));
}

/* accept any message and render it as text */
static void displaytrace_anything(t_displaytrace *x, t_symbol *s, int argc, t_atom *argv)
{
	char buf[MAXPDSTRING], atombuf[MAXPDSTRING];
	int i;

	buf[0] = '\0';
	if (s && s != &s_list)
		strncat(buf, s->s_name, MAXPDSTRING - strlen(buf) - 1);
	for (i = 0; i < argc; i++)
	{
		atom_string(argv + i, atombuf, MAXPDSTRING);
		if (buf[0])
			strncat(buf, " ", MAXPDSTRING - strlen(buf) - 1);
		strncat(buf, atombuf, MAXPDSTRING - strlen(buf) - 1);
	}
	displaytrace_symbol(x, gensym(buf));
}

static void displaytrace_bang(t_displaytrace *x)
{
	outlet_symbol(x->x_out, x->x_lines[x->x_n - 1]);
}

static void displaytrace_clear(t_displaytrace *x)
{
	x->x_n = 0;
	displaytrace_update(x);
}

static void displaytrace_max(t_displaytrace *x, t_floatarg f)
{
	x->x_max = (f < 1 ? 1 : (int)f);
	displaytrace_trim(x);
	displaytrace_update(x);
}

static void displaytrace_timeout(t_displaytrace *x, t_floatarg f)
{
	x->x_timeout = (f < 0.0 ? 0.0 : (double)f);
}

static void displaytrace_save(t_gobj *z, t_binbuf *b)
{
	t_displaytrace *x = (t_displaytrace *)z;
	binbuf_addv(b, "ssiisif", gensym("#X"), gensym("obj"), (int)x->x_obj.te_xpix, (int)x->x_obj.te_ypix,
				gensym("display-trace"), x->x_max, x->x_timeout);
	binbuf_addsemi(b);
}

static void displaytrace_free(t_displaytrace *x)
{
	if (x->x_lines)
		freebytes(x->x_lines, x->x_alloc * sizeof(t_symbol *));
}

void *displaytrace_new(t_symbol *s, int argc, t_atom *argv)
{
	t_displaytrace *x = (t_displaytrace *)pd_new(displaytrace_class);
	x->x_glist = (t_glist *)canvas_getcurrent();
	x->x_lines = 0;
	x->x_n = x->x_alloc = 0;
	x->x_max = (argc > 0 ? (int)atom_getfloat(argv) : DISPLAYTRACE_DEFMAX);
	if (x->x_max < 1)
		x->x_max = 1;
	x->x_timeout = (argc > 1 ? (double)atom_getfloat(argv) : DISPLAYTRACE_DEFTIMEOUT);
	x->x_selected = 0;
	x->x_settime = 0.0;
	x->x_out = outlet_new(&x->x_obj, &s_symbol);

	return x;
}

static const t_widgetbehavior displaytrace_widgetbehavior = {displaytrace_getrect,
															 displaytrace_displace,
															 displaytrace_select,
															 NULL,
															 displaytrace_delete,
															 displaytrace_vis,
															 NULL};

void setup_display0x2dtrace(void)
{
	displaytrace_class = class_new(gensym("display-trace"),		  ///< object name
								   (t_newmethod)displaytrace_new, ///< constructor
								   (t_method)displaytrace_free,	  ///< destructor
								   sizeof(t_displaytrace),		  ///< size of data space
								   CLASS_DEFAULT,				  ///< Class flags
								   A_GIMME, 0);

	class_addbang(displaytrace_class, displaytrace_bang);
	class_addsymbol(displaytrace_class, displaytrace_symbol);
	class_addfloat(displaytrace_class, displaytrace_float);
	class_addanything(displaytrace_class, displaytrace_anything);
	class_addmethod(displaytrace_class, (t_method)displaytrace_clear, gensym("clear"), 0);
	class_addmethod(displaytrace_class, (t_method)displaytrace_max, gensym("max"), A_FLOAT, 0);
	class_addmethod(displaytrace_class, (t_method)displaytrace_timeout, gensym("timeout"), A_FLOAT, 0);

	class_setwidget(displaytrace_class, &displaytrace_widgetbehavior);
	class_setsavefn(displaytrace_class, displaytrace_save);
}
