/*! @file
  @brief
  mruby/c File class based on libc.

  <pre>
  Copyright (C) 2015- Kyushu Institute of Technology.
  Copyright (C) 2015- Shimane IT Open-Innovation Center.

  This file is distributed under BSD 3-Clause License.

  </pre>
*/

/***** Feature test switches ************************************************/
/***** System headers *******************************************************/
//@cond
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <assert.h>
//@endcond

/***** Local headers ********************************************************/
#include "mrubyc.h"

/***** Constat values *******************************************************/
/***** Macros ***************************************************************/
#define MRBC_SET_INSTANCE_DATA(v,d) (*(void **)((v).instance->data) = (d))
#define MRBC_INSTANCE_DATA(v) (*(void **)((v).instance->data))

/***** Typedefs *************************************************************/
/***** Function prototypes **************************************************/
/***** Local variables ******************************************************/
/***** Global variables *****************************************************/
/***** Signal catching functions ********************************************/
/***** Functions ************************************************************/

//================================================================
/*! (method) constructor

  file1 = File.new( path, mode = "r" ) -> File
  (note)
   3rd parameter `perm` is not supported because not in fopen function.
*/
static void c_file_new(mrbc_vm *vm, mrbc_value v[], int argc)
{
  // v[1] is filename
  const char *filename;
  if( argc >= 1 ) {
    switch( v[1].tt ) {
    case MRBC_TT_STRING:
      filename = RSTRING_PTR(v[1]);
      break;
    default:
      goto RETURN_ARGUMENT_ERROR;
    }
  } else {
    goto RETURN_ARGUMENT_ERROR;
  }

  // v[2] is mode
  const char *mode = "r";
  if( argc >= 2 ) {
    switch( v[2].tt ) {
    case MRBC_TT_STRING:
      mode = RSTRING_PTR(v[2]);
      break;
    default:
      goto RETURN_ARGUMENT_ERROR;
    }
  }

  // open the file.
  FILE *fp = fopen(filename, mode);
  if( !fp ) {
    mrbc_raise( vm, MRBC_CLASS(ArgumentError), strerror(errno) );
    return;
  }

  // create mruby/c instance.
  mrbc_value val = mrbc_instance_new( vm, v[0].cls, sizeof(FILE *) );
  MRBC_SET_INSTANCE_DATA(val, fp);

  SET_RETURN( val );
  return;


 RETURN_ARGUMENT_ERROR:
  mrbc_raise( vm, MRBC_CLASS(ArgumentError), 0 );
}


//================================================================
/*! (method) close file

  file1.close
*/
static void c_file_close(mrbc_vm *vm, mrbc_value v[], int argc)
{
  FILE *fp = MRBC_INSTANCE_DATA(v[0]);

  fclose( fp );
}


//================================================================
/*! (method) delete file

  File.delete("filename")
*/
static void c_file_delete(mrbc_vm *vm, mrbc_value v[], int argc)
{
  int i;
  for( i = 1; i <= argc; i++ ) {
    if( v[i].tt != MRBC_TT_STRING ) {
      mrbc_raise( vm, MRBC_CLASS(ArgumentError), 0 );
      return;
    }

    const char *filename = RSTRING_PTR(v[i]);
    if( remove( filename ) != 0 ) {
      mrbc_raise( vm, MRBC_CLASS(ArgumentError), strerror(errno) );
      return;
    }
  }

  SET_INT_RETURN( i - 1 );
}


//================================================================
/*! (method) read

  File.read(path, length = nil) -> String | nil
  file1.read(length = nil) -> String | nil
  (note)
  2nd parameter `outbuf` is not supported.
*/
static void c_file_read(mrbc_vm *vm, mrbc_value v[], int argc)
{
  FILE *fp;
  int flag_all = 0;
  int pos_length;

  if( v[0].tt == MRBC_TT_CLASS ) {
    if( v[1].tt != MRBC_TT_STRING ) {
      mrbc_raise( vm, MRBC_CLASS(ArgumentError), 0 );
      return;
    }
    fp = fopen( RSTRING_PTR(v[1]), "r" );
    pos_length = 2;
    if( argc == 1 ) flag_all = 1;

  } else {
    fp = MRBC_INSTANCE_DATA(v[0]);
    pos_length = 1;
    if( argc == 0 ) flag_all = 1;
  }

  // check length parameter.
  int length;
  if( argc >= pos_length ) {
    switch( v[pos_length].tt ) {
    case MRBC_TT_INTEGER:
      length = mrbc_integer(v[pos_length]);
      break;
    case MRBC_TT_NIL:
      flag_all = 1;
      break;
    default:
      mrbc_raise( vm, MRBC_CLASS(ArgumentError), 0 );
      return;
    }
  }

  mrbc_value ret = mrbc_string_new( vm, 0, 0 );
  char buf[32];

  // read `length` bytes
  while( flag_all || length > 0 ) {
    int len = sizeof(buf);
    if( !flag_all && len > length ) len = length;

    len = fread( buf, 1, len, fp );
    if( len == 0 ) break;

    mrbc_string_append_cbuf( &ret, buf, len );
    length -= len;
  }

  if( pos_length == 2 ) {
    fclose( fp );
  }

  SET_RETURN( ret );
}


//================================================================
/*! (method) write

  file1.write(*str) -> Integer
*/
static void c_file_write(mrbc_vm *vm, mrbc_value v[], int argc)
{
  FILE *fp = MRBC_INSTANCE_DATA(v[0]);
  int length = 0;

  for( int i = 1; i <= argc; i++ ) {
    if( v[i].tt != MRBC_TT_STRING ) {
      mrbc_raise( vm, MRBC_CLASS(ArgumentError), 0 );
      return;
    }
    if( RSTRING_LEN(v[i]) == 0 ) continue;

    int len = fwrite( RSTRING_PTR(v[i]), 1, RSTRING_LEN(v[i]), fp );
    if( len == 0 ) break;

    length += len;
  }

  SET_INT_RETURN( length );
}


//================================================================
/*! (method) gets

  file1.gets

  (note)
  no parameter supprted.
*/
static void c_file_gets(mrbc_vm *vm, mrbc_value v[], int argc)
{
  FILE *fp = MRBC_INSTANCE_DATA(v[0]);
  mrbc_value ret = mrbc_string_new( vm, 0, 0 );
  char buf[1];

  while( fread( buf, 1, 1, fp ) != 0 ) {
    mrbc_string_append_cbuf( &ret, buf, 1 );
    if( buf[0] == '\n' ) break;
  }

  if( RSTRING_LEN(ret) == 0 ) {
    mrbc_string_delete( &ret );
    SET_NIL_RETURN();
  } else {
    SET_RETURN( ret );
  }
}


//================================================================
/*! (method) puts

  file1.puts(*arg) -> nil
  (note)
  Only string argument supported.
*/
static void c_file_puts(mrbc_vm *vm, mrbc_value v[], int argc)
{
  FILE *fp = MRBC_INSTANCE_DATA(v[0]);

  for( int i = 1; i <= argc; i++ ) {
    if( v[i].tt != MRBC_TT_STRING ) {
      mrbc_raise( vm, MRBC_CLASS(ArgumentError), 0 );
      return;
    }

    fwrite( RSTRING_PTR(v[i]), 1, RSTRING_LEN(v[i]), fp );
    if( RSTRING_LEN(v[i]) == 0 ||
        RSTRING_PTR(v[i])[ RSTRING_LEN(v[i])-1 ] != '\n' ) {
      putc('\n', fp);
    }
  }

  SET_NIL_RETURN();
}


//================================================================
/*! (method) pos

  file1.pos -> Integer
*/
static void c_file_pos(mrbc_vm *vm, mrbc_value v[], int argc)
{
  FILE *fp = MRBC_INSTANCE_DATA(v[0]);

  SET_INT_RETURN( ftell( fp ) );
}


//================================================================
/*! (method) pos=

  file1.pos = n
*/
static void c_file_set_pos(mrbc_vm *vm, mrbc_value v[], int argc)
{
  FILE *fp = MRBC_INSTANCE_DATA(v[0]);

  if( argc != 1 || v[1].tt != MRBC_TT_INTEGER ) {
    mrbc_raise( vm, MRBC_CLASS(ArgumentError), 0 );
    return;
  }

  fseek( fp, mrbc_integer(v[1]), SEEK_SET );
}


//================================================================
/*! initialize
*/
void mrbc_init_class_file(void)
{
  mrbc_class *file = mrbc_define_class(0, "File", 0);

  mrbc_define_method(0, file, "new", c_file_new);
  mrbc_define_method(0, file, "open", c_file_new);
  mrbc_define_method(0, file, "close", c_file_close);
  mrbc_define_method(0, file, "delete", c_file_delete);
  mrbc_define_method(0, file, "read", c_file_read);
  mrbc_define_method(0, file, "write", c_file_write);
  mrbc_define_method(0, file, "gets", c_file_gets);
  mrbc_define_method(0, file, "puts", c_file_puts);
  mrbc_define_method(0, file, "pos", c_file_pos);
  mrbc_define_method(0, file, "pos=", c_file_set_pos);
}
