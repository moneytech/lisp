#ifndef LISP_H
#define LIST_H

#include <stdint.h>

#define ELEM_TYPE_NIL        0
#define ELEM_TYPE_TRUE       1
#define ELEM_TYPE_FALSE      2

#define ELEM_TYPE_INT        3
typedef struct elem_int {
  uint32_t value;
} elem_int_t;

#define ELEM_TYPE_LIST       4
#define ELEM_TYPE_SET        5
struct elem_list {
  struct elem *value;
  struct elem *next;
};

#define ELEM_TYPE_STRING     6
#define ELEM_TYPE_SYM        7
struct elem_string {
  uint32_t len;
  uint8_t *str;
};

#define ELEM_TYPE_ERROR      8
struct elem_error {
  struct elem *map;
};

#define ELEM_TYPE_MAP        9
struct elem_map {
  struct elem *key;
  struct elem *value;
  struct elem *next;
};

#define ELEM_TYPE_FN         10
typedef struct elem *(fn)(struct elem *frame);

struct elem_fn {
  fn          *fn;
  struct elem *args;
  struct elem *expr;
};

#define ELEM_TYPE_ALLOC      11
struct alloc {
  uint32_t len;
  uint32_t tail;
  struct elem *table;
  struct elem *free_list;
};

struct elem_alloc {
  struct alloc* alloc;
};

struct elem {
  uint32_t        type;
  union {
    struct elem_list   lval;
    struct elem_int    ival;
    struct elem_string sval;
    struct elem_map    mval;
    struct elem_fn     fval;
    struct elem_alloc  aval;
    struct elem_error  eval;
  };
};

extern struct elem EMPTY_LIST;
extern struct elem NIL;

struct elem_cxt *new_elem_cxt();

struct elem* lisp_read(struct elem *frame, struct elem *env, struct elem *expr);
struct elem* lisp_write(struct elem *frame, struct elem *env, struct elem *expr);
struct elem* lisp_eval(struct elem *frame, struct elem *env, struct elem *expr);

int list_eq(struct elem *frame, struct elem *a, struct elem *b);
int set_eq(struct elem *frame, struct elem *a, struct elem *b);
int map_eq(struct elem *frame, struct elem *a, struct elem *b);
int elem_eq(struct elem *frame, struct elem *a, struct elem *b);
int sval_eq(struct elem *frame, struct elem *a, struct elem *b);
int ival_eq(struct elem *frame, struct elem *a, struct elem *b);

int list_sublist_eq(struct elem *frame, struct elem *a, struct elem *b);
int set_subset_eq(struct elem *frame, struct elem *a, struct elem *b);
int map_submap_eq(struct elem *frame, struct elem *a, struct elem *b);

#endif
