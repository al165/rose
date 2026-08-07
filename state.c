#include "string.h"

#include "m_pd.h"

#include "g_canvas.h"

#include "rose_common.h"
#include <m_pd.h>

#define STATE_PAD 3
#define STATE_MINCHR 4
#define STATE_MINLINE 1 /* never shorter than this */

static t_class *state_class;

typedef struct _state_entry
{
	t_symbol *key;
	char *value; /* NUL-terminated, owned by the entry */
} t_state_entry;

typedef struct _state
{
	t_object x_obj;
	t_glist *x_glist;
	t_state_entry *x_entries;
	int x_n;
	int x_alloc;
	int x_selected;
	t_outlet *x_out;
} t_state;

/* Look up an entry by key. Symbols are interned by gensym(), so pointer
 * equality is enough - no need to hash or strcmp. */
static int state_find(t_state *x, t_symbol *key)
{
	int i;
	for (i = 0; i < x->x_n; i++)
		if (x->x_entries[i].key == key)
			return i;
	return -1;
}

/* Insert or update the entry for key, storing value verbatim. */
static void state_set(t_state *x, t_symbol *key, const char *value)
{
	size_t len = strlen(value) + 1;
	int i = state_find(x, key);

	if (i >= 0)
	{
		x->x_entries[i].value = (char *)resizebytes(x->x_entries[i].value, strlen(x->x_entries[i].value) + 1, len);
		memcpy(x->x_entries[i].value, value, len);
		return;
	}

	if (x->x_n == x->x_alloc)
	{
		int newalloc = (x->x_alloc ? x->x_alloc * 2 : 8);
		x->x_entries = (t_state_entry *)resizebytes(x->x_entries, x->x_alloc * sizeof(t_state_entry),
													newalloc * sizeof(t_state_entry));
		x->x_alloc = newalloc;
	}

	x->x_entries[x->x_n].key = key;
	x->x_entries[x->x_n].value = (char *)getbytes(len);
	memcpy(x->x_entries[x->x_n].value, value, len);
	x->x_n++;
}

/* Join "key value" pairs with newlines. Caller must freebytes() the result,
 * passing back the length written to *lenp. */
static char *state_buildtext(t_state *x, int *lenp)
{
	int i, total = 1; /* room for the trailing NUL */
	char *buf, *p;

	for (i = 0; i < x->x_n; i++)
	{
		total += (int)strlen(x->x_entries[i].key->s_name) + 1; /* +1 for '\n' or the separating space */
		if (x->x_entries[i].value[0])
			total += (int)strlen(x->x_entries[i].value) + 1; /* +1 for the space before it */
	}

	buf = (char *)getbytes(total);
	p = buf;
	for (i = 0; i < x->x_n; i++)
	{
		int klen = (int)strlen(x->x_entries[i].key->s_name);
		int vlen = (int)strlen(x->x_entries[i].value);
		if (i)
			*p++ = '\n';
		memcpy(p, x->x_entries[i].key->s_name, klen);
		p += klen;
		if (vlen)
		{
			*p++ = ' ';
			memcpy(p, x->x_entries[i].value, vlen);
			p += vlen;
		}
	}
	*p = '\0';
	*lenp = total;
	return buf;
}

static void state_textsize(t_state *x, t_glist *glist, int *w, int *h)
{
	int fontsize = glist_getfont(glist);
	int zoom = glist_getzoom(glist);
	int i, cols = STATE_MINCHR;
	int rows = (x->x_n > STATE_MINLINE ? x->x_n : STATE_MINLINE);

	for (i = 0; i < x->x_n; i++)
	{
		int len = (int)strlen(x->x_entries[i].key->s_name);
		if (x->x_entries[i].value[0])
			len += 1 + (int)strlen(x->x_entries[i].value);
		if (len > cols)
			cols = len;
	}

	*w = cols * sys_fontwidth(fontsize) * zoom + 2 * STATE_PAD * zoom;
	*h = rows * sys_fontheight(fontsize) * zoom + 2 * STATE_PAD * zoom;
}

static void state_draw_config(t_state *x, t_glist *glist)
{
	t_canvas *canvas = glist_getcanvas(glist);
	int fontsize = glist_getfont(glist);
	int zoom = glist_getzoom(glist);
	int x1 = text_xpix(&x->x_obj, glist);
	int y1 = text_ypix(&x->x_obj, glist);
	int w, h, buflen;
	char tag[255], *buf;
	t_atom fontatoms[3];

	state_textsize(x, glist, &w, &h);

	rose_maketag(x, "BASE", tag, sizeof(tag));
	pdgui_vmess(0, "crs iiii", canvas, "coords", tag, x1, y1, x1 + w, y1 + h);

	unsigned int outline = x->x_selected ? ROSE_BORDER_SELECTED : ROSE_BORDER;
	pdgui_vmess(0, "crs rk rk", canvas, "itemconfigure", tag, "-fill", (unsigned int)ROSE_FILL, "-outline",
				(unsigned int)outline);

	SETSYMBOL(fontatoms + 0, gensym(sys_font));
	SETFLOAT(fontatoms + 1, -fontsize * zoom);
	SETSYMBOL(fontatoms + 2, gensym(sys_fontweight));

	rose_maketag(x, "TEXT", tag, sizeof(tag));
	pdgui_vmess(0, "crs ii", canvas, "coords", tag, x1 + STATE_PAD * zoom, y1 + STATE_PAD * zoom);
	pdgui_vmess(0, "crs rA rk", canvas, "itemconfigure", tag, "-font", 3, fontatoms, "-fill", (unsigned int)outline);

	buf = state_buildtext(x, &buflen);
	pdgui_vmess(0, "crs rs", canvas, "itemconfigure", tag, "-text", buf);
	freebytes(buf, buflen);
}

static void state_draw_new(t_state *x, t_glist *glist)
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

	state_draw_config(x, glist);
}

static void state_update(t_state *x)
{
	if (!glist_isvisible(x->x_glist))
		return;
	state_draw_config(x, x->x_glist);
	canvas_fixlinesfor(x->x_glist, (t_text *)x);
}

static void state_getrect(t_gobj *z, t_glist *glist, int *xp1, int *yp1, int *xp2, int *yp2)
{
	t_state *x = (t_state *)z;
	int w, h;
	state_textsize(x, glist, &w, &h);
	*xp1 = text_xpix(&x->x_obj, glist);
	*yp1 = text_ypix(&x->x_obj, glist);
	*xp2 = *xp1 + w;
	*yp2 = *yp1 + h;
}

static void state_displace(t_gobj *z, t_glist *glist, int dx, int dy)
{
	t_state *x = (t_state *)z;
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

static void state_select(t_gobj *z, t_glist *glist, int state)
{
	t_state *x = (t_state *)z;
	x->x_selected = state;
	if (glist_isvisible(glist))
		state_draw_config(x, glist);
}

static void state_delete(t_gobj *z, t_glist *glist)
{
	canvas_deletelinesfor(glist, (t_text *)z);
}

static void state_vis(t_gobj *z, t_glist *glist, int vis)
{
	t_state *x = (t_state *)z;
	if (vis)
		state_draw_new(x, glist);
	else
	{
		char tag_object[128];
		rose_maketag(x, "", tag_object, sizeof(tag_object));
		pdgui_vmess(0, "crs", glist_getcanvas(glist), "delete", tag_object);
	}
}

/* Any message with a symbol selector is treated as "<key> <rest...>":
 * the selector is the key, the remaining atoms are stringified and stored
 * as its value, replacing any previous value for that key. */
static void state_anything(t_state *x, t_symbol *s, int argc, t_atom *argv)
{
	char buf[MAXPDSTRING], atombuf[MAXPDSTRING];
	int i;

	if (!s || s == &s_list)
	{
		pd_error(x, "state: message needs a key symbol, e.g. \"param1 123\"");
		return;
	}

	buf[0] = '\0';
	for (i = 0; i < argc; i++)
	{
		atom_string(argv + i, atombuf, MAXPDSTRING);
		if (buf[0])
			strncat(buf, " ", MAXPDSTRING - strlen(buf) - 1);
		strncat(buf, atombuf, MAXPDSTRING - strlen(buf) - 1);
	}

	state_set(x, s, buf);
	state_update(x);
	outlet_anything(x->x_out, s, argc, argv);
}

static void state_clear(t_state *x)
{
	int i;
	for (i = 0; i < x->x_n; i++)
		freebytes(x->x_entries[i].value, strlen(x->x_entries[i].value) + 1);
	x->x_n = 0;
	state_update(x);
}

static void state_save(t_gobj *z, t_binbuf *b)
{
	t_state *x = (t_state *)z;
	binbuf_addv(b, "ssii", gensym("#X"), gensym("obj"), (int)x->x_obj.te_xpix, (int)x->x_obj.te_ypix);
	binbuf_addv(b, "s", gensym("state"));
	binbuf_addsemi(b);
}

static void state_free(t_state *x)
{
	int i;
	for (i = 0; i < x->x_n; i++)
		freebytes(x->x_entries[i].value, strlen(x->x_entries[i].value) + 1);
	if (x->x_entries)
		freebytes(x->x_entries, x->x_alloc * sizeof(t_state_entry));
}

void *state_new(t_symbol *s, int argc, t_atom *argv)
{
	t_state *x = (t_state *)pd_new(state_class);
	x->x_glist = (t_glist *)canvas_getcurrent();
	x->x_entries = 0;
	x->x_n = x->x_alloc = 0;
	x->x_selected = 0;
	x->x_out = outlet_new(&x->x_obj, &s_anything);
	(void)s;
	(void)argc;
	(void)argv;

	return x;
}

static const t_widgetbehavior state_widgetbehavior = {state_getrect, state_displace, state_select, NULL,
													  state_delete,	 state_vis,		 NULL};

void state_setup(void)
{
	state_class = class_new(gensym("state"),		///< object name
							(t_newmethod)state_new, ///< constructor
							(t_method)state_free,	///< destructor
							sizeof(t_state),		///< size of data space
							CLASS_DEFAULT,			///< Class flags
							A_GIMME, 0);

	class_addanything(state_class, state_anything);
	class_addmethod(state_class, (t_method)state_clear, gensym("clear"), 0);

	class_setwidget(state_class, &state_widgetbehavior);
	class_setsavefn(state_class, state_save);
}
