// base.c

#include <mruby.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>

#include "mruby/class.h"
#include "mruby/data.h"
#include "mruby/value.h"
#include "mruby/array.h"
#include "mruby/hash.h"
#include "mruby/string.h"
#include "mruby/variable.h"

#define MAJOR_V 4
#define MINOR_V 8

#define MAX_RECURSE 10

typedef struct
{
  uint8_t *str;
  int len;
} sym_stc;

typedef struct
{
  sym_stc *symbols;
  int n_symbols;
  void *data;
  int len,cur_p;
  mrb_value cur_v;
} bfr_stc;

static mrb_value marshal_load(mrb_state *mrb,mrb_value self);
static mrb_value load_marshal_recurse(mrb_state *mrb,bfr_stc *b);
static int64_t read_integer(mrb_state *mrb,bfr_stc *b);
static uint8_t *read_byte_seq(mrb_state *mrb,bfr_stc *b,int *len);

static mrb_value marshal_dump(mrb_state *mrb,mrb_value self);
static void b_append(bfr_stc *b,void *data,int len);
static void write_marshal_recurse(mrb_state *mrb,bfr_stc *b,mrb_value v,int lvl);
static void write_integer(mrb_state *mrb,bfr_stc *b,mrb_int i);
static void write_byte_seq(mrb_state *mrb,bfr_stc *b,uint8_t *s,int len);
static void write_symbol(mrb_state *mrb,bfr_stc *b,uint8_t *s,int len);

void  mrb_mruby_marshal_c_gem_init(mrb_state* mrb)
{
  struct RClass *m=mrb_define_module(mrb,"Marshal");

  mrb_define_module_function(mrb,m,"load",&marshal_load,MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb,m,"restore",&marshal_load,MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb,m,"dump",&marshal_dump,MRB_ARGS_REQ(1));
  mrb_define_const(mrb,m,"MAJOR_VERSION",mrb_fixnum_value(MAJOR_V));
  mrb_define_const(mrb,m,"MINOR_VERSION",mrb_fixnum_value(MINOR_V));
}

void mrb_mruby_marshal_c_gem_final(mrb_state* mrb)
{
}

/*********************************************
 *                   LOAD                    *
 *********************************************/

static mrb_value marshal_load(mrb_state *mrb,mrb_value self)
{
  mrb_value s;
  
  mrb_get_args(mrb,"S",&s);

  bfr_stc b;  
  uint8_t v,vers[2];

  bzero(&b,sizeof(bfr_stc));

  b.len=RSTRING_LEN(s);
  b.data=malloc(b.len);
  memcpy(b.data,RSTRING_PTR(s),b.len);
  
  vers[0]=((uint8_t *)b.data)[0];
  vers[1]=((uint8_t *)b.data)[1];
  
  if(vers[0]!=MAJOR_V || vers[1]!=MINOR_V)
    mrb_raisef(mrb,E_TYPE_ERROR,"%S: Bad version (%S.%S instead of %S.%S)!",mrb_str_new_cstr(mrb,__func__),
	       mrb_fixnum_value(vers[0]),mrb_fixnum_value(vers[1]),
	       mrb_fixnum_value(MAJOR_V),mrb_fixnum_value(MINOR_V));

  b.cur_p=2;

  mrb_value to_ret=load_marshal_recurse(mrb,&b);
  int i;

  for(i=0;i<b.n_symbols;i++)
    free(b.symbols[i].str);
  free(b.symbols);
  free(b.data);  
  
  return to_ret;
}

static mrb_value load_marshal_recurse(mrb_state *mrb,bfr_stc *b)
{
  if(b->cur_p>=b->len)
    mrb_raisef(mrb,E_TYPE_ERROR,"%S: String overflow!",mrb_str_new_cstr(mrb,__func__));

  uint8_t t=((uint8_t *)b->data)[b->cur_p++];
  mrb_value to_ret;
  int i;
  
  switch(t)
  {    
  case '0':
    return mrb_nil_value();
  case 'F':
    return mrb_false_value();
  case 'T':
    return mrb_true_value();    
  case 'c':
  {
    int len;    
    uint8_t *ptr=read_byte_seq(mrb,b,&len),bfr[len+1];
    memcpy(bfr,ptr,len);
    bfr[len]=0;

    to_ret=mrb_obj_value(mrb_class_get(mrb,(char *)bfr));
  }  
  return to_ret;  
  case 'm':
  {
    int len;    
    uint8_t *ptr=read_byte_seq(mrb,b,&len),bfr[len+1];
    memcpy(bfr,ptr,len);
    bfr[len]=0;

    to_ret=mrb_obj_value(mrb_module_get(mrb,(char *)bfr));
  }  
  return to_ret;  
  case 'i':
    return mrb_fixnum_value(read_integer(mrb,b));    
  case 'f':    
  {
    int len=read_integer(mrb,b);
    char bfr[len+1];

    memcpy(bfr,b->data+b->cur_p,len);
    bfr[len]=0;

    double v;

    sscanf(bfr,"%lf",&v);
    to_ret=mrb_float_value(mrb,v);
    b->cur_p+=len;
  }
  return to_ret;  
  case '"':
  {
    int len;    
    uint8_t *ptr=read_byte_seq(mrb,b,&len);
    to_ret=mrb_str_new(mrb,(char *)ptr,len);
  }
  return to_ret;
  case ':':
  {
    int len;    
    uint8_t *ptr=read_byte_seq(mrb,b,&len);

    b->symbols=realloc(b->symbols,sizeof(sym_stc)*(b->n_symbols+1));
    b->symbols[b->n_symbols].len=len;
    b->symbols[b->n_symbols].str=malloc(len);
    memcpy(b->symbols[b->n_symbols].str,ptr,len);
    b->n_symbols++;

    to_ret=mrb_symbol_value(mrb_intern_str(mrb,mrb_str_new(mrb,(char *)ptr,len)));
  }
  return to_ret;
  case ';':
  {
    int prog=read_integer(mrb,b);
    to_ret=mrb_symbol_value(mrb_intern_str(mrb,mrb_str_new(mrb,(char *)b->symbols[prog].str,b->symbols[prog].len)));
  }
  return to_ret;
  case '[':
  {
    int len=read_integer(mrb,b);
    to_ret=mrb_ary_new_capa(mrb,len);

    for(i=0;i<len;i++)
      mrb_ary_set(mrb,to_ret,i,load_marshal_recurse(mrb,b));

  }
  return to_ret;
  case '{':
  {
    int len=read_integer(mrb,b);
    to_ret=mrb_hash_new(mrb);
    mrb_value key,value;

    for(i=0;i<len;i++)
    {
      key=load_marshal_recurse(mrb,b);
      value=load_marshal_recurse(mrb,b);
      mrb_hash_set(mrb,to_ret,key,value);
    }
  }
  return to_ret;
  case 'o':
  {
    mrb_value cls_name=load_marshal_recurse(mrb,b),ivname,iv;
    mrb_int symlen;
    uint8_t *symptr=mrb_sym2name_len(mrb,mrb_symbol(cls_name),&symlen);
    struct RClass *cls=mrb_class_get(mrb,(char *)symptr);

    to_ret=mrb_obj_value(mrb_obj_alloc(mrb,MRB_TT_OBJECT,cls));

    int n_iv=read_integer(mrb,b);

    for(i=0;i<n_iv;i++)
    {
      ivname=load_marshal_recurse(mrb,b);
      iv=load_marshal_recurse(mrb,b);

      mrb_iv_set(mrb,to_ret,mrb_symbol(ivname),iv);
    }
  }
  return to_ret;  
  case 'U':
  {
    mrb_value cls_name=load_marshal_recurse(mrb,b),dumped_data;
    mrb_int symlen;
    uint8_t *symptr=mrb_sym2name_len(mrb,mrb_symbol(cls_name),&symlen);
    struct RClass *cls=mrb_class_get(mrb,(char *)symptr);

    to_ret=mrb_obj_value(mrb_obj_alloc(mrb,MRB_TT_OBJECT,cls));    
    dumped_data=load_marshal_recurse(mrb,b);
    mrb_funcall(mrb,to_ret,"marshal_load",1,dumped_data);
  }
  return to_ret;  
  default:
    mrb_raisef(mrb,E_TYPE_ERROR,"%S: Cannot manage %S!",mrb_str_new_cstr(mrb,__func__),mrb_str_new(mrb,(char *)&t,1));
  }
    
  return b->cur_v;
}

static int64_t read_integer(mrb_state *mrb,bfr_stc *b)
{
  if(b->cur_p>=b->len)
    mrb_raisef(mrb,E_TYPE_ERROR,"%S: String overflow!",mrb_str_new_cstr(mrb,__func__));

  int8_t l=((uint8_t *)b->data)[b->cur_p];

  if(l==0)
  {
    b->cur_p++;
    return 0;
  }

  if(l<-4)
  {
    b->cur_p++;
    return l+5;
  }
  if(l>4)
  {
    b->cur_p++;
    return l-5;
  }
    
  int nbytes=abs(l);
  union
  {
    int64_t s;
    uint8_t b[8];
  } usv;

  memset(usv.b,l>0 ? 0 : 0xff,8);  
  memcpy(usv.b,b->data+(b->cur_p+1),nbytes);

  b->cur_p+=nbytes+1;  

  return usv.s;
}

static uint8_t *read_byte_seq(mrb_state *mrb,bfr_stc *b,int *len)
{
  if(b->cur_p>=b->len)
    mrb_raisef(mrb,E_TYPE_ERROR,"%S: String overflow!",mrb_str_new_cstr(mrb,__func__));
  
  (*len)=read_integer(mrb,b);

  uint8_t *to_ret=b->data+b->cur_p;
  
  b->cur_p+=(*len);
  return to_ret;
}
  
/*********************************************
 *                   DUMP                    *
 *********************************************/

static mrb_value marshal_dump(mrb_state *mrb,mrb_value self)
{
  mrb_value o;
  
  mrb_get_args(mrb,"o",&o);

  bfr_stc b;
  uint8_t v;

  bzero(&b,sizeof(bfr_stc));
  
  v=MAJOR_V;
  b_append(&b,&v,sizeof(uint8_t));
  v=MINOR_V;
  b_append(&b,&v,sizeof(uint8_t));

  write_marshal_recurse(mrb,&b,o,0);

  mrb_value to_ret=mrb_str_new(mrb,b.data,b.len);
  int i;

  for(i=0;i<b.n_symbols;i++)
    free(b.symbols[i].str);
  free(b.symbols);
  free(b.data);  
  
  return to_ret;
}

static void b_append(bfr_stc *b,void *data,int len)
{
  b->data=realloc(b->data,b->len+len);
  memcpy(b->data+b->len,data,len);
  b->len+=len;
}

static void write_marshal_recurse(mrb_state *mrb,bfr_stc *b,mrb_value v,int lvl)
{  
  if(lvl>MAX_RECURSE)
    mrb_raisef(mrb,E_TYPE_ERROR,"%S: Recursion level exceeded",mrb_str_new_cstr(mrb,__func__));

  uint8_t n;
  int i;
  mrb_value vv;
  
  if(mrb_nil_p(v))
  {
    n='0';
    b_append(b,&n,sizeof(uint8_t));
    return;
  }

  struct RClass *cls=mrb_obj_class(mrb,v);

  switch(mrb_type(v))
  {
  case MRB_TT_FALSE:
    n='F';
    b_append(b,&n,sizeof(uint8_t));
    return;
  case MRB_TT_TRUE:
    n='T';
    b_append(b,&n,sizeof(uint8_t));
    return;
  case MRB_TT_CLASS:
    n='c';
    b_append(b,&n,sizeof(uint8_t));
    vv=mrb_class_path(mrb,mrb_class_ptr(v));
    write_byte_seq(mrb,b,(uint8_t *)RSTRING_PTR(vv),RSTRING_LEN(vv));
    return;
  case MRB_TT_MODULE:
    n='m';
    b_append(b,&n,sizeof(uint8_t));
    vv=mrb_class_path(mrb,mrb_class_ptr(v));
    write_byte_seq(mrb,b,(uint8_t *)RSTRING_PTR(vv),RSTRING_LEN(vv));
    return;
  case MRB_TT_FIXNUM:
    n='i';
    b_append(b,&n,sizeof(uint8_t));
    write_integer(mrb,b,mrb_fixnum(v));
    return;
  case MRB_TT_FLOAT:
  {
    n='f';
    b_append(b,&n,sizeof(uint8_t));

    uint8_t bfr[256];
    
    sprintf((char *)bfr,"%.18g",mrb_float(v));
    write_byte_seq(mrb,b,bfr,strlen((char *)bfr));
  }
  return;
  case MRB_TT_SYMBOL:
  {
    mrb_int symlen;
    uint8_t *symptr=mrb_sym2name_len(mrb,mrb_symbol(v),&symlen);

    write_symbol(mrb,b,symptr,symlen);
  }
  return;
  case MRB_TT_ARRAY:
  {
    n='[';
    b_append(b,&n,sizeof(uint8_t));
    write_integer(mrb,b,RARRAY_LEN(v));
    for(i=0;i<RARRAY_LEN(v);i++)
      write_marshal_recurse(mrb,b,mrb_ary_entry(v,i),lvl+1);
  }
  return;
  case MRB_TT_HASH:
  {
    n='{';
    b_append(b,&n,sizeof(uint8_t));
    mrb_value keys=mrb_hash_keys(mrb,v),key;
    write_integer(mrb,b,RARRAY_LEN(keys));
    for(i=0;i<RARRAY_LEN(keys);i++)
    {
      key=mrb_ary_entry(keys,i);      
      write_marshal_recurse(mrb,b,key,lvl+1);
      write_marshal_recurse(mrb,b,mrb_hash_get(mrb,v,key),lvl+1);
    }    
  }    
  return;
  case MRB_TT_STRING:
  {
    n='"';
    b_append(b,&n,sizeof(uint8_t));
    write_byte_seq(mrb,b,(uint8_t *)RSTRING_PTR(v),RSTRING_LEN(v));
  }
  return;
  case MRB_TT_DATA:
  {
    if(!mrb_obj_respond_to(mrb,cls,mrb_intern_lit(mrb,"_dump_data")))
      mrb_raisef(mrb,E_TYPE_ERROR,"%S: Cannot handle data of class [%S] (_dump_data not found)",mrb_str_new_cstr(mrb,__func__),
		 mrb_class_path(mrb,cls));
    n='d';
    b_append(b,&n,sizeof(uint8_t));
    write_marshal_recurse(mrb,b,mrb_funcall(mrb,v,"_dump_data",0),lvl+1);
  }
  return;
  case MRB_TT_OBJECT:
  {
    mrb_value cls_name=mrb_class_path(mrb,cls);

    if(mrb_obj_respond_to(mrb,cls,mrb_intern_lit(mrb,"marshal_dump")))
    {
      n='U';
      b_append(b,&n,sizeof(uint8_t));
      
      write_symbol(mrb,b,(uint8_t *)RSTRING_PTR(cls_name),RSTRING_LEN(cls_name));
      
      write_marshal_recurse(mrb,b,mrb_funcall(mrb,v,"marshal_dump",0),lvl+1);
    }
    else
    {
      n='o';
      b_append(b,&n,sizeof(uint8_t));
      
      write_symbol(mrb,b,(uint8_t *)RSTRING_PTR(cls_name),RSTRING_LEN(cls_name));    
      
      mrb_value iv=mrb_obj_instance_variables(mrb,v),ive,ive_v;
      int n_iv=RARRAY_LEN(iv);
      mrb_int symlen;
      uint8_t *symptr;
      
      write_integer(mrb,b,n_iv);
      
      for(i=0;i<n_iv;i++)
      {
	ive=mrb_ary_entry(iv,i);
	symptr=mrb_sym2name_len(mrb,mrb_symbol(ive),&symlen);
	write_symbol(mrb,b,symptr,symlen);
	ive_v=mrb_iv_get(mrb,v,mrb_symbol(ive));
	write_marshal_recurse(mrb,b,ive_v,lvl+1);
      }
    }
  }  
  return;  
  default:
  {
    mrb_raisef(mrb,E_TYPE_ERROR,"%S: Cannot handle class [%S] (type %S)",mrb_str_new_cstr(mrb,__func__),mrb_class_path(mrb,cls),mrb_fixnum_value(mrb_type(v)));
  }
  }
}

static void write_integer(mrb_state *mrb,bfr_stc *b,mrb_int i)
{
  if(i>-124 && i<123)
  {
    uint8_t n;
    
    if(i==0)
      n=0;
    else if(i<0)	
      n=(i-5)&0xff;
    else
      n=i+5;
  
    b_append(b,&n,sizeof(uint8_t));
    return;    
  }

  union
  {
    uint64_t u;
    int64_t s;
    uint8_t b[8];
  } usv;
  
  usv.s=i;
  
  uint8_t bfr[9];
  int j;
  
  if(i>0)
    for(j=8;j>0 && usv.b[7]==0;j--)
      usv.u<<=8;
  else
    for(j=8;j>0 && usv.b[7]==0xff;j--)
      usv.u<<=8;
  bfr[0]=j;
  memcpy(bfr+1,usv.b+(8-j),j);

  b_append(b,bfr,j+1);
}

static void write_byte_seq(mrb_state *mrb,bfr_stc *b,uint8_t *s,int len)
{
  write_integer(mrb,b,len);
  b_append(b,s,len);
}

static void write_symbol(mrb_state *mrb,bfr_stc *b,uint8_t *s,int len)
{
  uint8_t n;
  int i;
  
  for(i=0;i<b->n_symbols;i++)
    if(len==b->symbols[i].len && !memcmp(b->symbols[i].str,s,len))
      break;
  
  if(i<b->n_symbols)
  {
    n=';';
    b_append(b,&n,sizeof(uint8_t));
    write_integer(mrb,b,i);
  }
  else
  {
    b->symbols=realloc(b->symbols,sizeof(sym_stc)*(b->n_symbols+1));
    b->symbols[b->n_symbols].len=len;
    b->symbols[b->n_symbols].str=malloc(len);
    memcpy(b->symbols[b->n_symbols].str,s,len);
    b->n_symbols++;
    
    n=':';
    b_append(b,&n,sizeof(uint8_t));
    write_byte_seq(mrb,b,(uint8_t *)s,len);
  }
}



  
  
  
