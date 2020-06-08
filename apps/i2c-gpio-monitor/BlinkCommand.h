#pragma once
#include "Command.h"

class BlinkCommand : public Command
{
public:

	//-----------------------------------------------------------------------------------------------
	virtual void fn(MyData& p_md, ClientData& p_cd, const CommandBuffer& p_cb)
	{
		if(p_cb.m_argc < 2)
		{
			ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "ok\n");
			show_blink_list(p_md, p_cd);
			return;
		}

		if(p_cb.m_argc == 3)
			ats_logf(ATSLOG_INFO, "Blinker3: %s %s %s", p_cb.m_argv[0], p_cb.m_argv[1], p_cb.m_argv[2] );
		else if(p_cb.m_argc == 4)
			ats_logf(ATSLOG_INFO, "Blinker4: %s %s %s %s", p_cb.m_argv[0], p_cb.m_argv[1], p_cb.m_argv[2], p_cb.m_argv[3]  );
		else if(p_cb.m_argc == 5)
			ats_logf(ATSLOG_INFO, "Blinker5: %s %s %s %s %s", p_cb.m_argv[0], p_cb.m_argv[1], p_cb.m_argv[2], p_cb.m_argv[3] , p_cb.m_argv[4] );
		else if(p_cb.m_argc == 6)
			ats_logf(ATSLOG_INFO, "Blinker6: %s %s %s %s %s %s", p_cb.m_argv[0], p_cb.m_argv[1], p_cb.m_argv[2], p_cb.m_argv[3] , p_cb.m_argv[4], p_cb.m_argv[5] );
		const char* cmd = p_cb.m_argv[1];

		if(!(strcmp("kick", cmd) && strcmp("add", cmd)))
		{
			kick_or_add_blink(p_md, p_cd, p_cb, cmd);
		}
		else if(!strcmp("del", cmd))  // command format: 'blink del xxx'  where xxx was set with 'blink kick name=xxx'
		{

			if(p_cb.m_argc < 3)
			{
				ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "error\nMust specify Blinker name to delete\n");
				ats_logf(ATSLOG_DEBUG, "%s[%d]: Must specify Blinker name to delete", cmd, p_cd.m_sockfd);
				return;
			}

			p_md.remove_blinker(p_cb.m_argv[2]);
			ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "ok\n");
		}
		else if(!(strcmp("restart", cmd)))
		{
			restart_all_scripts(p_md, p_cd);
		}

		else
		{
			ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "error\nInvalid blink command \"%s\"\n", cmd);
			ats_logf(ATSLOG_DEBUG, "%s[%d]: Invalid blink command", cmd, p_cd.m_sockfd);
		}

	}

	//-----------------------------------------------------------------------------------------------
	void show_blink_list(MyData& p_md, ClientData& p_cd)
	{
		p_md.lock_blink();
		BlinkerMap::const_iterator i = p_md.m_BlinkerMap.begin();

		while(i != p_md.m_BlinkerMap.end())
		{
			const ats::String& blink_name = i->first;
			const Blinker& blinker = *(i->second);
			++i;

			ats::String script;

			blinker.lock();

			for(size_t j = 0; j < blinker.m_script.size(); ++j)
			{
				char buf[32];
				snprintf(buf, sizeof(buf) - 1, "%s[%s,%d]", j ? ", " : "", blinker.m_script[j] & (1 << 31) ? "ON" : "OFF", (blinker.m_script[j] & 0xffffff));
				buf[sizeof(buf) - 1] = '\0';
				script += buf;
			}

			const int priority = blinker.m_priority;
			const int timeout = blinker.timeout(false);
			const ats::String led(blinker.m_led);
			const ats::String app(blinker.app_name());
			const bool running_app = blinker.running_app(false);
			blinker.unlock();
			ats::String info;

			if(!led.empty())
			{
				ats::String s;
				ats_sprintf(&s, ", led=[%s]", led.c_str());
				info += s;
			}

			if(timeout)
			{
				ats::String s;
				ats_sprintf(&s, ", timeout=%d", timeout);
				info += s;
			}

			if(!app.empty())
			{
				ats::String s;
				ats_sprintf(&s, ", app%s=\"%s\"", running_app ? "[R]" : "", app.c_str());
				info += s;
			}

			if(!script.empty())
			{
				ats::String s;
				ats_sprintf(&s, ", script=\"%s\"", script.c_str());
				info += s;
			}

			ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "\t%s (priority=%d%s)\n", blink_name.c_str(), priority, info.c_str());
		}

		p_md.unlock_blink();
		// AWARE360 FIXME: End of response carriage return should happen before "unlock_blink".
		ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "\r");
	}

	//-----------------------------------------------------------------------------------------------
	void kick_or_add_blink(MyData& p_md, ClientData& p_cd, const CommandBuffer& p_cb, const char* p_cmd)
	{
		ats::StringMap m;
		m.from_args(p_cb.m_argc - 2, p_cb.m_argv + 2);

		const ats::String& name = m.get("name");
		const ats::String& led = m.get("led");
		const ats::String& app = m.get("app");
		const ats::String& script = (app.empty()) ? m.get("script") : ats::String();
		const ats::String& timeout = m.get("timeout");

		if(name.empty())
		{
			ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "error\nMust specify Blinker name\n");
			ats_logf(ATSLOG_DEBUG, "%s[%d]: Must specify Blinker name", p_cmd, p_cd.m_sockfd);
			return;
		}

		bool is_new;
		ats::String emsg;
		Blinker* b = p_md.add_blinker(name, led, script, is_new, emsg);

		if(!b)
		{
			ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "error\n%s\n", emsg.c_str());
			ats_logf(ATSLOG_DEBUG, "%s[%d]: %s: %s", p_cmd, p_cd.m_sockfd, name.c_str(), emsg.c_str());
			return;
		}

		if(!timeout.empty())
		{
			b->m_timeout = strtol(timeout.c_str(), 0, 0);
		}

		const bool is_kick_command = ('k' == (*p_cmd));

		if(is_kick_command)
		{
			b->kick_blink_timeout_thread();
		}

		const bool start_script = is_new || (b->m_script.empty());
		b->unlock();

		if(!(app.empty()))
		{
			const ats::String& emsg = p_md.load_blinker_program(*b, m);
			p_md.unlock_blink();

			if(!emsg.empty())
			{
				ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "error %s\n", emsg.c_str());
				ats_logf(ATSLOG_DEBUG, "%s[%d]: %s: %s", p_cmd, p_cd.m_sockfd, name.c_str(), emsg.c_str());
			}

			return;
		}

		if(is_new)
		{
			const ats::String& led_terminate_state = m.get("led_terminate_state");

			if(!led_terminate_state.empty())
			{
				b->lock();
				b->m_led_terminate_state = ats::get_bool(led_terminate_state);
				b->unlock();
			}

			const ats::String& priority = m.get("priority");
			b->m_priority = strtol(priority.c_str(), 0, 0);
		}

		if(!(is_kick_command && b->empty()))
		{
			b->set_script(script);
		}

		if(start_script)
		{
			b->run();
		}

		p_md.unlock_blink();
		ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "ok\n");
	}
	
	//-----------------------------------------------------------------------------------------------
	void restart_all_scripts(MyData& p_md, ClientData& p_cd)
	{
		p_md.lock_blink();
		BlinkerMap::const_iterator i = p_md.m_BlinkerMap.begin();

		while(i != p_md.m_BlinkerMap.end())
		{
			Blinker& blinker = *(i->second);
			++i;

			blinker.lock();
			blinker.SetRestart(true);
			blinker.m_index = 0;
			blinker.script_timer()->stop_timer(true, false);
			blinker.unlock();
		}
		p_md.unlock_blink();
	}
};
