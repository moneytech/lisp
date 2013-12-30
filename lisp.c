#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

#define ALLOC(x y) calloc(x, y)
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

typedef struct Value* (*NativeFunc)(struct Value *env, struct Value *args);

#define TYPE_NIL    0
#define TYPE_INT    1
#define TYPE_DOUBLE 2
#define TYPE_FLOAT  3
#define TYPE_SYMBOL 4
#define TYPE_PAIR   5
#define TYPE_STRING 6
#define TYPE_NATIVE 7
#define TYPE_ERROR  8

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
    NativeFunc nval;
  } v;
};

struct CArray {
  char *data;
  int length;
};

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

struct SymbolTable* symbol_table_new(int length) {
  struct SymbolTable *table = NEW(SymbolTable);
  table->length = length;
  table->table  = NEW_ARRAY(CArray, length);
  return table;
}

int symbol_table_get(struct SymbolTable *table, struct CArray* carray) {
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

struct CArray *symbol_table_get_carray(struct SymbolTable *table, int index) {
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

struct Value *NIL = value_nil_new();

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

struct Value* env_put(
  struct Value *env, 
  struct Value *symbol, 
  struct Value *value
) {
  return value_pair_new(value_pair_new(symbol, value), envDDD);
}

struct Value *env_new() {
  return NIL;
}

int value_eq(Value *a, Value *b) {
  if ( a == b ) {
    return 1;
  } else if ( a->type != b->type ) {
    return 0;
  } else if ( a->type == TYPE_SYM ) {
    return a->v.ival == b->v.ival;
  } else if ( a->type == TYPE_NIL ) {
    return 1;
  } else {
    return a == b;
  }
}

struct Value* value_pair_head(struct Value *pair) {
  if ( value_is_type(pair, TYPE_PAIR) ) {
    return value->v.pval.head;
  } else {
    return NIL;
  }
}

struct Value* value_pair_tail(struct Value *pair) {
  if ( value_is_type(pair, TYPE_PAIR) ) {
    return value->v.pval.tail;
  } else {
    return NIL;
  }
}

struct Value* env_get(
  struct Value *table,
  struct Value *symbol
) {
  if ( value_eq(value_pair_head(table), symbol) ) {
    return value_pair_tail(table);
  } else {
    return env_get(value_pair_tail(table), symbol);
  }
}

struct Value* value_native_new(NativeFunc nval) {
  struct Value *result = new_value(TYPE_NATIVE);
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

struct Value* parse_list(struct InputStream *in, struct SymbolTable *table) {
  struct Value *v = parse_value(in, table);
  if ( v != nil ) {
    return value_list_prepend(v, parse_list(in, table));
  } else {
    return NIL;
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

  v->v.ival = symbol_table_get(table, buf);

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

void print_value(struct OutputStream *out, struct SymbolTable *symbol_table, struct Value *v) {
  if ( v->type == TYPE_SYMBOL ) {
    print_carray(out, symbol_table_get_carray(symbol_table, v->v.ival));
  } else if ( v->type == TYPE_INT ) {
    print_integer(out, v->v.ival);
  } else if ( v->type == TYPE_STRING ) {
    print_char(out, '"');
    print_carray(out, v->v.cval);
    print_char(out, '"');
  } else if ( v->type == TYPE_LIST ) {
    print_char(out, '(');
    struct List *l = v->v.lval;
    while(l != 0) {
      print_value(out, symbol_table, l->value);
      if ( l->next != 0 ) {
        print_space(out);
      }
      l = l->next;
    }
    print_char(out, ')');
  }
}

struct Value* native_head(struct Value *env, Value *args) {
  return value_pair_head(value_pair_head(args));
}

struct Value* native_tail(struct Value *env, Value *args) {
  return value_pair_tail(value_pair_head(args));
}

struct Value*

struct Value* native_eval(struct Value *env, struct Value *value) {
  if ( value_is_type(TYPE_PAIR) ) {
    Value *h = value_head(value);
    Value *t = value_tail(value);
    if ( value_is_type(TYPE_SYM) ) {
      h = env_get(env, h);
    }
    if ( value_is_type(TYPE_NATIVE_FUNC) ) {
      return value_apply_func(env, h, lookup_list(env, t));
    }
  }
  else {
    return value;
  }
  return value;
}

struct Value* value_from_cstr(struct SymbolTable *symbol_table, char *cstr) {
  struct Value* result = new_value(TOKEN_SYMBOL);
  result->v.ival = symbol_table_get(symbol_table, carray_from_cstr(cstr));
  return result;
}

struct Value* sym(SymbolTable *symbol_table, char *str) {
  return symbol_table_get(symbol_table, carray_from_cstr(str));
}

int main(int argc, char **argv) {
  struct InputStream *in = input_stream_new(stdin);
  struct OutputStream *out = output_stream_new(stdout);
  struct SymbolTable *symbol_table = symbol_table_new(1024);
  struct Env *env = env_new(256);
  env->nil = value_from_cstr(symbol_table, "nil");
  env_put(env, sym(symbol_table, "head"), value_new_native(&head));
  env_put(env, sym(symbol_table, "tail"), value_new_native(&tail));
  struct Value *value = parse_value(in, symbol_table);
  print_value(out, symbol_table, eval(value, symbol_table, env));
  return 0;
}


