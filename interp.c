/* $Id: interp.c,v 1.43 2012/03/06 06:09:29 mit-sato Exp $ */

#include <assert.h>
#include <string.h>
#include <signal.h>
#include "types.h"
#include "cell.h"
#include "interp.h"
#include "config.h"
#include "bulk.h"
#include "parser.h"
#include "toy.h"
#include "global.h"
#include "cstack.h"

Toy_Type* cmd_pwd(Toy_Interp *interp, Toy_Type *posargs, Hash *nameargs, int arglen);
static Toy_Type* parse_env(char* buff, char sep);

Toy_Interp*
new_interp(char* name, int stack_size, Toy_Interp* parent,
	   int argc, char **argv, char **envp) {

    Toy_Interp *interp;
    static int gc_init_once = 0;

    if (0 == gc_init_once) {
	GC_INIT();
	init_cstack();
	gc_init_once = 1;
    }

    interp = GC_MALLOC(sizeof(Toy_Interp));
    ALLOC_SAFE(interp);

    if (NULL == parent) {

	def_global();
	
	interp->obj_stack = (Toy_Obj_Env**)(GC_MALLOC(sizeof(Toy_Obj_Env*) * stack_size));
	ALLOC_SAFE(interp->obj_stack);
	memset(interp->obj_stack, 0, (sizeof(Toy_Obj_Env*) * stack_size));

	interp->func_stack = (Toy_Func_Env**)(GC_MALLOC(sizeof(Toy_Func_Env*) * stack_size));
	ALLOC_SAFE(interp->func_stack);
	memset(interp->func_stack, 0, (sizeof(Toy_Func_Env*) * stack_size));

	interp->funcs = new_hash();
	if (NULL == interp->funcs) return NULL;

	interp->classes = new_hash();
	if (NULL == interp->classes) return NULL;

	interp->globals = new_hash();
	if (NULL == interp->globals) return NULL;

	interp->scripts = new_hash();
	if (NULL == interp->scripts) return NULL;

	interp->name = name;
	interp->stack_size = stack_size;
	interp->cur_obj_stack = -1;
	interp->cur_func_stack = -1;

	interp->trace_info = GC_MALLOC(sizeof(struct _toy_func_trace_info));
	ALLOC_SAFE(interp->trace_info);
	interp->trace_info->line = 0;
	interp->trace_info->object_name = new_symbol("Toplevel");
	interp->trace_info->func_name = new_symbol("Main");
	interp->trace_info->statement = new_statement(new_list(new_symbol("(toysh)")), 0);
	
	interp->trace = 0;
	interp->trace_fd = 2;
#ifdef HAS_GCACHE
	interp->gcache = new_hash();
	interp->cache_hit = 0;
	interp->cache_missing = 0;
#endif /* HAS_GCACHE */
	hash_set_t(interp->scripts, const_atscriptid,
		   new_integer_si(interp->script_id));

	interp->cstack = 0;
	interp->cstack_size = 0;
	interp->coroid = 0;
	interp->co_parent = 0;
	interp->co_value = 0;
	interp->co_calling = 0;

	sig_flag = 0;

	if (argv && envp) {
	    interp_setup(interp, argc, argv, envp);
	}

    } else {

	interp->name = parent->name;
	interp->stack_size = stack_size;

	interp->cur_func_stack = 0;
	interp->func_stack = (Toy_Func_Env**)(GC_MALLOC(sizeof(Toy_Func_Env*) * stack_size));
	ALLOC_SAFE(interp->func_stack);
	memset(interp->func_stack, 0, sizeof(Toy_Func_Env*) * stack_size);
	interp->func_stack[interp->cur_func_stack] = parent->func_stack[parent->cur_func_stack];

	interp->cur_obj_stack = 0;
	interp->obj_stack = (Toy_Obj_Env**)(GC_MALLOC(sizeof(Toy_Obj_Env*) * stack_size));
	ALLOC_SAFE(interp->obj_stack);
	memset(interp->obj_stack, 0, sizeof(Toy_Func_Env*) * stack_size);

	interp->obj_stack[interp->cur_obj_stack] = parent->obj_stack[parent->cur_obj_stack];
	interp->funcs = parent->funcs;
	interp->classes = parent->classes;
	interp->globals = parent->globals;
	interp->scripts = parent->scripts;
	interp->trace_info = parent->trace_info;
	interp->script_id = parent->script_id;
#ifdef HAS_GCACHE
	interp->gcache = parent->gcache;
	interp->cache_hit = parent->cache_hit;
	interp->cache_missing = parent->cache_missing;
#endif /* HAS_GCACHE */
	interp->trace = parent->trace;
	interp->cstack = 0;
	interp->cstack_size = 0;
	interp->coroid = 0;
	interp->co_parent = 0;
	interp->co_value = 0;
	interp->co_calling = 0;
    }

    return interp;
}

Toy_Interp*
interp_setup(Toy_Interp* interp, int argc, char **argv, char **envp) {
    Toy_Type *delegate;
    Hash *gdict;
    int i;
    Toy_Type *argl, *l;
    Toy_Type *any;
    Toy_Type *env, *envl, *envv;
    Toy_Type *setupl;

    delegate = new_list(const_Object);

    toy_push_new_obj_env(interp, "Toplevel", new_object("Main", new_hash(), delegate));
    toy_push_func_env(interp, new_hash(), NULL, NULL);

    /* make world */

    toy_add_class(interp, "Functions", interp->funcs, delegate);
    toy_add_class(interp, "Nil", NULL, delegate);
    toy_add_class(interp, "Integer", NULL, delegate);
    toy_add_class(interp, "Real", NULL, delegate);
    toy_add_class(interp, "String", NULL, delegate);
    toy_add_class(interp, "List", NULL, delegate);
    toy_add_class(interp, "Block", NULL, delegate);
    toy_add_class(interp, "RQuote", NULL, delegate);
    toy_add_class(interp, "Object", NULL, NULL);
    toy_add_class(interp, "Hash", NULL, delegate);
    toy_add_class(interp, "File", NULL, delegate);
    toy_add_class(interp, "Array", NULL, delegate);
    toy_add_class(interp, "Dict", NULL, delegate);
    toy_add_class(interp, "Vector", NULL, delegate);
    toy_add_class(interp, "Coro", NULL, delegate);

    toy_add_commands(interp);
    toy_add_methods(interp);

    /* set VERSION */
    hash_set_t(interp->globals, const_VERSION, new_string_str(VERSION));

    /* set ARGV */
    l = argl = new_list(NULL);
    for (i=0; i<argc; i++) {
	l = list_append(l, new_string_str(argv[i]));
    }
    gdict = interp->globals;
    hash_set(gdict, "ARGV", argl);
    hash_set(gdict, "LIB_PATH", new_list(new_string_str(LIB_PATH)));

    /* set ENV */
    l = envl = new_list(new_symbol("dict"));
    env = toy_call(interp, envl);
    if (GET_TAG(env) == DICT) {
	for (; *envp; envp++) {
	    envv = parse_env(*envp, '=');
	    l = envl = new_list(env);
	    l = list_append(l, new_symbol("set"));
	    l = list_append(l, list_get_item(envv));
	    l = list_append(l, list_get_item(list_next(envv)));
	    toy_call(interp, envl);
	}
    }
    hash_set_t(interp->globals, const_ENV, env);

    cmd_pwd(interp, new_list(NULL), new_hash(), 0);

    /* load initial setup file "setup.toy" */
    setupl = new_list(new_symbol("load"));
    list_append(setupl, new_string_str(SETUP_FILE));
    any = toy_eval_script(interp, new_script(new_list(new_statement(setupl, 0))));
    if (GET_TAG(any) == EXCEPTION) {
	    fprintf(stderr, "Exception occurd in setup script file at load '%s', %s.\n",
		    SETUP_FILE, to_string(any));
    }

    return interp;
}

int
toy_add_class(Toy_Interp* interp, char* name, Hash* slot, Toy_Type *delegate) {
    Toy_Type *obj;
    Hash *h;

    if (slot == NULL) {
	if (NULL == (h = new_hash())) return 0;
    } else {
	h = slot;
    }

    obj = new_object(name, h, delegate);
    if (NULL == hash_set(interp->classes, name, obj)) return 0;

    return 1;
}

static Toy_Type* parse_argspec(char* argspec);

void
toy_add_func(Toy_Interp* interp,
	     char* name,
	     Toy_Type* native(Toy_Interp*, Toy_Type*, Hash*, int),
	     char* argspec) {

    Toy_Type *argspec_list;
    Toy_Type *o;

    argspec_list = parse_argspec(argspec);
    o = new_native(native, argspec_list);

    hash_set(interp->funcs, name, o);
}

void
toy_add_method(Toy_Interp* interp,
	       char* class,
	       char* method,
	       Toy_Type* native(Toy_Interp*, Toy_Type*, Hash*, int),
	       char* argspec) {

    Toy_Type *argspec_list;
    Toy_Type *o;
    Toy_Type *h;

    h = hash_get(interp->classes, class);
    if (NULL == h) {
	assert(0);
    }

    argspec_list = parse_argspec(argspec);
    o = new_native(native, argspec_list);

    hash_set(h->u.object.slots, method, o);
}

static Toy_Type*
parse_argspec(char* argspec) {
    Toy_Type *l;
    Cell *c;
    char *p;

    if (argspec == NULL) return NULL;

    p = argspec;
    c = new_cell("");
    l = new_list(NULL);

    if (strlen(argspec) == 0) {
	list_append(l, const_Nil);
	return l;
    }

    while (*p) {
	if (*p == ',') {
	    list_append(l, new_symbol(cell_get_addr(c)));
	    c = new_cell("");
	} else {
	    cell_add_char(c, *p);
	}
	p++;
    }
    list_append(l, new_symbol(cell_get_addr(c)));

    return l;
}

static Toy_Type*
parse_env(char* buff, char sep) {
    Toy_Type *l;
    Cell *c;
    char *p;

    if (buff == NULL) return NULL;

    p = buff;
    c = new_cell("");
    l = new_list(NULL);

    if (strlen(buff) == 0) {
	list_append(l, const_Nil);
	return l;
    }

    while (*p) {
	if (*p == sep) {
	    list_append(l, new_string_str(cell_get_addr(c)));
	    c = new_cell("");
	    p++;
	    cell_add_str(c, p);
	    break;
	} else {
	    cell_add_char(c, *p);
	}
	p++;
    }
    list_append(l, new_string_str(cell_get_addr(c)));

    return l;
}

Toy_Obj_Env*
toy_new_obj_env(Toy_Interp *interp, Toy_Type *cur_object, Toy_Type *self) {
    Toy_Obj_Env *env;

    env = GC_MALLOC(sizeof(Toy_Obj_Env));
    ALLOC_SAFE(env);

    env->cur_object = cur_object;
    env->cur_object_slot = cur_object->u.object.slots;
    env->self = self;

    return env;
}

int
toy_push_new_obj_env(Toy_Interp* interp, char* class, Toy_Type* self) {
    Toy_Obj_Env *env;
    Toy_Type *o; 

    if ((interp->cur_obj_stack+1) >= interp->stack_size) return 0;

    o = new_object("Object", new_hash(), new_list(const_Object));
    if (NULL == (env = toy_new_obj_env(interp, o, o))) {
	return 0;
    }

    interp->cur_obj_stack++;
    interp->obj_stack[interp->cur_obj_stack] = env;

    hash_set_t(interp->obj_stack[interp->cur_obj_stack]->cur_object_slot,
	       const_atname, new_symbol(class));

    return 1;
}

int
toy_push_obj_env(Toy_Interp *interp, Toy_Obj_Env *env) {
    if ((interp->cur_obj_stack+1) >= interp->stack_size) return 0;

    interp->cur_obj_stack++;
    interp->obj_stack[interp->cur_obj_stack] = env;

    return 1;
}

Toy_Obj_Env*
toy_pop_obj_env(Toy_Interp* interp) {
    Toy_Obj_Env *env;

    env = interp->obj_stack[interp->cur_obj_stack];
    interp->obj_stack[interp->cur_obj_stack] = NULL;
    interp->cur_obj_stack--;

    return env;
}

int
toy_push_func_env(Toy_Interp* interp, Hash *localvar, Toy_Func_Env *upenv, Toy_Type *tobe_bind_val) {
    Toy_Func_Env *env;

    if ((interp->cur_func_stack+1) >= interp->stack_size) return 0;

    env = GC_MALLOC(sizeof(Toy_Func_Env));
    ALLOC_SAFE(env);

    env->localvar = localvar;
    env->upstack = upenv;

    env->trace_info = interp->trace_info;
    env->script_id = interp->script_id;
    env->tobe_bind_val = tobe_bind_val;

    interp->cur_func_stack++;
    interp->func_stack[interp->cur_func_stack] = env;

    return 1;
}

Toy_Func_Env*
toy_pop_func_env(Toy_Interp* interp) {
    Toy_Func_Env *env;

    env = interp->func_stack[interp->cur_func_stack];
    interp->func_stack[interp->cur_func_stack] = NULL;
    interp->cur_func_stack--;

    return env;
}

Toy_Env*
new_closure_env(Toy_Interp* interp) {
    Toy_Env *env;

    env = GC_MALLOC(sizeof(Toy_Env));
    ALLOC_SAFE(env);

    env->func_env = interp->func_stack[interp->cur_func_stack];
    env->object_env = interp->obj_stack[interp->cur_obj_stack];
    env->tobe_bind_val = NULL;

    return env;
}


char*
get_stack_trace(Toy_Interp *interp, Cell *stack) {
    char *buff;
    int i;

    buff = GC_MALLOC(256);
    ALLOC_SAFE(buff);

    snprintf(buff, 256, "%s:%d: %s in %s::%s\n",
	     get_script_path(interp, interp->func_stack[interp->cur_func_stack]->script_id),
	     interp->trace_info->line,
	     to_print(interp->trace_info->statement),
	     (interp->func_stack[interp->cur_func_stack]->trace_info->object_name ?
	      		to_string(interp->func_stack[interp->cur_func_stack]->trace_info->object_name) : "self"),
	     (interp->func_stack[interp->cur_func_stack]->trace_info->func_name ?
	      		to_string(interp->func_stack[interp->cur_func_stack]->trace_info->func_name) : "???"));
    buff[255] = 0;
    cell_add_str(stack, buff);

    for (i=interp->cur_func_stack; i>0; i--) {
	snprintf(buff, 256, "%s:%d: %s in %s::%s\n",
		 get_script_path(interp, interp->func_stack[i-1]->script_id),
		 interp->func_stack[i]->trace_info->line,
		 to_print(interp->func_stack[i]->trace_info->statement),
		 (interp->func_stack[i-1]->trace_info->object_name ?
		  		to_string(interp->func_stack[i-1]->trace_info->object_name) : "self"),
		 (interp->func_stack[i-1]->trace_info->func_name ?
		  		to_string(interp->func_stack[i-1]->trace_info->func_name) : "???"));
	buff[255] = 0;
	cell_add_str(stack, buff);
    }

    return cell_get_addr(stack);
}

char*
get_script_path(Toy_Interp* interp, int script_id) {
    Hash *h;
    Toy_Type *cont;
    Toy_Script *s;

    if (script_id <= 0) return "-";

    h = interp->scripts;
    cont = hash_get(h, to_string(new_integer_si(script_id)));
    if (NULL == cont) return "???";

    s = (Toy_Script*)cont->u.container;
    return cell_get_addr(s->path->u.string);
}

#ifdef HAS_GCACHE
void
gcache_clear(Toy_Interp* interp) {
    hash_clear(interp->gcache);
    interp->cache_hit = 0;
    interp->cache_missing = 0;
}
#endif
