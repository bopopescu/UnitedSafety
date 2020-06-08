#ifndef COMMAND_LINE_PARSER_H
#define COMMAND_LINE_PARSER_H

#include "../gcc/gcc.h"

#define MAX_COMMAND_ARGS 64
#define MAX_COMMAND_BUF_LEN 324


#define GET_COMMAND_FN_IO_ERROR -1
#define GET_COMMAND_FN_EOF -2

struct GetCommandParam
{
	const char *m_terminator;
	int m_terminator_len;
	char m_emsg[64];
};

struct CommandParseState
{
	struct CommandBuffer* m_cbuf;
	const char* m_input_buf;
	int m_input_bufsize;
	char* m_command_buf;
	int m_command_buf_remain;
	int m_current_arg;
	int m_arg_cindex;
	char* m_arg;
	int m_i;
	int m_in_field;
	int m_in_quote;
	int m_is_escape;
};

struct CommandBuffer
{
	const char** m_argi;
	const char** m_argf;
	char** m_argv;
	char* m_command_buf;
	struct CommandParseState* m_state;
	int m_argc;
	int m_max_argc;
	int m_max_buflen;

	const char* m_argi_static[MAX_COMMAND_ARGS];
	const char* m_argf_static[MAX_COMMAND_ARGS];
	char* m_argv_static[MAX_COMMAND_ARGS];
	char m_command_buf_static[MAX_COMMAND_BUF_LEN];
};


EXTERN_C void init_CommandBuffer( struct CommandBuffer *p_cbuf);


EXTERN_C int alloc_dynamic_buffers( struct CommandBuffer *p_cbuf, int p_max_argc, int p_max_buflen);


EXTERN_C void free_dynamic_buffers( struct CommandBuffer *p_cbuf);


EXTERN_C const char *gen_arg_list(
	const char *buf,
	const int bufsize,
	struct CommandBuffer *p_cbuf);


EXTERN_C const char *get_command_fn(
	struct GetCommandParam *p_gcp,
	char *buf,
	const int bufsize,
	int (*p_fn)( void *, char *, int),
	void *p_arg);


EXTERN_C int concat_args(
	char *p_des,
	int p_len,
	int p_argc,
	char **p_argv);

EXTERN_C char *malloc_printf(int *p_nwrite, const char *p_format, ...)
	__attribute__((format(printf,2,3)));

#endif
