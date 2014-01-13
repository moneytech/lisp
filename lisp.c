// This is a small experiment trying to write a lisp interpreter.
// The current implementation is not very correct. The next idea
// to include is having the stack represented by a list rather
// than relying on the C-stack
//
// Also of interest is that reverse is actually a primitive way to copy
// lists. They can't be easily copied directly without creating references
// to mutable objects
//
// There are some primitive evaluation forms
//   (quote (1 2)) -> (1 2)
//   (let (a 1) a) -> 
//   (fn (a) a)
//   (if (cond) (a) (b))
//
// Also of interest is propogating error conditions 
//
// Evaluation would proceed via a kind of copy to the stack
//
// (eval lhs rhs parent)
//   (eval () (+ (+ 1 2) 3) ())
//   (eval (#plus) ((+ 1 2) 3) ())
//   (eval () (+ 1 2) (eval (#plus) (3) ()))
//   (eval (#plus) (1 2) (eval (#plus) (3) ()))
//   (eval (1 #plus) (2) (eval (#plus) (3) ()))
//   (eval (1 2 #plus) () (eval (#plus) (3) ()))
//   (eval (3) () (eval (#plus) (3) ()))
//   (eval (3 #plus (3) ()))
//   (eval (3 3 #plus) () ())
//   6
//
// The event loop works by moving evaluations over from rhs to lhs
// Whenever complex expression are encountered a new stack frame
// is introdced with the old one linked in cont
//
// eval(lhs, rhs, cont)
//   loop:
//     if empty?(rhs) 
//       if empty?(cont)
//         return apply(lhs)
//       else
//         lhs <- append(apply(lhs), cont.lhs)
//         rhs <- cont.rhs
//         cont <- cont.cont
//     else
//       if primitive?(head(rhs)) then
//         lhs <- append(head(rhs), lhs)
//         rhs <- tail(rhs)
//         cont <- cont
//       else
//         cont <- (lhs, tail(rhs), cont)
//         lhs <- empty
//         rhs <- head(rhs)
//
// The environment needs to be woven through this
// structure so that it makes sense
//
// Applications may modify the environment

#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

#define ALLOC(x,y) calloc(x, y)
#define NEW(x) ((struct x *)ALLOC(sizeof(struct x), 1))
#define NEW_ARRAY(x, y) ((struct x *)ALLOC(sizeof(struct x), y))
#define DELETE(x) free(x)

struct InputStream {
  int c;
  FILE *in;
};

struct OutputStream {
  FILE *out;
};

struct SymbolTable {
  struct CArray *table;
  int length;
};

typedef struct Value* (*NativeFuncPtr)(struct Value *env, struct Value *args);

#define TYPE_NIL        0
#define TYPE_EMPTY_PAIR 1
#define TYPE_INT        2
#define TYPE_DOUBLE     3
#define TYPE_FLOAT      4
#define TYPE_SYMBOL     5
#define TYPE_PAIR       6
#define TYPE_STRING     7
#define TYPE_NATIVE     8
#define TYPE_ERROR      9

struct Value;

struct Pair {
  struct Value *head;
  struct Value *tail;
};

struct Value {
  int type;
  union {
    int    ival;
    double dval;
    float  fval;
    struct Pair pval;
    struct CArray *cval;
    NativeFuncPtr nval;
  } v;
};

struct CArray {
  char *data;
  int length;
};

// Proto

struct Value* eval_value(struct Value *env, struct Value *args);
struct Value* eval_list(struct Value *env, struct Value *args);
struct Value* lookup_value(struct Value *env, struct Value *value);

struct InputStream *input_stream_new(FILE *inp) {
  struct InputStream *in = NEW(InputStream);
  in->c = fgetc(inp);
  in->in = inp;
  return in;
};

struct OutputStream *output_stream_new(FILE *outp) {
  struct OutputStream *out = NEW(OutputStream);
  out->out = outp;
  return out;
};

struct CArray* carray_copy(struct CArray *carray) {
  struct CArray *result = NEW(CArray);
  result->length = carray->length;
  result->data   = ALLOC(result->length, 1);
  memcpy(result->data, carray->data, result->length);
  return result;
};

void carray_copy_into(struct CArray *src, struct CArray *dst) {
  if ( dst->data != 0 ) {
    DELETE(dst->data);
  }
  dst->length = src->length;
  dst->data   = ALLOC(dst->length, 1);
  memcpy(dst->data, src->data, dst->length);
};

struct CArray* carray_copy_length(struct CArray *carray, int length) {
  struct CArray *result = NEW(CArray);
  result->length = length;
  result->data   = ALLOC(result->length, 1);
  memcpy(result->data, carray->data, result->length);
  return result;
};

int peek(struct InputStream *in) {
  return in->c;
}

int next(struct InputStream *in) {
  int result = in->c;
  in->c = fgetc(in->in);
  return result;
}

struct CArray* carray_new(int length) {
  struct CArray *result = NEW(CArray);
  result->length = length;
  result->data   = ALLOC(result->length, 1);
  return result;
}

struct CArray* carray_from_cstr(char *str) {
  struct CArray *result = NEW(CArray);
  result->length = strlen(str);
  result->data   = ALLOC(result->length, 1);
  memcpy(result->data, str, result->length);
  return result;
}

void carray_delete(struct CArray *carray) {
  DELETE(carray->data);
  DELETE(carray);
}

int carray_hash(struct CArray *carray) {
  int i, r=0;
  for(i=0;i<carray->length;++i) {
    r ^= carray->data[i];
  }
  return r;
}

int carray_eq(struct CArray *a, struct CArray *b) {
  if ( a->length != b->length ) {
    return 0;
  }
  return memcmp(a->data, b->data, a->length) == 0;
}

struct SymbolTable* st_new(int length) {
  struct SymbolTable *table = NEW(SymbolTable);
  table->length = length;
  table->table  = NEW_ARRAY(CArray, length);
  return table;
}

int st_get(struct SymbolTable *table, struct CArray* carray) {
  int index = carray_hash(carray) % table->length;
  while( 
    table->table[index].length != 0 && 
    ! carray_eq(carray, table->table + index) 
  ) {
    index = (index + 1) % table->length;
  }
  if ( table->table[index].length == 0 ) {
    carray_copy_into(carray, table->table + index);
  }
  return index;
}

struct CArray *st_get_carray(struct SymbolTable *table, int index) {
  return &table->table[index];
}

struct Value* value_new(int type) {
  struct Value *v = NEW(Value);
  v->type = type;
  return v;
}

struct Value* value_nil_new() {
  return value_new(TYPE_NIL);
}

struct Value* value_empty_pair_new() {
  return value_new(TYPE_EMPTY_PAIR);
}

struct Value *NIL;
struct Value *EMPTY_PAIR;

struct Value* value_error_new() {
  struct Value *v = value_new(TYPE_ERROR);
  return v;
}

struct Value *value_pair_new(struct Value *head, struct Value *tail) {
  struct Value *v = value_new(TYPE_PAIR);
  v->v.pval.head = head;
  v->v.pval.tail = tail;
  return v;
}

int value_is_type(struct Value *value, int type) {
  return value->type == type;
}

struct Value* assoc_value(
  struct Value *env, 
  struct Value *symbol, 
  struct Value *value
) {
  return value_pair_new(value_pair_new(symbol, value), env);
}

struct Value *env_new() {
  return EMPTY_PAIR;
}

int value_eq(struct Value *a, struct Value *b) {
  if ( a == b ) {
    return 1;
  } else if ( a->type != b->type ) {
    return 0;
  } else if ( a->type == TYPE_SYMBOL ) {
    return a->v.ival == b->v.ival;
  } else {
    return a == b;
  }
}

struct Value* value_pair_head(struct Value *pair) {
  if ( value_is_type(pair, TYPE_PAIR) ) {
    return pair->v.pval.head;
  } else {
    return NIL;
  }
}

struct Value* value_pair_tail(struct Value *pair) {
  if ( value_is_type(pair, TYPE_PAIR) ) {
    return pair->v.pval.tail;
  } else {
    return EMPTY_PAIR;
  }
}

struct Value* rev(struct Value *env, struct Value *lst) {
  struct Value *res = EMPTY_PAIR;
  while(lst != EMPTY_PAIR) {
    res = value_pair_new(value_pair_head(lst), res);
    lst = value_pair_tail(lst);
  }
  return res;
}

struct Value* lookup_value(
  struct Value *env,
  struct Value *value
) {
  if ( value_is_type(value, TYPE_SYMBOL) ) {
    if ( env == EMPTY_PAIR ) {
      return NIL;
    } else {
      struct Value *h = value_pair_head(env);
      if ( value_eq(value_pair_head(h), value) ) {
        return value_pair_tail(h);
      } else {
        return lookup_value(value_pair_tail(env), value);
      }
    }
  } else {
    return value;
  }
}

struct Value* value_native_new(NativeFuncPtr nval) {
  struct Value *result = value_new(TYPE_NATIVE);
  result->v.nval = nval;
  return result;
}

int is_alpha(int c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

int is_white_space(int c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

int is_numeric(int c) {
  return (c >= '0' && c <= '9');
}

int is_alnum(int c) {
  return is_alpha(c) || is_numeric(c);
}

struct Value* parse_value(struct InputStream *in, struct SymbolTable *table);

struct Value* value_list_prepend(struct Value *head, struct Value *tail) {
  struct Value *list = value_new(TYPE_PAIR);
  list->v.pval.head = head;
  list->v.pval.tail = tail;
  return list;
}

struct Value* parse_list(struct InputStream *in, struct SymbolTable *table) {
  struct Value *v = parse_value(in, table);
  if ( v != EMPTY_PAIR ) {
    return value_list_prepend(v, parse_list(in, table));
  } else {
    return EMPTY_PAIR;
  }
}

struct Value* parse_symbol(struct InputStream *in, struct SymbolTable *table, int c) {
  struct CArray *buf = carray_new(1024);
  struct Value *v = value_new(TYPE_SYMBOL);
  int i =0;

  buf->data[i++] = c;
  while( is_alnum(peek(in)) && i < buf->length) {
    buf->data[i++] = next(in);
  }
  buf->length = i;

  v->v.ival = st_get(table, buf);

  carray_delete(buf);
  return v;
}

struct Value* parse_string(struct InputStream *in, struct SymbolTable *table) {
  struct CArray *buf = carray_new(1024);
  struct Value *v = value_new(TYPE_STRING);
  int i = 0;

  while(i < buf->length-1 && peek(in) != '"' ) {
    if ( peek(in) == '\\' ) {
      next(in);
    }
    buf->data[i++] = next(in);
  }
  next(in);
  v->v.cval = carray_copy_length(buf, i);

  carray_delete(buf);
  return v;
}

struct Value* parse_numeric(struct InputStream *in, struct SymbolTable *table, int c) {
  struct Value *v = value_new(TYPE_INT);
  v->v.ival = c - '0';

  while(is_alpha(peek(in))) {
    v->v.ival *= 10;
    v->v.ival += next(in) - '0';
  }

  return v;
}

struct Value* parse_value(struct InputStream *in, struct SymbolTable *table) {
  int c = next(in);
  struct Value *v = 0;

  while( is_white_space(c) ) {
    c = next(in);
  }
  if ( c == '(' ) {
    v = parse_list(in, table);
  } else if ( c == ')' ) {
    v = EMPTY_PAIR;
  } else if ( is_alpha(c) ) {
    v = parse_symbol(in, table, c);
  } else if ( c == '"' ) {
    v = parse_string(in, table);
  } else if ( is_numeric(c) ) {
    v = parse_numeric(in, table, c);
  } 
  return v;
}

void print_char(struct OutputStream *out, int c) {
  fputc(c, out->out);
}

void print_cstr(struct OutputStream *out, char *cstr) {
  while( *cstr ) {
    fputc(*cstr, out->out);
    cstr++;
  }
}

void print_carray(struct OutputStream *out, struct CArray *carray) {
  int i = 0;
  for(i=0;i<carray->length;++i) {
    if ( carray->data[i] == '\\' || carray->data[i] == '"' ) {
      print_char(out, '\\');
    }
    print_char(out, carray->data[i]);
  }
}

void print_space(struct OutputStream *out) {
  print_char(out, ' ');
}

void print_integer(struct OutputStream *out, int v) {
  fprintf(out->out, "%d", v);
}

void print_value(struct OutputStream *out, struct SymbolTable *st, struct Value *v) {
  if ( v->type == TYPE_SYMBOL ) {
    print_carray(out, st_get_carray(st, v->v.ival));
  } else if ( v->type == TYPE_INT ) {
    print_integer(out, v->v.ival);
  } else if ( v->type == TYPE_STRING ) {
    print_char(out, '"');
    print_carray(out, v->v.cval);
    print_char(out, '"');
  } else if ( v->type == TYPE_NATIVE ) {
    print_cstr(out, "#native");
  } else if ( v->type == TYPE_PAIR || v == EMPTY_PAIR ) {
    print_char(out, '(');
    struct Value *list = v;
    while(list != EMPTY_PAIR) {
      print_value(out, st, value_pair_head(list));
      list = value_pair_tail(list);
      if ( list != EMPTY_PAIR ) {
        print_char(out, ' ');
      }
    }
    print_char(out, ')');
  } else if ( v == NIL ) {
    print_cstr(out, "nil");
  }
}

struct Value* native_head(struct Value *env, struct Value *args) {
  return value_pair_head(value_pair_head(eval_list(env, args)));
}

struct Value* native_tail(struct Value *env, struct Value *args) {
  return value_pair_tail(value_pair_head(eval_list(env, args)));
}

struct Value *assoc_list(struct Value *env, struct Value *val) {
  if ( val == EMPTY_PAIR ) {
    return env;
  } else {
    env = assoc_value(env, value_pair_head(val), value_pair_head(value_pair_tail(val)));
    return assoc_list(env, value_pair_tail(value_pair_tail(val)));
  }
}

struct Value* native_let(struct Value *env, struct Value *args) {
  env = assoc_list(env, value_pair_head(args));
  return eval_list(env, value_pair_tail(args));
}

struct Value* native_quote(struct Value *env, struct Value *args) {
  return value_pair_head(args);
}

NativeFuncPtr value_native_get_func(struct Value *func) {
  return func->v.nval;
}

struct Value* value_apply_func(struct Value *env, struct Value *func, struct Value *args) {
  return (*value_native_get_func(func))(env, args);
}

struct Value* eval_list(struct Value *env, struct Value *args) {
  if ( args == EMPTY_PAIR ) {
    return args;
  } else {
    return value_pair_new(eval_value(env, value_pair_head(args)), eval_list(env, value_pair_tail(args)));
  }
}

struct Value* eval_value(struct Value *env, struct Value *value) {
  if ( value_is_type(value, TYPE_PAIR) ) {
    struct Value *h = value_pair_head(value);
    struct Value *t = value_pair_tail(value);
    if ( value_is_type(h, TYPE_SYMBOL) ) {
      h = lookup_value(env, h);
    }
    if ( value_is_type(h, TYPE_NATIVE) ) {
      return value_apply_func(env, h, t);
    } else {
      return NIL;
    }
  } else if ( value_is_type(value, TYPE_SYMBOL) ) {
    return lookup_value(env, value);
  }
  else {
    return value;
  }
}

struct Value* sym(struct SymbolTable *st, char *cstr) {
  struct Value* result = value_new(TYPE_SYMBOL);
  result->v.ival = st_get(st, carray_from_cstr(cstr));
  return result;
}

void init() {
  NIL = value_nil_new();
  EMPTY_PAIR = value_empty_pair_new();
}

struct Value *assoc(struct Value *env, struct Value *key, struct Value *value) {
  return value_pair_new(value_pair_new(key, value), env);
}

struct Value *assoc_str(struct Value *env, struct SymbolTable *st, char* cstr, struct Value *val) {
  return assoc(env, sym(st, cstr), val);
}

struct Value *assoc_native(struct Value *env, struct SymbolTable *st, char* cstr, NativeFuncPtr func) {
  return assoc(env, sym(st, cstr), value_native_new(func));
}

int main(int argc, char **argv) {
  init();

  struct InputStream  *in  = input_stream_new(stdin);
  struct OutputStream *out = output_stream_new(stdout);
  struct SymbolTable  *st  = st_new(1024);
  struct Value        *env = EMPTY_PAIR;

  env = assoc_str(env, st, "nil", NIL);
  env = assoc_native(env, st, "head", &native_head);
  env = assoc_native(env, st, "tail", &native_tail);
  env = assoc_native(env, st, "quote",  &native_quote);
  env = assoc_native(env, st, "let",  &native_let);

  struct Value *value = parse_value(in, st);
  print_value(out, st, value);
  print_char(out, '\n');
  print_value(out, st, eval_value(env, value));
  print_char(out, '\n');
  return 0;
}


