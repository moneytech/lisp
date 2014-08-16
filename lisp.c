#include "lisp.h"

#include <stdlib.h> // calloc
#include <string.h> // memset
#include <stdio.h>  // fprintf
#include <assert.h> // assert

#define NEW(t) (t *)calloc(sizeof(t), 1)
#define NEW_ARRAY(t, l) (t *)calloc(sizeof(t), l)
#define FREE_ARRAY(e) free(e)
#define FREE(e) free(e)
#define ERROR_UNLESS_IS_TYPE(frame, e, t)        \
  if ( ! is_type(e, t) ) {   \
    return new_error(frame, "Type mismatch");             \
  };

struct elem EMPTY_LIST = { 
  .type = ELEM_TYPE_LIST,
  .lval.value = 0,
  .lval.next = 0
};

struct elem EMPTY_SET = { 
  .type = ELEM_TYPE_SET,
  .lval.value = 0,
  .lval.next = 0 
};

struct elem EMPTY_MAP  = { 
  .type       = ELEM_TYPE_MAP,
  .mval.key   = 0,
  .mval.value = 0,
  .mval.next  = 0
};

#define DEFINE_SYM(name, text)                    \
  struct elem name  = {                           \
    .type       = ELEM_TYPE_SYM,                  \
    .sval.len   = sizeof(#text),                  \
    .sval.str   = (char *)#text                   \
  };                                              \
  struct elem *sym_##text() {                     \
    return &name;                                 \
  }                                               \

DEFINE_SYM(SYM_ALLOC,  alloc)
DEFINE_SYM(SYM_ENV,    env)
DEFINE_SYM(SYM_RHS,    rhs)
DEFINE_SYM(SYM_LHS,    lhs)
DEFINE_SYM(SYM_PARENT, parent)
DEFINE_SYM(SYM_MSG,    msg)
DEFINE_SYM(SYM_FRAME,  frame)
DEFINE_SYM(SYM_ERROR,  error)
DEFINE_SYM(SYM_CURR_CHAR, curr_char)
DEFINE_SYM(SYM_EXPR, expr)
DEFINE_SYM(SYM_POS, pos)
DEFINE_SYM(SYM_INPUT, input)
DEFINE_SYM(SYM_PRINTLN, println)

struct elem NIL        = { 
  .type = ELEM_TYPE_NIL,
};

struct elem TRUE        = { 
  .type = ELEM_TYPE_TRUE,
};

struct elem FALSE        = { 
  .type = ELEM_TYPE_FALSE,
  .lval.value = 0,
};

struct elem* frame_get(struct elem *frame, struct elem *key);

struct elem *map_get(struct elem *frame, struct elem *m, struct elem *k);
struct elem *map_set(struct elem *frame, struct elem *m, struct elem *k, struct elem *v);

struct elem *nil() {
  return &NIL;
}

struct elem *false_value() {
  return &FALSE;
}

struct elem *true_value() {
  return &TRUE;
}

int is_type(struct elem *e, int type) {
  return e->type == type;
}

int is_nil(struct elem *e) {
  return e == nil();
}

int is_true(struct elem *expr) {
  return expr != nil() && ! is_type(expr, ELEM_TYPE_ERROR) && expr != false_value();
}

int is_false(struct elem *frame, struct elem *expr) {
  return expr != nil() || is_type(expr, ELEM_TYPE_ERROR) || expr == false_value();
}

struct elem *empty_list() {
  return &EMPTY_LIST;
}

struct elem *empty_map() {
  return &EMPTY_MAP;
}

int list_is_empty(struct elem *t) {
  return t == &EMPTY_LIST;
}

int map_is_empty(struct elem *t) {
  return t == &EMPTY_MAP;
}

int set_is_empty(struct elem *t) {
  return t == &EMPTY_SET;
}

struct elem *list_value(struct elem *s) {
  return s->lval.value;
}

struct elem *list_next(struct elem *s) {
  return s->lval.next;
}

struct elem *set_value(struct elem *s) {
  return s->lval.value;
}

struct elem *set_next(struct elem *s) {
  return s->lval.next;
}

struct elem *map_key(struct elem *s) {
  return s->mval.key;
}

struct elem *map_value(struct elem *s) {
  return s->mval.value;
}

struct elem *map_next(struct elem *s) {
  return s->mval.next;
}

int is_list(struct elem *s) {
  return s->type == ELEM_TYPE_LIST;
}

int is_ident(struct elem *s) {
  return s->type == ELEM_TYPE_IDENT;
}

int is_fn(struct elem *s) {
  return s->type == ELEM_TYPE_FN;
}

int int_value(struct elem *e) {
  return e->ival.value;
}

int is_sym(struct elem *s) {
  return s->type == ELEM_TYPE_SYM;
}

struct elem *new_alloc_elem() {
  struct elem  *e = NEW(struct elem);
  e->type = ELEM_TYPE_ALLOC;
  e->aval.alloc = NEW(struct alloc);
  e->aval.alloc->len = 1000;
  e->aval.alloc->tail = 0;
  e->aval.alloc->table = NEW_ARRAY(struct elem, e->aval.alloc->len);
  e->aval.alloc->free_list = 0;
  return e;
}

void free_alloc_elem(struct elem *a) {
  int i;
  struct elem *e;
  for(i=0;i<a->aval.alloc->tail;++i) {
    e = a->aval.alloc->table + i;
    switch(e->type) {
    case ELEM_TYPE_IDENT:
    case ELEM_TYPE_STRING:
    case ELEM_TYPE_SYM:
      FREE_ARRAY(e->sval.str);
      break;
    }
  }
  FREE_ARRAY(a->aval.alloc->table);
  FREE(a->aval.alloc);
  FREE(a);
}

struct elem *alloc_elem(struct elem *alloc_elem) {
  struct alloc *alloc = alloc_elem->aval.alloc;
  struct elem *ret;
  if ( alloc->free_list != 0 ) {
    ret = alloc->free_list;
    alloc->free_list = alloc->free_list->lval.next;
    memset(ret, 0, sizeof(struct elem));
    return ret;
  } else {
    if ( alloc->tail < alloc->len ) {
      ret = alloc->table + alloc->tail++;
      memset(ret, 0, sizeof(struct elem));
      return ret;
    } else {
      ret = nil();
    }
  }
  return ret;
}

struct elem *frame_alloc_elem(struct elem *frame) {
  struct elem *a = map_get(frame, frame, sym_alloc());
  return alloc_elem(a);
}

struct elem *new_int(struct elem *frame, int i) {
  struct elem *ret = frame_alloc_elem(frame);
  if ( ret != 0 ) {
    ret->type = ELEM_TYPE_INT;
    ret->ival.value = i;
  }
  return ret;
}

struct elem *new_string_like(struct elem *frame, char *s, int type) {
  struct elem *ret = frame_alloc_elem(frame);
  ret->type = type;
  ret->sval.len = strlen(s) + 1;
  ret->sval.str = NEW_ARRAY(char, ret->sval.len+1);
  memcpy(ret->sval.str, s, ret->sval.len);
  return ret;
}

struct elem *new_string(struct elem *frame, char *s) {
  return new_string_like(frame, s, ELEM_TYPE_STRING);
}

struct elem *new_ident(struct elem *frame, char *s) {
  return new_string_like(frame, s, ELEM_TYPE_IDENT);
}

char *c_str(struct elem *e) {
  assert(is_type(e, ELEM_TYPE_STRING) || is_type(e, ELEM_TYPE_IDENT));
  return e->sval.str;
}

struct elem *new_sym(struct elem *frame, char *s) {
  return new_string_like(frame, s, ELEM_TYPE_SYM);
}

struct elem *new_fn(struct elem *frame, fn *fn) {
  struct elem *ret = frame_alloc_elem(frame);
  ret->type = ELEM_TYPE_FN;
  ret->fval.fn = fn;
  return ret;
}

struct elem *new_root_frame() {
  struct elem *a = new_alloc_elem();
  struct elem *f = alloc_elem(a);
  f->type = ELEM_TYPE_MAP;
  f->mval.key = sym_alloc();
  f->mval.value = a;
  f->mval.next = empty_map();
  return f;
}

void free_root_frame(struct elem *frame) {
  free_alloc_elem(frame_get(frame, sym_alloc()));
}

struct elem *to_sym(struct elem *frame, struct elem *ident) {
  return new_sym(frame, c_str(ident));
}

struct elem *alloc_map(
  struct elem *frame, 
  struct elem *m, 
  struct elem *k, 
  struct elem *v
) {
  struct elem *ret = frame_alloc_elem(frame);
  ret->type = ELEM_TYPE_MAP;
  ret->mval.key = k;
  ret->mval.value = v;
  ret->mval.next = m;
  return ret;
}

struct elem *alloc_list(
  struct elem *frame, 
  struct elem *l, 
  struct elem *v
) {
  struct elem *ret = frame_alloc_elem(frame);
  ret->type = ELEM_TYPE_LIST;
  ret->lval.value = v;
  ret->lval.next = l;
  return ret;
}

struct elem *alloc_set(
  struct elem *frame, 
  struct elem *s, 
  struct elem *v
) {
  struct elem *ret = frame_alloc_elem(frame);
  ret->type = ELEM_TYPE_SET;
  ret->lval.value = v;
  ret->lval.next = s;
  return ret;
}

int list_contains(struct elem *frame, struct elem *s, struct elem *x) {
  if ( list_is_empty(s) ) {
    return 0;
  }
  if ( elem_eq(frame, list_value(s), x) ) {
    return 1;
  }
  return list_contains(frame, list_next(s), x);
}

int set_contains(struct elem *frame, struct elem *s, struct elem *x) {
  if ( set_is_empty(s) ) {
    return 0;
  }
  if ( elem_eq(frame, set_value(s), x) ) {
    return 1;
  }
  return set_contains(frame, set_next(s), x);
}

int map_contains_key(struct elem *frame, struct elem *m, struct elem *x) {
  if ( map_is_empty(m) ) {
    return 0;
  }
  if ( elem_eq(frame, map_key(m), x) ) {
    return 1;
  }
  return map_contains_key(frame, map_next(m), x);
}
  

int set_subset_eq(struct elem *frame, struct elem *a, struct elem *b) {
  if ( a == b ) {
    return 1;
  }
  if ( list_is_empty(a) ) {
    return 0;
  }
  if ( list_is_empty(b) ) {
    return 0;
  }
  if ( ! set_contains(frame, list_value(a), b) ) {
    return 0;
  }
  return list_eq(frame, list_next(a), b);
}

int set_eq(struct elem *frame, struct elem *a, struct elem *b) {
  return set_subset_eq(frame, a, b) && set_subset_eq(frame, b, a);
}

int list_sublist_eq(struct elem *frame, struct elem *a, struct elem *b) {
  if ( a == b ) {
    return 1;
  }
  if ( list_is_empty(a) ) {
    return 0;
  }
  if ( list_is_empty(b) ) {
    return 1;
  }
  if ( ! elem_eq(frame, list_value(a), list_value(b)) ) {
    return 0;
  }
  return list_sublist_eq(frame, list_next(a), list_next(b));
}

int list_eq(struct elem *frame, struct elem *a, struct elem *b) {
  if ( a == b ) {
    return 1;
  }
  if ( list_is_empty(a) ) {
    return 0;
  }
  if ( list_is_empty(b) ) {
    return 0;
  }
  if ( ! elem_eq(frame, list_value(a), list_value(b)) ) {
    return 0;
  }
  return list_eq(frame, list_value(a), list_value(b)); 
}

int map_submap_eq(struct elem *frame, struct elem *a, struct elem *b) {
  if ( map_is_empty(a) ) {
    return 1;
  }
  struct elem *key = a->mval.key;
  struct elem *value = a->mval.value;
  return elem_eq(frame, map_get(frame, b, key), value) && map_submap_eq(frame, map_next(a), b);
}

int map_eq(struct elem *frame, struct elem *a, struct elem *b) {
  return map_submap_eq(frame, a, b) && map_submap_eq(frame, b, a);
}

int ival_eq(struct elem *frame, struct elem *a, struct elem *b) {
  return a->ival.value == b->ival.value;
}

int sval_eq(struct elem *frame, struct elem *a, struct elem *b) {
  int i;
  if ( a->sval.len != b->sval.len ) {
    return 0;
  }
  for(i=0;i<a->sval.len;++i) {
    if (a->sval.str[i] != b->sval.str[i]) {
      return 0;
    }
  }
  return 1;
}

int elem_eq(struct elem *frame, struct elem *a, struct elem *b) {
  if ( a == b ) {
    return 1;
  }
  if ( a->type != b->type ) {
    return 0;
  }
  switch(a->type) {
  case ELEM_TYPE_NIL:
    return 0;
  case ELEM_TYPE_INT:
    return ival_eq(frame, a, b);
  case ELEM_TYPE_LIST:
    return list_eq(frame, a, b);
  case ELEM_TYPE_SYM:
  case ELEM_TYPE_IDENT:
  case ELEM_TYPE_STRING:
    return sval_eq(frame, a, b);
  case ELEM_TYPE_ERROR:
    return 0;
  case ELEM_TYPE_SET:
    return set_eq(frame, a, b);
  case ELEM_TYPE_MAP:
    return map_eq(frame, a, b);
  case ELEM_TYPE_FN:
    return 0;
  default:
    abort(); // invalid type
  }
  return 0;
}

struct elem *map_get(struct elem *frame, struct elem *m, struct elem *k) {
  while(! map_is_empty(m)) {
    if ( elem_eq(frame, k, map_key(m)) ) {
      return map_value(m);
    }
    m = map_next(m);
  }
  return nil();
}

struct elem *map_set(
  struct elem *frame, 
  struct elem *m, 
  struct elem *k, 
  struct elem *v
) {
  if ( elem_eq(frame, map_get(frame, m, k), v) ) {
    return m;
  } else {
    return alloc_map(frame, m, k, v);
  }
}

struct elem *list_add(
  struct elem *frame,
  struct elem *l,
  struct elem *v
) {
  return alloc_list(frame, l, v);
}

struct elem *set_add(
  struct elem *frame,
  struct elem *s,
  struct elem *v
) {
  if ( ! set_contains(frame, s, v) ) {
    return alloc_set(frame, s, v);
  } else {
    return s;
  }
}

struct elem* trim_map(struct elem *frame, struct elem* m) {
  struct elem *r  = empty_map();
  struct elem *k;

  while( ! map_is_empty(m) ) {
    k = map_key(m);
    if ( ! map_contains_key(frame, r, k) ) {
      r = map_set(frame, r, k, map_value(m));
    }
    m = map_next(m);
  }

  return r;
}

struct elem* env_set(struct elem *frame, struct elem *key, struct elem *value) {
  struct elem* env = map_get(frame, frame, sym_env());
  env = map_set(frame, env, key, value);
  return map_set(frame, frame, sym_env(), env);
}

struct elem* env_get(struct elem *frame, struct elem *key) {
  struct elem* env = map_get(frame, frame, sym_env());
  return map_get(frame, env, key);
}

struct elem* frame_set(struct elem *frame, struct elem *key, struct elem *value) {
  return map_set(frame, frame, key, value);
}

struct elem* frame_get(struct elem *frame, struct elem *key) {
  return map_get(frame, frame, key);
}

struct elem* new_child_frame(struct elem* frame, 
                             struct elem *parent_frame, 
                             struct elem *e) {
  struct elem* nf = empty_map();
  nf = map_set(frame, nf, sym_parent(), parent_frame);
  nf = map_set(frame, nf, sym_env(), empty_map());
  nf = map_set(frame, nf, sym_lhs(), empty_list());
  nf = map_set(frame, nf, sym_rhs(), e);
  nf = map_set(frame, nf, sym_alloc(), frame_get(frame, sym_alloc()));
  return nf;
}

struct elem *new_error(struct elem *frame, char *str) {
  struct elem *e = frame_alloc_elem(frame);
  struct elem *m = empty_map();

  m = map_set(frame, m, sym_msg(), new_string(frame, str));
  m = map_set(frame, m, sym_frame(), frame);
  
  e->type = ELEM_TYPE_ERROR;
  e->eval.map = m;
  
  return e;
}

struct elem *list_reverse(struct elem *frame, struct elem *l) {
  struct elem *r = empty_list();
  while( ! list_is_empty(l) ) {
    r = list_add(frame, r, list_value(l));
    l = list_next(l);
  }
  return r;
}

struct elem *frame_error(struct elem *frame, struct elem *error) {
  abort();
}

struct elem *frame_call(
  struct elem *frame, 
  struct elem *parent_frame, 
  struct elem *fn, 
  struct elem *args
) {
  if ( fn->fval.fn != 0 ) {
    struct elem *child_frame = new_child_frame(frame, parent_frame, args);
    return fn->fval.fn(child_frame);
  } else {
    struct elem *child_frame = new_child_frame(frame, parent_frame, fn->fval.expr);
    struct elem *fn_args = fn->fval.args;
    while( ! list_is_empty(args) && ! list_is_empty(fn_args) ) {
      env_set(child_frame, list_value(args), list_value(fn_args));
      args = list_next(args);
      fn_args = list_next(fn_args);
    }
    return child_frame;
  }
}

struct elem *frame_eval(struct elem *frame) 
{
  struct elem *lhs, *rhs, *value, *fn, *ret, *parent, *args;

  while(1) {

    rhs = frame_get(frame, sym_rhs());

    if ( ! is_list(rhs) ) {
      frame = frame_set(frame, sym_lhs(), rhs);
      frame = frame_set(frame, sym_rhs(), empty_list());
      continue;
    }
    
    lhs = frame_get(frame, sym_lhs());
    if ( list_is_empty(rhs) ) {
      if ( is_list(lhs) ) {
        if ( ! list_is_empty(lhs) ) {
          lhs = list_reverse(frame, lhs);
          fn = list_value(lhs);
          args = list_next(lhs);
          if ( ! is_fn(fn) ) {
            return frame_error(frame, new_error(frame, "Expected function"));
          }
          frame = frame_call(frame, frame_get(frame, sym_parent()), fn, args);
          continue;
        }
      } 

      // return from current frame
      parent = frame_get(frame, sym_parent());
      if ( is_nil(parent) ) {
        return frame_get(frame, sym_lhs());
      }

      ret   = lhs;
      lhs   = frame_get(frame, sym_parent());
      frame = frame_set(frame, sym_lhs(), list_add(frame, lhs, ret));
      continue;
    }

    value = list_value(rhs);
    rhs = list_next(rhs);
    
    if ( is_list(value) ) {
      frame = frame_set(frame, sym_rhs(), rhs); 
      frame = new_child_frame(frame, frame, value);
      continue;
    }
    
    if ( is_ident(value) ) {
      value = env_get(frame, to_sym(frame, value));
    }
    
    lhs = list_add(frame, lhs, value);
    frame = frame_set(frame, sym_lhs(), lhs);
    frame = frame_set(frame, sym_rhs(), rhs);
  }
}

void elem_print(struct elem *frame, FILE *out, struct elem *l);

void list_print(struct elem *frame, FILE *out, struct elem *l) {
  int first = 1;
  fprintf(out, "(");
  while(! list_is_empty(l)) {
    if ( first ) {
      first = 0;
    } else {
      fprintf(out, " ");
    }
    elem_print(frame, out, list_value(l));
    l = list_next(l);
  }
  fprintf(out, ")");
}

void set_print(struct elem *frame, FILE *out, struct elem *l) {
  fprintf(out, "#{");
  int first = 1;
  while(! set_is_empty(l)) {
    if ( first ) {
      first = 0;
    } else {
      fprintf(out, " ");
    }
    elem_print(frame, out, set_value(l));
    l = set_next(l);
  }
  fprintf(out, "}");
}

void map_print(struct elem *frame, FILE *out, struct elem *l) {
  int first = 1;
  fprintf(out, "{");
  while(! map_is_empty(l)) {
    if ( first ) {
      first = 0;
    } else {
      fprintf(out, " ");
    }
    elem_print(frame, out, map_key(l));
    fprintf(out, " ");
    elem_print(frame, out, map_value(l));
    l = map_next(l);
  }
  fprintf(out, "}");
}

void sval_print(struct elem *frame, FILE *out, struct elem *s) {
  fprintf(out, "%s", s->sval.str);
}

void string_print(struct elem *frame, FILE *out, struct elem *s) {
  fprintf(out, "\"");
  sval_print(frame, out, s);
  fprintf(out, "\"");
}

void ident_print(struct elem *frame, FILE *out, struct elem *s) {
  sval_print(frame, out, s);
}

void error_print(struct elem *frame, FILE *out, struct elem *s) {
  fprintf(out, "<err:");
  elem_print(frame, out, s->eval.map);
  fprintf(out, ">");
}

void sym_print(struct elem *frame, FILE *out, struct elem *s) {
  fprintf(out, ":");
  sval_print(frame, out, s);
}

void fn_print(struct elem *frame, FILE *out, struct elem *e) {
  fprintf(out, "<fn:%p>", e->fval.fn);
}

struct elem *elem_read(struct elem *frame);
struct elem *map_read(struct elem *frame);
struct elem *list_read(struct elem *frame);
struct elem *string_read(struct elem *frame);
struct elem *ident_read(struct elem *frame);
struct elem *symbol_read(struct elem *frame);

struct elem *reader_set_expr(struct elem *frame, struct elem *expr) {
  return frame_set(frame, sym_expr(), expr);
}

struct elem *reader_get_expr(struct elem *frame) {
  return frame_get(frame, sym_expr());
}

struct elem *reader_set_error(struct elem *frame, struct elem *err) {
  return frame_set(frame, sym_error(), err);
}

struct elem *reader_get_error(struct elem *frame) {
  return frame_get(frame, sym_error());
}

struct elem *reader_set_curr_char(struct elem *frame, struct elem *c) {
  return frame_set(frame, sym_curr_char(), c);
}

struct elem *reader_get_curr_char(struct elem *frame) {
  return frame_get(frame, sym_curr_char());
}

struct elem *reader_set_pos(struct elem *frame, struct elem *e) {
  return frame_set(frame, sym_pos(), e);
}

struct elem *reader_get_pos(struct elem *frame) {
  return frame_get(frame, sym_pos());
}

struct elem *reader_set_input(struct elem *frame, struct elem *e) {
  return frame_set(frame, sym_input(), e);
}

struct elem *reader_get_input(struct elem *frame) {
  return frame_get(frame, sym_input());
}

int reader_has_error(struct elem *frame) {
  return frame_get(frame, sym_error()) != nil();
}

struct elem *int_leq(struct elem *frame, struct elem *a, struct elem *b) {
  ERROR_UNLESS_IS_TYPE(frame, a, ELEM_TYPE_INT);
  ERROR_UNLESS_IS_TYPE(frame, b, ELEM_TYPE_INT);
  if ( int_value(a) < int_value(b) ) {
    return true_value();
  } else {
    return nil();
  }
}

struct elem *string_char_at(struct elem *frame, struct elem *s, struct elem *p) {
  ERROR_UNLESS_IS_TYPE(frame, s, ELEM_TYPE_STRING);
  ERROR_UNLESS_IS_TYPE(frame, p, ELEM_TYPE_INT);
  if ( p->ival.value < s->sval.len ) {
    return new_int(frame, s->sval.str[p->ival.value]);
  } else {
    return new_error(frame, "Position exceeds length of string");
  }
}

struct elem *string_size(struct elem *frame, struct elem *s) {
  ERROR_UNLESS_IS_TYPE(frame, s, ELEM_TYPE_STRING);
  return new_int(frame, s->sval.len);
}

struct elem *int_plus(struct elem *frame, struct elem *a, struct elem *b) {
  ERROR_UNLESS_IS_TYPE(frame, a, ELEM_TYPE_INT);
  ERROR_UNLESS_IS_TYPE(frame, b, ELEM_TYPE_INT);
  return new_int(frame, a->ival.value + b->ival.value);
}

struct elem* reader_next_char(struct elem *frame) {
  struct elem *pos = reader_get_pos(frame);
  struct elem *input = reader_get_input(frame);
  if ( is_true(int_leq(frame, pos, string_size(frame, input))) ) {
    pos = int_plus(frame, pos, new_int(frame, 1));
    frame = reader_set_pos(frame, pos);
    frame = reader_set_curr_char(frame, string_char_at(frame, input, pos));
  } else {
    frame = reader_set_error(frame, new_error(frame, "End of string encountered while reading"));
  }
  return frame;
}
                           

struct elem *string_read(struct elem *frame) {
  char buf[4096];
  int  tail = 0;
  int  len = sizeof(buf);
  frame = reader_next_char(frame);

  while( int_value(reader_get_curr_char(frame)) != '"' && tail < len ) {
    buf[tail++] = int_value(reader_get_curr_char(frame));
    frame = reader_next_char(frame);
  }
  buf[tail] = 0;
  frame = reader_next_char(frame);

  return reader_set_expr(frame, new_string(frame, buf));
}

int is_ident_char(int c) {
  if ( ( c > 'A' && c < 'Z' ) || ( c > 'a' && c < 'z' ) ) {
    return 1;
  }
  switch(c) {
  case '_':
  case '-':
  case '?':
  case '!':
    return 1;
  }
  return 0;
}

int is_sym_char(int c) {
  if ( ( c > 'A' && c < 'Z' ) || ( c > 'a' && c < 'z' ) ) {
    return 1;
  }
  switch(c) {
  case '_':
  case '-':
  case '?':
  case '!':
    return 1;
  }
  return 0;
}

struct elem *ident_read(struct elem *frame) {
  char buf[4096];
  int  tail = 0;
  int  len = sizeof(buf);
  while( is_ident_char(int_value(reader_get_curr_char(frame))) && tail < len ) {
    buf[tail++] = int_value(reader_get_curr_char(frame));
    frame = reader_next_char(frame);
  }
  buf[tail] = 0;

  return reader_set_expr(frame, new_ident(frame, buf));
}

struct elem *sym_read(struct elem *frame) {
  char buf[4096];
  int  tail = 0;
  int  len = sizeof(buf);
  frame = reader_next_char(frame);

  while( ! is_sym_char(int_value(reader_get_curr_char(frame))) && tail < len ) {
    buf[tail++] = int_value(reader_get_curr_char(frame));
    frame = reader_next_char(frame);
  }
  buf[tail] = 0;
  frame = reader_next_char(frame);

  return reader_set_expr(frame, new_sym(frame, buf));
}

struct elem *is_space(struct elem *frame, struct elem *c) {
  if ( int_value(c) == ' ' || int_value(c) == '\n' || int_value(c) == '\t' ) {
    return true_value();
  } else {
    return false_value();
  }
}

struct elem *reader_skip_whitespace(struct elem *frame) {
  while( is_true(is_space(frame, reader_get_curr_char(frame))) ) {
    frame = reader_next_char(frame);
  }
  return frame;
}

struct elem *map_read(struct elem *frame) {
  struct elem* map = empty_map();
  struct elem* key, *value;
  
  frame = reader_skip_whitespace(frame);

  while(! reader_has_error(frame) ) {
    switch(int_value(reader_get_curr_char(frame))) {
    case '}':
      frame = reader_set_expr(frame, map);
      return frame;
    default:
      frame = elem_read(frame);
      key = reader_get_expr(frame);
      frame = elem_read(frame);
      value = reader_get_expr(frame);
      map_set(frame, map, key, value);
      break;
    }
  }

  return frame;
}

struct elem *list_read(struct elem *frame) {
  struct elem* list = empty_list();
  struct elem *value;
  
  frame = reader_next_char(frame);
  frame = reader_skip_whitespace(frame);

  while(! reader_has_error(frame) ) {
    switch(int_value(reader_get_curr_char(frame))) {
    case ')':
      frame = reader_set_expr(frame, list_reverse(frame, list));
      return frame;
    default:
      frame = elem_read(frame);
      value = reader_get_expr(frame);
      list = list_add(frame, list, value);
      break;
    }
  }

  return frame;
}


struct elem *elem_read(struct elem *frame) {
  frame = reader_skip_whitespace(frame);
  int c = int_value(reader_get_curr_char(frame));

  switch(c) {
  case '{':
    return map_read(frame);
  case '(':
    return list_read(frame);
  case '"':
    return string_read(frame);
  case ':':
    return sym_read(frame);
  }

  if ( is_ident_char(c) ) {
    return ident_read(frame);
  }

  return reader_set_error(frame, new_error(frame, "Symbol not recognised."));
}

struct elem *reader_new_frame(struct elem *frame, char *expr) {
  struct elem* pos = new_int(frame, 0);
  struct elem* input = new_string(frame, expr);
  frame = reader_set_input(frame, input);
  frame = reader_set_pos(frame, pos);
  frame = reader_set_curr_char(frame, string_char_at(frame, input, pos));
  return frame;
}

void elem_print(struct elem *frame, FILE *out, struct elem *e) {
  switch(e->type) {
  case ELEM_TYPE_NIL:
    fprintf(out, "nil");
    break;
  case ELEM_TYPE_INT:
    fprintf(out, "%d", e->ival.value);
    break;
  case ELEM_TYPE_LIST:
    list_print(frame, out, e);
    break;
  case ELEM_TYPE_STRING:
    string_print(frame, out, e);
    break;
  case ELEM_TYPE_IDENT:
    ident_print(frame, out, e);
    break;
  case ELEM_TYPE_ERROR:
    error_print(frame, out, e);
    break;
  case ELEM_TYPE_SYM:
    sym_print(frame, out, e);
    break;
  case ELEM_TYPE_SET:
    set_print(frame, out, e);
    break;
  case ELEM_TYPE_MAP:
    map_print(frame, out, trim_map(frame, e));
    break;
  case ELEM_TYPE_FN:
    fn_print(frame, out, e);
    break;
  case ELEM_TYPE_ALLOC:
    fprintf(out, "<alloc:%p>", e->aval.alloc);
    break;
  default:
    abort(); // invalid type
  }
}

struct elem* builtin_println(struct elem *frame) {
  struct elem *rhs = frame_get(frame, sym_rhs());
  if ( is_list(rhs) ) {
    while( ! list_is_empty(rhs) ) {
      elem_print(frame, stdout, list_value(rhs));
      rhs = list_next(rhs);
    }
  } else {
    elem_print(frame, stdout, rhs);
  }
  fprintf(stdout, "\n");
  frame = frame_set(frame, sym_rhs(), nil());
  return frame;
}

struct elem* elem_println(struct elem *frame, FILE *out, struct elem *expr) {
  elem_print(frame, out, expr);
  fprintf(out, "\n");
  return frame;
}

struct elem *reader_read(struct elem *frame, char *expr) {
  frame = reader_new_frame(frame, expr);
  frame = elem_read(frame);
  if ( reader_has_error(frame) ) {
    return reader_get_error(frame);
  } else {
    return reader_get_expr(frame);
  }
}

int test_parsing(char *expr) {
  struct elem* root_frame = reader_new_frame(new_root_frame(), expr);
  struct elem* frame = root_frame;
  int status;
  frame = elem_read(frame);
  if ( reader_has_error(frame) ) {
    status = 1;
    elem_println(frame, stdout, reader_get_error(frame));
  } else {
    status = 0;
    elem_println(frame, stdout, reader_get_expr(frame));
  }
  free_root_frame(root_frame);
  return status;

}

int test_eval(char *expr) {
  struct elem *root_frame = new_root_frame();
  struct elem *reader_root = new_root_frame();
  struct elem *frame = root_frame;
  struct elem *env = empty_map();
  frame = frame_set(frame, sym_rhs(), reader_read(reader_root, expr));
  frame = frame_set(frame, sym_lhs(), empty_list());
  env = map_set(frame, env, sym_println(), new_fn(frame, builtin_println));
  frame = frame_set(frame, sym_env(), env);
  frame = frame_eval(frame);
  elem_println(frame, stdout, frame);
  free_root_frame(root_frame);
  free_root_frame(reader_root);
  return 0;
}

int test_eval_1() {
  return test_eval("\"hello\"");
}

int test_eval_2() {
  return test_eval("(println \"hello\")");
}

int test_reader_1() {
  return test_parsing("\"hello\"");
}

int test_reader_2() {
  return test_parsing("(\"hello\")");
}

int test_reader_3() {
  return test_parsing("(\"hello\" \"world\")");
}

int main(int argc, char **argv) {
  test_reader_1();
  test_reader_2();
  test_reader_3();

  test_eval_1();
  test_eval_2();
  return 0;
}
