#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <stdarg.h>
#include "command_line_parser.h"

void init_CommandBuffer( struct CommandBuffer *p_cbuf)
{
	p_cbuf->m_argc = 0;
	p_cbuf->m_state = 0;

	p_cbuf->m_max_argc = MAX_COMMAND_ARGS;
	p_cbuf->m_max_buflen = MAX_COMMAND_BUF_LEN;

	p_cbuf->m_argv = p_cbuf->m_argv_static;
	p_cbuf->m_argi = p_cbuf->m_argi_static;
	p_cbuf->m_argf = p_cbuf->m_argf_static;
	p_cbuf->m_command_buf = p_cbuf->m_command_buf_static;
}


int alloc_dynamic_buffers(struct CommandBuffer* p_cbuf, int p_max_argc, int p_max_buflen)
{

	if(!p_cbuf)
	{
		return 0;
	}

	free_dynamic_buffers(p_cbuf);

	const size_t argx_size = sizeof(char*) * p_max_argc;

	void* p[2];
	p[0] = malloc(argx_size * 3);
	p[1] = malloc(p_max_buflen * sizeof(char));

	if(!(p[0] && p[1]))
	{

		if(p[0])
		{
			free(p[0]);
		}

		if(p[1])
		{
			free(p[1]);
		}

		return 0;
	}

	// XXX: "m_argi" controls the memory (pass it to function "free").
	p_cbuf->m_argi = (const char**)(p[0]);
	p_cbuf->m_argf = (const char**)(p[0] + argx_size);
	p_cbuf->m_argv = (char**)(p[0] + (argx_size * 2));

	p_cbuf->m_argc = 0;
	p_cbuf->m_command_buf = (char *)(p[1]);
	p_cbuf->m_max_argc = p_max_argc;
	p_cbuf->m_max_buflen = p_max_buflen;
	return 1;
}


void free_dynamic_buffers( struct CommandBuffer *p_cbuf)
{

	if(!p_cbuf)
	{
		return;
	}

	// XXX: "m_argi" controls the memory (use it for compares and pass it to function "free").
	if(p_cbuf->m_argi != p_cbuf->m_argi_static)
	{
		free(p_cbuf->m_argi);
		p_cbuf->m_argi = p_cbuf->m_argi_static;
		p_cbuf->m_argf = p_cbuf->m_argf_static;
		p_cbuf->m_argv = p_cbuf->m_argv_static;
		p_cbuf->m_max_argc = MAX_COMMAND_ARGS;
	}

	if(p_cbuf->m_command_buf != p_cbuf->m_command_buf_static)
	{
		free(p_cbuf->m_command_buf);
		p_cbuf->m_command_buf = p_cbuf->m_command_buf_static;
		p_cbuf->m_max_buflen = MAX_COMMAND_BUF_LEN;
	}

	p_cbuf->m_argc = 0;
}


// Description:
static const char *err_too_many_arguments = "too many arguments";
static const char* err_command_buffer_full = "command buffer full";
static const char* add_char( struct CommandParseState* p_state, const char p_c)
{

	if( (p_state->m_command_buf_remain) > 0)
	{

		if( ((char*)0) == (p_state->m_arg))
		{

			if( (p_state->m_cbuf->m_argc + 1) >= p_state->m_cbuf->m_max_argc)
			{
				return err_too_many_arguments;
			}

			p_state->m_arg = p_state->m_command_buf;
			const int i = p_state->m_cbuf->m_argc;
			const char* index_in_input_buf = p_state->m_input_buf + p_state->m_i;
			p_state->m_cbuf->m_argi[i] = index_in_input_buf;
			p_state->m_cbuf->m_argf[i] = index_in_input_buf;
			p_state->m_cbuf->m_argv[i] = p_state->m_arg;
			++(p_state->m_cbuf->m_argc);
			p_state->m_arg_cindex = 0;
		}

		--(p_state->m_command_buf_remain);
		++(p_state->m_command_buf);
		(p_state->m_arg)[(p_state->m_arg_cindex)++] = p_c;
		return ((const char *)0);
	}

	return err_command_buffer_full;
}


// Description:
static void end_arg( struct CommandParseState* p_state)
{

	if(p_state->m_arg)
	{
		--(p_state->m_command_buf_remain);
		++(p_state->m_command_buf);
		p_state->m_cbuf->m_argf[p_state->m_cbuf->m_argc - 1] = p_state->m_input_buf + p_state->m_i;
		(p_state->m_arg)[p_state->m_arg_cindex] = '\0';
		(p_state->m_arg) = (char *)0;
	}

}


// Description:
static const char *err_eof_input_buffer = "unexpected EOF buffer";
static const char *err_eof_first_hex = "unexpected EOF for first hex digit";
static const char *err_eof_second_hex = "unexpected EOF for second hex digit";
static const char *err_invalid_first_hex = "invalid first hex digit";
static const char *err_invalid_second_hex = "invalid second hex digit";
static const char *add_x_char( struct CommandParseState *p_state)
{
	if( (p_state->m_i + 2) >= (p_state->m_input_bufsize))
		return err_eof_input_buffer;

	char hi = p_state->m_input_buf[ p_state->m_i + 1];
	char lo = p_state->m_input_buf[ p_state->m_i + 2];

	if( '\0' == hi) return err_eof_first_hex;
	if( '\0' == lo) return err_eof_second_hex;

	if( (hi >= '0') && (hi <= '9')) hi = hi - '0';
	else if( (hi >= 'A') && (hi <= 'F')) hi = (hi - 'A') + 10;
	else if( (hi >= 'a') && (hi <= 'f')) hi = (hi - 'a') + 10;
	else return err_invalid_first_hex;

	if( (lo >= '0') && (lo <= '9')) lo = lo - '0';
	else if( (lo >= 'A') && (lo <= 'F')) lo = (lo - 'A') + 10;
	else if( (lo >= 'a') && (lo <= 'f')) lo = (lo - 'a') + 10;
	else return err_invalid_second_hex;

	const char *retval = add_char( p_state, ((hi << 4) + lo));
	if( !retval) p_state->m_i += 2;
	return retval;
}


// Description:
const char *gen_arg_list(
	const char *buf,
	const int bufsize,
	struct CommandBuffer *p_cbuf)
{
	struct CommandParseState state;
	struct CommandParseState* s = p_cbuf->m_state ? p_cbuf->m_state : &state;

	s->m_cbuf = p_cbuf;
	s->m_input_buf = buf;
	s->m_input_bufsize = bufsize;
	// XXX: Leave one character for terminating NULL in command buffer.
	s->m_command_buf_remain = p_cbuf->m_max_buflen - 1;
	s->m_command_buf = p_cbuf->m_command_buf;
	s->m_arg = 0;
	s->m_arg_cindex = 0;
	s->m_in_field = 0;
	s->m_in_quote = 0;
	s->m_is_escape = 0;
	s->m_cbuf->m_argc = 0;
	int done = 0;

	for(	s->m_i = 0;
		(!done) && (((s->m_i) < bufsize) && (buf[s->m_i]));
		++(s->m_i))
	{
		const char c = buf[s->m_i];

		if(s->m_is_escape)
		{
			s->m_is_escape = 0;
			s->m_in_field = 1;
			const char* emsg;

			switch( c)
			{
			case 'n': emsg = add_char(s, '\n'); break;
			case 'r': emsg = add_char(s, '\r'); break;
			case 't': emsg = add_char(s, '\t'); break;
			case 'b': emsg = add_char(s, '\b'); break;
			case '0': emsg = add_char(s, '\0'); break;
			case 'x': emsg = add_x_char(s); break;
			default: emsg = add_char(s, c); break;
			}

			if(emsg)
			{
				return emsg;
			}

		}
		else
		{

			switch( c)
			{
			case '\\':
				s->m_is_escape = !s->m_is_escape;
				break;

			case '"':
				s->m_in_quote = !s->m_in_quote;
				break;

			case '#':

				if(s->m_in_quote)
				{
					s->m_in_field = 1;
					const char *emsg = add_char( s, c);

					if( emsg)
					{
						return emsg;
					}

				}
				else
				{
					done = 1;
				}

				break;

			case ' ':
			case '\t':
			case '\r':	// For convenience, treat the carriage
					// return as white space.
			case '\n':
				{
					const char* emsg = 0;

					if(s->m_in_quote)
					{
						s->m_in_field = 1;
						emsg = add_char(s, c);

					}
					else if(s->m_in_field)
					{
						s->m_in_field = 0;
						end_arg(s);
					}

					if(emsg)
					{
						return emsg;
					}

				}
				break;

			default:
				{
					s->m_in_field = 1;
					const char* emsg = add_char(s, c);

					if(emsg)
					{
						return emsg;
					}

				}
				break;
			}

		}

	}

	end_arg(s);
	return 0;
}

static int is_command_terminator(
	struct GetCommandParam *p_gcp,
	const char c)
{
	const char *p = p_gcp->m_terminator;
	int count = p_gcp->m_terminator_len;
	while( (count--) > 0) {
		if( c == (*(p++))) return 1;
	}
	return 0;
}


static const char *err_command_too_long = "command is too long";


#define STRERROR( MAC_errno, MAC_buf_name, MAC_buf_len) \
        char MAC_buf_name[ MAC_buf_len]; \
        strerror_r( MAC_errno, MAC_buf_name, MAC_buf_len - 1); \
        MAC_buf_name[ MAC_buf_len - 1] = '\0'


const char *get_command_fn(
	struct GetCommandParam *p_gcp,
	char *buf,
	const int bufsize,
	int (*p_fn)( void *, char *, int),
	void *p_arg)
{
	const int elen = sizeof( p_gcp->m_emsg);
	char *emsg = p_gcp->m_emsg;
	int i = 0;
	int c;
	do {
		c = p_fn( p_arg, p_gcp->m_emsg, elen);
		if( c < 0) return emsg;

		if( !(is_command_terminator( p_gcp, c))) {
			if( i >= (bufsize - 1)) {
				for(;;) {
					const int c = p_fn( p_arg, p_gcp->m_emsg, elen);
					if( c < 0) return emsg;
					if( is_command_terminator( p_gcp, c)) break;
				}
				return err_command_too_long;
			}
			buf[i] = (char)c;
			++i;
		}

	} while( !(is_command_terminator( p_gcp, c)));
	buf[i] = '\0';
	return ((const char *)0);
}


int concat_args(
	char *p_des,
	int p_len,
	int p_argc,
	char **p_argv)
{
	if( (p_len--) <= 0) return -__LINE__;
	while( (p_argc-- > 0) && p_len) {
		const char *s = *(p_argv++);
		for(;;) {
			if( !(*s)) break;
			if( !p_len) return -__LINE__;
			*(p_des++) = *(s++);
			--p_len;
		}
		if( p_argc) {
			if( !p_len) return -__LINE__;
			*(p_des++) = ' ';
			--p_len;
		}
	}
	*p_des = 0;
	return 0;
}

char *malloc_printf(int *p_write, const char *p_format, ...)
{
	int n, size = 128;
	char *np;
	va_list ap;
	char *p = (char *)malloc(size);
	for(;;) {
		/* Try to print in the allocated space. */
		va_start(ap, p_format);
		n = vsnprintf(p, size, p_format, ap);
		va_end(ap);
		/* If that worked, return the string. */
		if (n > -1 && n < size) {
			if(p_write) *p_write = n;
			return p;
		}
		/* Else try again with more space. */
		if (n > -1)    /* glibc 2.1 */
			size = n+1; /* precisely what is needed */
		else           /* glibc 2.0 */
			size *= 2;  /* twice the old size */

		if(!(np = (char *)realloc(p, size))) {
			free(p);
			return NULL;
			if(p_write) *p_write = -ENOMEM;
		} else {
			p = np;
		}
	}
}
