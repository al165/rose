#include <errno.h>
#include <stdlib.h>

#include "m_pd.h"

static t_class *getenv_class = NULL;

typedef struct _getenv
{
	t_object x_obj;
	t_symbol *var;
	t_atom fallback;
	t_outlet *x_out;
} t_getenv;

void getenv_bang(t_getenv *x)
{

	// Check if symbol is empty
	if (x->var == &s_)
		return;

	const char *env_var = getenv(x->var->s_name);

	if (env_var == NULL)
	{
		switch (x->fallback.a_type)
		{
		case A_NULL:
			return;
		case A_FLOAT:
			outlet_float(x->x_out, atom_getfloat(&x->fallback));
			return;
		case A_SYMBOL:
			outlet_symbol(x->x_out, atom_getsymbol(&x->fallback));
			return;
		default:
			break;
		}
	}

	errno = 0;

	char *end;
	double value = strtod(env_var, &end);
	if (env_var == end || *end != '\0' || errno == ERANGE)
	{
		// Cannot be converted to integer, output as symbol
		outlet_symbol(x->x_out, gensym(env_var));
	}
	else
		outlet_float(x->x_out, (t_float)value);
}

void getenv_anything(t_getenv *x, t_symbol *s, int argc, t_atom *argv)
{
	if (s == &s_symbol || s == &s_list)
	{
		if (argc < 1 || argv[0].a_type != A_SYMBOL)
		{
			pd_error(x, "getenv: expected a variable name");
			return;
		}
		s = argv[0].a_w.w_symbol;
		argc--;
		argv++;
	}
	else if (s == &s_float)
	{
		pd_error(x, "getenv: expected a variable name, got a float");
		return;
	}

	x->var = s;
	if (argc > 0)
		x->fallback = argv[0];
	else
		x->fallback.a_type = A_NULL;

	getenv_bang(x);
}

void *getenv_new(t_symbol *s, int argc, t_atom *argv)
{
	(void)s;
	t_getenv *x = (t_getenv *)pd_new(getenv_class);

	x->var = atom_getsymbolarg(0, argc, argv);

	if (argc > 1)
		x->fallback = argv[1];
	else
		x->fallback.a_type = A_NULL;

	x->x_out = outlet_new(&x->x_obj, &s_anything);

	return (void *)x;
}

void getenv_setup(void)
{
	getenv_class =
		class_new(gensym("getenv"), (t_newmethod)getenv_new, NULL, sizeof(t_getenv), CLASS_DEFAULT, A_GIMME, 0);

	class_addanything(getenv_class, (t_method)getenv_anything);
	class_addbang(getenv_class, (t_method)getenv_bang);
}
