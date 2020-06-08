#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <semaphore.h>

#include "ats-common.h"
#include "ats-string.h"
#include "atslogger.h"
#include "socket_interface.h"
#include "command_line_parser.h"
#include "MyData.h"
#include "Command.h"

extern MyData* g_md;

class PostCommand
{
public:
	pthread_t m_thread;
	Command* m_cmd;
	CommandBuffer m_cb;

	PostCommand()
	{
		m_cmd = 0;
		init_CommandBuffer(&m_cb);
	}

};

typedef std::map <PostCommand*, void*> PostCommandMap;
typedef std::pair <PostCommand*, void*> PostCommandPair;
static PostCommandMap g_post_command;
static std::vector <PostCommand*> g_post_command_reaper_list;
static sem_t g_post_command_sem;
static pthread_t g_post_command_reaper_thread;
static pthread_mutex_t g_post_command_mutex;

static void lock_post_command()
{
	pthread_mutex_lock(&g_post_command_mutex);
}

static void unlock_post_command()
{
	pthread_mutex_unlock(&g_post_command_mutex);
}

// Description: Collects and cleans up completed post-command threads. Completed commands are removed in
//	First-In-First-Out (FIFO) order.
static void* post_command_reaper(void*)
{

	for(;;)
	{
		sem_wait(&g_post_command_sem);
		PostCommand* pc;

		lock_post_command();
		{
			std::vector <PostCommand*>::iterator i = g_post_command_reaper_list.begin();
			pc = *i;
			g_post_command_reaper_list.erase(i);
		}

		{
			PostCommandMap::iterator i = g_post_command.find(pc);

			if(i != g_post_command.end())
			{
				g_post_command.erase(i);
			}

		}
		unlock_post_command();

		pthread_join(pc->m_thread, 0);
		delete pc;
	}

	return 0;
}

// Description: Thread handler function which runs the command given in "p". Once the command has finished,
//	"this" thread is added to the end of the command reaper list (for a future "join").
static void* post_command_thread(void* p)
{
	PostCommand& pc = *((PostCommand*)p);
	ClientData cd;
	init_ClientData(&cd);
	pc.m_cmd->fn(*g_md, cd, pc.m_cb);
	lock_post_command();
	g_post_command_reaper_list.push_back(&pc);
	unlock_post_command();
	sem_post(&g_post_command_sem);
	return 0;
}

void init_post_command()
{
	pthread_mutex_init(&g_post_command_mutex, 0);
	pthread_create(&g_post_command_reaper_thread, 0, post_command_reaper, 0);
}

ats::String post_command(const ats::String& p_cmd)
{
	PostCommand* pc = new PostCommand();

	const char *err;

	if((err = gen_arg_list(p_cmd.c_str(), p_cmd.length(), &(pc->m_cb))))
	{
		delete pc;
		return ats::String("gen_arg_list failed: ") + err;
	}

	if(pc->m_cb.m_argc <= 0)
	{
		delete pc;
		return ats::String();
	}

	const ats::String cmd(pc->m_cb.m_argv[0]);
	ats::StringMap args;
	args.from_args(pc->m_cb.m_argc - 1, pc->m_cb.m_argv + 1);
	CommandMap::const_iterator i = g_md->m_cmd.find(cmd);

	if(i != g_md->m_cmd.end())
	{
		pc->m_cmd = i->second;
		const int ret = pthread_create(&(pc->m_thread), 0, post_command_thread, pc);

		if(ret)
		{
			delete pc;
			return ats::toStr(ret) + ": failed to start command thread";
		}

		return ats::String();
	}

	delete pc;
	return "invalid command \"" + cmd + "\"";
}
