#include <editline/readline.h>
#include "mpc.h"

/* Forward declarations */

struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

lenv* lenv_new(void);
void lenv_del(lenv* e);
void lenv_def(lenv* e, lval* k, lval* v);
lenv* lenv_copy(lenv* e);

/* Lisp Value */

enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR };

typedef lval*(*lbuiltin)(lenv*, lval*);

struct lval {
    int type;

    long num;
    char* err;
    char* sym;

    lbuiltin builtin;
    lenv* env;
    lval* formals;
    lval* body;

    int count;
    lval** cell;
};

struct lenv {
    lenv* par;
    int count;
    char** syms;
    lval** vals;
};

/* Create a number type lisp value */
lval* lval_num(long x) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

/* Create a error type lisp value */
lval* lval_err(char* fmt, ...) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;

    /* Create va list and initialize it */
    va_list va;
    va_start(va, fmt);

    /* Allocate 512 bytes of space */
    v->err = malloc(512);

    /* printf into the error string with a maximum of 511 characters */
    vsnprintf(v->err, 511, fmt, va);

    v->err = realloc(v->err, strlen(v->err)+1);
    va_end(va);
    return v;
}

/* Create a symbol type lisp value */
lval* lval_sym(char* s) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}

/* Create a function type lisp value */
lval* lval_fun(lbuiltin func) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_FUN;
    v->builtin = func;
    return v;
}

/* Create a empty lisp value */
lval* lval_sexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

/* Create a quoted lisp value */
lval* lval_qexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

lval* lval_lambda(lval* formals, lval* body) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_FUN;

    /* Set builtin to null */
    v->builtin = NULL;

    /* Build new environment */
    v->env = lenv_new();

    /* Set formals and body */
    v->formals = formals;
    v->body = body;
    return v;
}

void lval_del(lval* v) {
    switch (v->type) {
        case LVAL_NUM: break;
        case LVAL_ERR: free(v->err); break;
        case LVAL_SYM: free(v->sym); break;
        case LVAL_FUN: 
            if (!v->builtin) {
                lenv_del(v->env);
                lval_del(v->formals);
                lval_del(v->body);
            }
        break;
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            for (int i = 0; i < v->count; i++) {
                lval_del(v->cell[i]);
            }
            free(v->cell);
        break;
    }
    free(v);
}

lval* lval_add(lval* v, lval* x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count-1] = x;
    return v;
}

char* ltype_name(int t) {
    switch(t) {
        case LVAL_FUN: return "Function";
        case LVAL_NUM: return "Number";
        case LVAL_ERR: return "Error";
        case LVAL_SYM: return "Symbol";
        case LVAL_SEXPR: return "S-Expression";
        case LVAL_QEXPR: return "Q-Expression";
        default: return "Unknown";
    }
}

void lval_print(lval* v);
lval* lval_eval(lenv* e, lval* v);

void lval_expr_print(lval* v, char open, char close) {
    putchar(open);
    for (int i = 0; i < v->count; i++) {
        lval_print(v->cell[i]);

        if (i != (v->count-1)) {
            putchar(' ');
        }
    }
    putchar(close);
}

void lval_print(lval* v) {
    switch (v->type) {
        case LVAL_NUM: printf("%li", v->num); break;
        case LVAL_ERR: printf("Error: %s", v->err); break;
        case LVAL_SYM: printf("%s", v->sym); break;
        case LVAL_FUN: 
            if (v->builtin) {
                printf("<function>"); 
            } else {
                printf("(\\ "); lval_print(v->formals); putchar(' '); lval_print(v->body); putchar(')');
            }
        break;
        case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
        case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
    }
}

void lval_println(lval* v) { lval_print(v); putchar('\n'); }

lval* lval_copy(lval* v) {
    lval* x = malloc(sizeof(lval));
    x->type = v->type;

    switch (v->type) {

        /* Copy numbers and functions directly */
        case LVAL_FUN: 
            if (v->builtin) {
                x->builtin = v->builtin;
            } else {
                x->builtin = NULL;
                x->env = lenv_copy(v->env);
                x->formals = lval_copy(v->formals);
                x->body = lval_copy(v->body);
            }
        break;
        case LVAL_NUM: x->num = v->num; break;
                       
        case LVAL_ERR: x->err = malloc(strlen(v->err) + 1); strcpy(x->err, v->err); break;
        case LVAL_SYM: x->sym = malloc(strlen(v->sym) + 1); strcpy(x->sym, v->sym); break;

        case LVAL_SEXPR:
        case LVAL_QEXPR:
            x->count = v->count;
            x->cell = malloc(sizeof(lval*) * x->count);
            for (int i = 0; i < x->count; i++) {
                x->cell[i] = lval_copy(v->cell[i]);
            }
        break;
    }

    return x;
}

lval* lval_pop(lval* v, int i) {
    /* Find the item at the given index */
    lval* x = v->cell[i];

    /* Shift the memory following the item at "i" over the top of it */
    memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count-i-1));

    /* Decrease the count of items in the list */
    v->count--;

    /* Reallocate the memory used */
    v-> cell = realloc(v->cell, sizeof(lval*) * v->count);
    return x;
}

lval* lval_take(lval* v, int i) {
    lval* x = lval_pop(v, i);
    lval_del(v);
    return x;
}

lenv* lenv_new(void) {
    lenv* e = malloc(sizeof(lval));
    e->par = NULL;
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
}

void lenv_del(lenv* e) {
    for (int i = 0; i < e->count; i++) {
        free(e->syms[i]);
        lval_del(e->vals[i]);
    }
    free(e->syms);
    free(e->vals);
    free(e);
}

lval* lenv_get(lenv* e, lval* k) {

    for (int i = 0; i < e->count; i++) {
        /* Check for the given symbol and return a copy of the lval if found */
        if (strcmp(e->syms[i], k->sym) == 0) { return lval_copy(e->vals[i]); }
    }

    /* Check in parent or return an error */
    if (e->par) {
        return lenv_get(e->par, k);
    } else {
        return lval_err("Unbound symbol '%s'", k->sym);
    }
}

void lenv_put(lenv* e, lval* k, lval* v) {
    /* Overwrite if symbol is already in lenv */
    for (int i = 0; i < e->count; i++) {
        if (strcmp(e->syms[i], k->sym) == 0) {
            lval_del(e->vals[i]);
            e->vals[i] = lval_copy(v);
            return;
        }
    }

    /* If adding a new entry allocate space */
    e->count++;
    e->vals = realloc(e->vals, sizeof(lval*) * e->count);
    e->syms = realloc(e->syms, sizeof(char*) * e->count);

    /* Copy lval and symbol into new location */
    e->vals[e->count-1] = lval_copy(v);
    e->syms[e->count-1] = malloc(strlen(k->sym)+1);
    strcpy(e->syms[e->count-1], k->sym);
}

void lenv_def(lenv* e, lval* k, lval* v) {
    while(e->par) { e = e->par; }
    lenv_put(e, k, v);
}

lenv* lenv_copy(lenv* e) {
    lenv* n = malloc(sizeof(lenv));
    n->par = e->par;
    n->count = e->count;
    n->syms = malloc(sizeof(char*) * n->count);
    n->vals = malloc(sizeof(lval*) * n->count);
    for (int i = 0; i < e->count; i++) {
        n->syms[i] = malloc(strlen(e->syms[i]) + 1);
        strcpy(n->syms[i], e->syms[i]);
        n->vals[i] = lval_copy(e->vals[i]);
    }
    return n;
}

#define LASSERT(args, cond, fmt, ...) if (!(cond)) { lval* err = lval_err(fmt, ##__VA_ARGS__); lval_del(args); return err; }
#define LASSERT_NUM(func, args, num)\
    LASSERT(args, (args->count == num), "Function '%s' passed too many arguments. Got %i, Expected %i.", func, a->count, num); 
#define LASSERT_TYPE(func, args, index, expect) \
    LASSERT(args, (args->cell[index]->type == expect), "Function '%s' passed incorrect type for argument %i.  Got %s, expected %s", func, index, ltype_name(args->cell[index]->type), ltype_name(LVAL_QEXPR));

lval* builtin_head(lenv* e, lval* a) {
    LASSERT_NUM("head", a, 1); 
    LASSERT_TYPE("head", a, 0, LVAL_QEXPR);
    LASSERT(a, (a->cell[0]->count != 0), "Function 'head' passed {}");

    lval* v = lval_take(a, 0);
    while (v->count > 1) { lval_del(lval_pop(v, 1)); }
    return v;
}

lval* builtin_tail(lenv* e, lval* a) {
    LASSERT_NUM("tail", a, 1); 
    LASSERT_TYPE("tail", a, 0, LVAL_QEXPR);
    LASSERT(a, (a->cell[0]->count != 0), "Function 'tail' passed {}");

    lval* v = lval_take(a, 0);
    lval_del(lval_pop(v, 0));
    return v;
}

lval* builtin_list(lenv* e, lval* a) {
    a->type = LVAL_QEXPR;
    return a;
}

lval* builtin_lambda(lenv* e, lval* a) {
    LASSERT_NUM("\\", a, 2);
    LASSERT_TYPE("\\", a, 0, LVAL_QEXPR);
    LASSERT_TYPE("\\", a, 1, LVAL_QEXPR);

    /* Check first Q-expression only contains symbols*/
    lval* syms = a->cell[0];

    for (int i = 0; i < syms->count; i++) {
        LASSERT(a, (syms->cell[i]->type == LVAL_SYM), "Cannot define non-symbol. Got %s, expected %s", ltype_name(syms->cell[i]->type), ltype_name(LVAL_SYM));
    }

    lval* formals = lval_pop(a, 0);
    lval* body = lval_pop(a, 0);
    lval_del(a);
    return lval_lambda(formals, body);
}

lval* lval_join(lval* x, lval* y) {

    /* For each cell in 'y' add it to 'x' */
    while (y->count) {
        x = lval_add(x, lval_pop(y, 0));
    }

    /* Delete empty 'y' and return 'x' */
    lval_del(y);
    return x;
}

lval* builtin_join(lenv* e, lval* a) {
    for (int i = 0; i < a->count; i++) {
        LASSERT_TYPE("join", a, i, LVAL_QEXPR);
    }

    lval* x = lval_pop(a, 0);

    while(a->count) {
        x = lval_join(x, lval_pop(a, 0));
    }

    lval_del(a);
    return x;
}

lval* builtin_eval(lenv* e, lval* a) {
    LASSERT_NUM("eval", a, 1); 
    LASSERT_TYPE("eval", a, 0, LVAL_QEXPR);

    lval* x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(e, x);
}

lval* builtin_op(lenv* e, lval* a, char* op) {

    /* Ensure all arguments are numbers */
    for (int i =0; i < a->count; i++) {
        if (a->cell[i]->type != LVAL_NUM) {
            lval_del(a);
            return lval_err("Cannot apply operator to non number");
        }
    }

    /* Pop the first element */
    lval* x = lval_pop(a, 0);

    /* If no arguments and is a subtract operation then perform unary negation */
    if ((strcmp(op, "-") == 0) && a->count == 0) { x->num = -x->num; }

    while (a->count > 0) {
        lval* y = lval_pop(a, 0);


        if (strcmp(op, "+") == 0) { x->num += y->num; }
        if (strcmp(op, "-") == 0) { x->num -= y->num; }
        if (strcmp(op, "*") == 0) { x->num *= y->num; }
        if (strcmp(op, "/") == 0) {
            if(y->num == 0) {
                lval_del(x);
                lval_del(y);
                x = (lval_err("Divisiion by zero")); break;
            }
            x->num /= y->num;
        }

        /* Delete element now finished with it */
        lval_del(y);
    }

    /* Delete input expression and return result */
    lval_del(a);
    return x;
}

lval* builtin_var(lenv* e, lval* a, char* func) {
    LASSERT_TYPE(func, a, 0, LVAL_QEXPR);

    /* First asrgument is symbol list */
    lval* syms = a->cell[0];

    /* Ensure all elements of first list are symbols */
    for (int i = 0; i < syms->count; i++) {
        LASSERT(a, (syms->cell[i]->type == LVAL_SYM), "Function '%s' cannot define non-symbol at index '%i'", func, i);
    }

    /* Check correct number of symbols and values */
    LASSERT(a, (syms->count == a->count-1), "Function '%s' cannot define incorrect number of values to symbols", func);

    /* Assign copies of values to symbols */
    for (int i = 0; i < syms->count; i++) {
        /* If 'def' define in global scope.  If 'put' define in local scope */
        if (strcmp(func, "def") == 0) { lenv_def(e, syms->cell[i], a->cell[i+1]); }
        if (strcmp(func, "=") == 0) { lenv_put(e, syms->cell[i], a->cell[i+1]); }
    }

    lval_del(a);
    return lval_sexpr();
}

lval* builtin_def(lenv* e, lval* a) { return builtin_var(e, a, "def"); }
lval* builtin_put(lenv* e, lval* a) { return builtin_var(e, a, "="); }

lval* builtin_add(lenv* e, lval* a) { return builtin_op(e, a, "+"); }
lval* builtin_sub(lenv* e, lval* a) { return builtin_op(e, a, "-"); }
lval* builtin_mul(lenv* e, lval* a) { return builtin_op(e, a, "*"); }
lval* builtin_div(lenv* e, lval* a) { return builtin_op(e, a, "/"); }
        
lval* builtin(lenv* e, lval* a, char* func) {
    if (strcmp("list", func) == 0) { return builtin_list(e, a); }
    if (strcmp("head", func) == 0) { return builtin_head(e, a); }
    if (strcmp("tail", func) == 0) { return builtin_tail(e, a); }
    if (strcmp("join", func) == 0) { return builtin_join(e, a); }
    if (strcmp("eval", func) == 0) { return builtin_eval(e, a); }
    if (strstr("+-*/", func)) { return builtin_op(e, a, func); }
    lval_del(a);
    return lval_err("Unkown function");
}

void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
    lval* k = lval_sym(name);
    lval* v = lval_fun(func);
    lenv_put(e, k, v);
    lval_del(k);
    lval_del(v);
}

void lenv_add_builtins(lenv* e) {
    /* List functions */
    lenv_add_builtin(e, "list", builtin_list);
    lenv_add_builtin(e, "head", builtin_head);
    lenv_add_builtin(e, "tail", builtin_tail);
    lenv_add_builtin(e, "eval", builtin_eval);
    lenv_add_builtin(e, "join", builtin_join);
    lenv_add_builtin(e, "\\", builtin_lambda);
    lenv_add_builtin(e, "def", builtin_def);
    lenv_add_builtin(e, "=", builtin_put);

    lenv_add_builtin(e, "+", builtin_add);
    lenv_add_builtin(e, "-", builtin_sub);
    lenv_add_builtin(e, "*", builtin_mul);
    lenv_add_builtin(e, "/", builtin_div);
}

lval* lval_call(lenv* e, lval* f, lval* a) {
    if(f->builtin) { return f->builtin(e, a); }

    int given = a->count;
    int total = f->formals->count;

    while(a->count) {
        
        if ( f->formals->count == 0) {
            lval_del(a);
            return lval_err("Function passed to many arguments.  Got %i, expected %i.", given, total);
        }

        lval* sym = lval_pop(f->formals, 0);

        lval* val = lval_pop(a, 0);

        lenv_put(f->env, sym, val);

        lval_del(sym); lval_del(val);
    }

    /* Arguments have been bound to environment so we can delete them */
    lval_del(a);

    /* Return evealuated value if all arguments have been bound otherwise return partial */
    if (f->formals->count == 0) {
        f->env->par = e;
        return builtin_eval(f->env, lval_add(lval_sexpr(), lval_copy(f->body)));
    } else {
        return lval_copy(f);
    }
}

lval* lval_eval_sexpr(lenv* e, lval* v) {

    /* Evaluate children */
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(e, v->cell[i]);
    }

    /* Error checking */
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
    }

    /* Empty expression */
    if (v->count == 0) { return v; }

    /* Single expression */
    if (v->count == 1) { return lval_take(v, 0); }

    /* Ensure first element is a symbol */
    lval* f = lval_pop(v, 0);
    if (f->type != LVAL_FUN) {
        lval* err = lval_err("S-expression starts with incorrect type. Got %s, expected %s.", ltype_name(f->type), ltype_name(LVAL_FUN));
        lval_del(f);
        lval_del(v);
        return err;
    }

    /* Call builtin with operator */
    lval* result = lval_call(e, f, v);
    lval_del(f);
    return result;
}

lval* lval_eval(lenv* e, lval* v) {
    if (v->type == LVAL_SYM) {
        lval* x = lenv_get(e, v);
        lval_del(v);
        return x;
    }

    /* Evalute s-expressions */
    if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(e, v); }
    /* Just return anything else as is */
    return v;
}

lval* lval_read_num(mpc_ast_t* t) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

lval* lval_read(mpc_ast_t* t) {
    /* If symbol or number return lval of that type */
    if (strstr(t->tag, "number")) { return lval_read_num(t); }
    if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

    /* If root or s expression create an empty lval */
    lval* x = NULL;

    if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
    if (strstr(t->tag, "sexpr")) { x = lval_sexpr(); }
    if (strstr(t->tag, "qexpr")) { x = lval_qexpr(); }

    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
        if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
}

int main(int argc, char** argv) {
    /* Create some parsers */
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr = mpc_new("sexpr");
    mpc_parser_t* Qexpr = mpc_new("qexpr");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");

    mpca_lang(MPC_LANG_DEFAULT,
        "\
            number : /-?[0-9]+/ ; \
            symbol : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ; \
            sexpr : '(' <expr>* ')' ; \
            qexpr : '{' <expr>* '}' ; \
            expr : <number> | <symbol> | <sexpr> | <qexpr> ; \
            lispy : /^/ <expr>* /$/ ; \
        ",
        Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

    puts("Lispy Version 0.0.0.0.1");
    puts("Press Ctrl+c to Exit\n");

    lenv* e = lenv_new();
    lenv_add_builtins(e);

    while(1) {

        char* input = readline("lispy> ");
        add_history(input);

        /* Echo back to use */
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispy, &r)) {
            //mpc_ast_print(r.output);
            lval* x = lval_eval(e, lval_read(r.output));
            lval_println(x);
            lval_del(x);
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    lenv_del(e);

    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
    return 0;
}
