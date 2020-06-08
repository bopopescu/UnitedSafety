#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/device.h>

#include "redstone.h"

void redstone_init_RedStoneParserContext(struct RedStoneParserContext* p_rpc, const char* p_name, int p_max_key_len, int p_max_val_len, char p_key[], char p_val[])
{
	memset(p_rpc, sizeof(*p_rpc), 0);
	p_rpc->m_name = p_name;
	p_rpc->m_max_key_len = p_max_key_len;
	p_rpc->m_max_val_len = p_max_val_len;
	p_rpc->m_key = p_key;
	p_rpc->m_val = p_val;
}
EXPORT_SYMBOL_GPL(redstone_init_RedStoneParserContext);

void redstone_append_command(struct RedStoneParserContext* p_rpc, struct RedStoneParserCommand* p_cmd)
{

	if(p_cmd)
	{
		struct RedStoneParserCommand** p = &(p_rpc->m_cmd);

		while(*p)
		{
			p = &(p_rpc->m_cmd->m_next);
		}

		*p = p_cmd;
	}

}
EXPORT_SYMBOL_GPL(redstone_append_command);

void redstone_reset_parser_state_machine(struct RedStoneParserContext* p_rpc)
{
	p_rpc->m_key[0] = '\0';
	p_rpc->m_val[0] = '\0';
	p_rpc->m_key_i = 0;
	p_rpc->m_val_i = 0;
	p_rpc->m_state = READ_KEY;
}
EXPORT_SYMBOL_GPL(redstone_reset_parser_state_machine);

static int process_command(struct RedStoneParserContext* p_rpc)
{
	struct RedStoneParserCommand* cmd = p_rpc->m_cmd;

	if(p_rpc->m_dbg >= 3)
	{
		printk(KERN_NOTICE "%s: key=\"%s\", val=\"%s\"\n", p_rpc->m_name, p_rpc->m_key, p_rpc->m_val);
	}

	while(cmd)
	{

		if(!strcmp(cmd->m_name, p_rpc->m_key))
		{
			return cmd->m_cmd(p_rpc);
		}

		cmd = cmd->m_next;
	}

	if(p_rpc->m_dbg >= 1)
	{
		printk(KERN_NOTICE "%s: Invalid key=\"%s\"\n", p_rpc->m_name, p_rpc->m_key);
	}

	return -EINVAL;
}

static int read_from_user(
	char* p_c,
	size_t* i,
	const char __user** p_user)
{

	if(!(*i))
	{
		return 0;
	}

	--(*i);

	if(copy_from_user(p_c, (*p_user)++, 1))
	{
		return -EFAULT;
	}

	return 1;
}

ssize_t redstone_write(
	struct RedStoneParserContext* p_rpc,
	struct file* p_file,
	const char __user* p_user,
	size_t p_len,
	loff_t* p_off)
{
	size_t i = p_len;

	if(!i)
	{
		return 0;
	}

	for(;;)
	{
		char c;

		if((READ_KEY == p_rpc->m_state) || (READ_VAL == p_rpc->m_state))
		{
			const int ret = read_from_user(&c, &i, &p_user);

			if(ret < 0)
			{
				redstone_reset_parser_state_machine(p_rpc);
				return ret;
			}

			if(!ret)
			{
				break;
			}

		}

		switch(p_rpc->m_state)
		{
		case READ_KEY:

			if('=' == c)
			{
				p_rpc->m_key[p_rpc->m_key_i] = '\0';
				p_rpc->m_state = READ_VAL;
				break;
			}

			if('\n' == c)
			{
				p_rpc->m_key[p_rpc->m_key_i] = '\0';
				p_rpc->m_state = PROCESS_COMMAND;
				break;
			}

			if(p_rpc->m_key_i >= (p_rpc->m_max_key_len - 1))
			{
				redstone_reset_parser_state_machine(p_rpc);
				return -EINVAL;
			}

			p_rpc->m_key[p_rpc->m_key_i++] = c;

			break;

		case READ_VAL:

			if('\n' == c)
			{
				p_rpc->m_val[p_rpc->m_val_i] = '\0';
				p_rpc->m_state = PROCESS_COMMAND;
				break;
			}

			if(p_rpc->m_val_i >= (p_rpc->m_max_val_len - 1))
			{
				redstone_reset_parser_state_machine(p_rpc);
				return -EINVAL;
			}

			p_rpc->m_val[p_rpc->m_val_i++] = c;
			break;

		case PROCESS_COMMAND:
			{
				const int ret = process_command(p_rpc);
				redstone_reset_parser_state_machine(p_rpc);

				if(ret)
				{
					return ret;
				}

			}
			break;
		}

	}

	return p_len - i;
}
EXPORT_SYMBOL_GPL(redstone_write);
