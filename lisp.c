#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

#define TYPE_INT    0
#define TYPE_DOUBLE 1
#define TYPE_FLOAT  2
#define TYPE_SYMBOL 3
#define TYPE_LIST   4
#define TYPE_STRING 5
#define TYPE_NATIVE 6

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

typedef struct Value* (*native_func_t)(struct Value *value, struct SymbolTable *t, struct Env *env);

struct Value {
  int type;
  union {
    int    ival;
    double dval;
    float  fval;
    struct List *lval;
    struct CArray *cval;
    native_func_t nval;
  } v;
};

struct CArray {
  char *data;
  int length;
};

struct List {
  struct Value *value;
  struct List  *next;
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

struct EnvPair {
  int symbol;
  struct Value value;
};

struct Env {
  struct EnvPair *table;
  int length;
  struct Value* nil;
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

struct Env *new_env(int length) {
  struct Env *table = NEW(Env);
  table->length = length;
  table->table  = NEW_ARRAY(EnvPair, length);
  return table;
}

void env_put(
  struct Env *table, 
  int symbol, 
  struct Value *value
) {
  int index = symbol % table->length;
  while( 
    table->table[index].symbol != 0 && 
    table->table[index].symbol != symbol
  ) {
    index = (index + 1) % table->length;
  }
  table->table[index].symbol = symbol;
  table->table[index].value  = *value;
}

struct Value* env_get(
  struct Env *table, 
  int symbol
) {
  int index = symbol % table->length;
  while( 
    table->table[index].symbol != 0 && 
    table->table[index].symbol != symbol
  ) {
    index = (index + 1) % table->length;
  }
  if ( table->table[index].symbol != 0 ) {
    return &table->table[index].value;
  } else {
    return 0;
  }
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

struct List* list_new() {
  return 0;
}

struct List* list_append(struct List *l, struct Value *v) {
  if ( l == 0 ) {
    l = NEW(List);
    l->value = v;
  } else {
    while(l->next != 0) {
      l = l->next;
    }
    l->next = NEW(List);
    l->next->value = v;
  }
  return l;
}

struct Value* value_new(int type) {
  struct Value *v = NEW(Value);
  v->type = type;
  return v;
}

struct Value* parse_value(struct InputStream *in, struct SymbolTable *table);

struct Value* parse_list(struct InputStream *in, struct SymbolTable *table) {
  struct List *l = list_new();
  struct Value *v = parse_value(in, table);
  while( v != 0 ) {
    l = list_append(l, v);
    v = parse_value(in, table);
  }
  v = value_new(TYPE_LIST);
  v->v.lval = l;
  return v;
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


int value_is_non_empty_list(struct Value *value) {
  return value->type == TYPE_LIST && value->v.ival != 0;
}

struct Value* value_nil(struct Env *env) {
  return env->nil;
}

struct Value* value_new_list(struct List *list) {
  struct Value *result = new_value(TYPE_LIST);
  result->v.lval = list;
  return result;
}

struct Value* value_new_native(native_func nval) {
  struct Value *result = new_value(TYPE_NATIVE);
  result->v.nval = nval;
  return result;
}

struct Value* head(Value *value, struct SymbolTable *t, struct Env *env) {
  if ( value_is_non_empty_list(value) ) {
    return value->v.ival->value;
  } else {
    return value_nil(env);
  }
}

struct Value* tail(Value *value, struct SymbolTable *t, struct Env *env) {
  if ( value_is_non_empty_list(value) ) {
    return value_new_list(value->v.ival->next);
  } else {
    return value_nil(env);
  }
}

struct Value* list(Value *value, SymbolTable *t, Env *env) {
  Value result = value_new(TYPE_LIST);
  result->v.ival = value->v.ival->next;
  return result;
}

struct Value* eval(struct Value *value, struct SymbolTable *t, struct Env *env) {
  if ( value_is_non_empty_list(value) ) {
    
    Value *head_value = head(value, t, env);
    if ( value_is_symbol(head_value) ) {
      env_get(env, head);
    return tail(value);
  } else {
    return value;
  }
}

struct Value* value_from_cstr(struct SymbolTable *symbol_table, char *cstr) {
  struct Value* result = new_value(TOKEN_SYMBOL);
  result->v.ival = symbol_table_get(symbol_table, carray_from_cstr(cstr));
  return result;
}

int main(int argc, char **argv) {
  struct InputStream *in = input_stream_new(stdin);
  struct OutputStream *out = output_stream_new(stdout);
  struct SymbolTable *symbol_table = symbol_table_new(1024);
  struct Env *env = env_new(256);
  env->nil = value_from_cstr(symbol_table, "nil");
  env_put(env, symbol_table_get(symbol_table, carray_from_cstr("list")), value_new_native(&list));
  env_put(env, symbol_table_get(symbol_table, carray_from_cstr("head")), value_new_native(&head));
  env_put(env, symbol_table_get(symbol_table, carray_from_cstr("tail")), value_new_native(&tail));
  struct Value *value = parse_value(in, symbol_table);
  print_value(out, symbol_table, eval(value, symbol_table, env));
  return 0;
}


