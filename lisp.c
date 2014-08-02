#include "lisp.h"

#include <stdlib.h> // calloc
#include <string.h> // memset
#include <stdio.h>  // fprintf

#define NEW(t) (t *)calloc(sizeof(t), 1)
#define NEW_ARRAY(t, l) (t *)calloc(sizeof(t), l)

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
    .sval.len   = sizeof(#text)-1,                \
    .sval.str   = (uint8_t *)#text                \
  };                                              \
  struct elem *sym_##text() {                     \
    return &name;                                 \
  }                                               \

DEFINE_SYM(SYM_ALLOC,  alloc)
DEFINE_SYM(SYM_ENV,    env)
DEFINE_SYM(SYM_RHS,    rhs)
DEFINE_SYM(SYM_LHS,    lhs)
DEFINE_SYM(SYM_PARENT, parent)

struct elem NIL        = { 
  .type = ELEM_TYPE_NIL,
  .lval.value = 0,
  .lval.next = 0
};

struct elem *map_get(struct elem *frame, struct elem *m, struct elem *k);
struct elem *map_set(struct elem *frame, struct elem *m, struct elem *k, struct elem *v);

struct elem *nil() {
  return &NIL;
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

struct elem *new_string(struct elem *frame, char *s) {
  struct elem *ret = frame_alloc_elem(frame);
  ret->type = ELEM_TYPE_STRING;
  ret->sval.len = strlen(s) + 1;
  ret->sval.str = NEW_ARRAY(uint8_t, ret->sval.len+1);
  memcpy(ret->sval.str, s, ret->sval.len);
  return ret;
}

struct elem *new_error(struct elem *frame, char *s) {
  struct elem *ret = frame_alloc_elem(frame);
  ret->type = ELEM_TYPE_ERROR;
  ret->sval.len = strlen(s) + 1;
  ret->sval.str = NEW_ARRAY(uint8_t, ret->sval.len+1);
  memcpy(ret->sval.str, s, ret->sval.len);
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

int elem_is_type(struct elem *e, int type) {
  return e->type == type;
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
  case ELEM_TYPE_STRING:
    return sval_eq(frame, a, b);
  case ELEM_TYPE_ERROR:
    return 0;
  case ELEM_TYPE_SYM:
    return sval_eq(frame, a, b);
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

struct elem* new_child_frame(struct elem* frame, struct elem *e) {
  struct elem* nf = empty_map();
  nf = map_set(frame, nf, sym_parent(), frame);
  nf = map_set(frame, nf, sym_env(), empty_map());
  nf = map_set(frame, nf, sym_lhs(), empty_list());
  nf = map_set(frame, nf, sym_rhs(), e);
  nf = map_set(frame, nf, sym_alloc(), frame_get(frame, sym_alloc()));
  return nf;
}

struct elem *frame_eval_step(struct elem *frame) 
{
  struct elem *lhs, *rhs, *value;

  rhs = frame_get(frame, sym_rhs());
  if ( ! is_list(rhs) ) {
    frame = frame_set(frame, sym_lhs(), rhs);
    frame = frame_set(frame, sym_rhs(), empty_list());
    return frame;
  }

  if ( list_is_empty(rhs) ) {
    return frame;
  }

  lhs = frame_get(frame, sym_lhs());
  value = list_value(rhs);
  rhs = list_next(rhs);

  if ( is_list(value) ) {
    frame = frame_set(frame, sym_rhs(), rhs); 
    return new_child_frame(frame, value);
  }

  if ( is_sym(value) ) {
    value = env_get(frame, value);
  }

  lhs = list_add(frame, lhs, value);
  frame = frame_set(frame, sym_lhs(), lhs);
  frame = frame_set(frame, sym_rhs(), rhs);

  return frame;
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

void error_print(struct elem *frame, FILE *out, struct elem *s) {
  fprintf(out, "<err:");
  sval_print(frame, out, s);
  fprintf(out, ">");
}

void sym_print(struct elem *frame, FILE *out, struct elem *s) {
  fprintf(out, ":");
  sval_print(frame, out, s);
}

void fn_print(struct elem *frame, FILE *out, struct elem *e) {
  fprintf(out, "<fn:%p>", e->fval.fn);
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

int main(int argc, char **argv) {
  struct elem *frame = new_root_frame();
  struct elem *e = empty_list();
  e = list_add(frame, e, new_string(frame, "hello"));
  e = list_add(frame, e, new_string(frame, "world"));
  frame = frame_set(frame, sym_rhs(), e);
  frame = frame_set(frame, sym_lhs(), empty_list());
  frame = frame_set(frame, sym_env(), empty_map());
  frame = frame_eval_step(frame);
  frame = frame_eval_step(frame);
  elem_print(frame, stdout, frame);
  fprintf(stdout, "\n");
  
  return 0;
}
