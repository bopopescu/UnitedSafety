#pragma once

#include <linux/string.h>
#include <linux/fs.h>

typedef enum
{
	READ_KEY,
	READ_VAL,
	PROCESS_COMMAND
} REDSTONE_PARSER_STATE;

struct RedStoneParserContext;

struct RedStoneParserCommand
{
	struct RedStoneParserCommand* m_next;
	const char* m_name;
	int (*m_cmd)(struct RedStoneParserContext*);
};

struct RedStoneParserContext
{
	int m_dbg;
	int m_max_key_len;
	int m_max_val_len;
	const char* m_name;
	char* m_key;
	char* m_val;
	size_t m_key_i;
	size_t m_val_i;
	struct RedStoneParserCommand* m_cmd;
	REDSTONE_PARSER_STATE m_state;
};

/*
 * Description: Convenience macro for defining the parsing data structures.
 *
 * Parameters:
 *	M_prefix - (variable name) The prefix that generated global variables should use (to prevent name collision).
 *
 *	M_max_key_len - (int) The maximum length of keys
 *
 *	M_max_val_len - (int) The maximum length for variables
 *
 * Post Conditions:
 *	1. Generates the key buffer (static char []) called "{M_prefix}key", with length "M_max_key_len".
 *	2. Generates the value buffer (static char []) called "{M_prefix}val", with length "M_max_val_len".
 *	3. Generates the RedStoneParserContext struct called "{M_prefix}rpc".
 */
#define REDSTONE_COMMAND_PARSER_DEFINITION(M_prefix, M_max_key_len, M_max_val_len) \
	static char M_prefix ## key[M_max_key_len]; \
	static char M_prefix ## val[M_max_val_len]; \
	static struct RedStoneParserContext M_prefix ## rpc;

void redstone_init_RedStoneParserContext(struct RedStoneParserContext* p_rpc, const char* p_name, int p_max_key_len, int p_max_val_len, char p_key[], char p_val[]);

struct RedStoneParserContext* redstone_create_RedStoneParserContext(const char* p_name, int p_max_key_len, int p_max_val_len);

void redstone_reset_parser_state_machine(struct RedStoneParserContext* p_rpc);

void redstone_append_command(struct RedStoneParserContext* p_rpc, struct RedStoneParserCommand* p_cmd);

ssize_t redstone_write(
	struct RedStoneParserContext* p_rpc,
	struct file* p_file,
	const char __user* p_user,
	size_t p_len,
	loff_t* p_off);

