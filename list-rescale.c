#include "m_pd.h"

#define LISTRESCALE_IN_MIN 0.0
#define LISTRESCALE_IN_MAX 127.0
#define LISTRESCALE_OUT_MIN 0.0
#define LISTRESCALE_OUT_MAX 1.0

static t_class *listrescale_class = NULL;

typedef struct _listrescale
{
	t_object x_obj;
	t_float in_min;
	t_float in_max;
	t_float out_min;
	t_float out_max;
	t_outlet *x_out;
} t_listrescale;

void listrescale_list(t_listrescale *x, t_symbol *s, int argc, t_atom *argv)
{
	(void)s;

	t_atom *outv = (t_atom *)getbytes(sizeof(t_atom) * argc);
	for (int i = 0; i < argc; i++)
	{
		float in = atom_getfloat(argv + i);
		float t = (in - x->in_min) / (x->in_max - x->in_min);
		float out = x->out_min + t * (x->out_max - x->out_min);

		SETFLOAT(&outv[i], out);
	}

	outlet_list(x->x_out, &s_list, argc, outv);
	freebytes(outv, sizeof(t_atom) * argc);
}

void *listrescale_new(t_symbol *s, int argc, t_atom *argv)
{
	(void)s;
	t_listrescale *x = (t_listrescale *)pd_new(listrescale_class);

	x->in_min = (argc > 0 ? (double)atom_getfloat(argv) : LISTRESCALE_IN_MIN);
	x->in_max = (argc > 1 ? (double)atom_getfloat(argv + 1) : LISTRESCALE_IN_MAX);
	x->out_min = (argc > 2 ? (double)atom_getfloat(argv + 2) : LISTRESCALE_OUT_MIN);
	x->out_max = (argc > 3 ? (double)atom_getfloat(argv + 3) : LISTRESCALE_OUT_MAX);

	x->x_out = outlet_new(&x->x_obj, &s_list);

	return (void *)x;
}

void setup_list0x2drescale(void)
{
	listrescale_class = class_new(gensym("list-rescale"), (t_newmethod)listrescale_new, NULL, sizeof(t_listrescale),
								  CLASS_DEFAULT, A_GIMME, 0);

	class_addlist(listrescale_class, listrescale_list);
}
