#include <unistd.h>
#include "ats-security.h"

ats::PrivilegedCommand::PrivilegedCommand()
{
	pipe(m_command_pipe);
	pipe(m_response_pipe);
	m_pid = -1;
	m_server_status = 0;
}

int ats::PrivilegedCommand::fork()
{

	if(m_pid >= 0)
	{
		return m_pid;
	}

	m_pid = ::fork();

	if(is_child())
	{
		/* Redirect standard files to /dev/null */
		freopen( "/dev/null", "r", stdin);
		freopen( "/dev/null", "w", stdout);
		freopen( "/dev/null", "w", stderr);
	}

	if(m_pid < 0)
	{
		m_server_status = -m_pid;
	}

	return m_pid;
}

int ats::PrivilegedCommand::server_status() const
{
	return m_server_status;
}

bool ats::PrivilegedCommand::is_child() const
{
	return (0 == m_pid);
}

bool ats::PrivilegedCommand::is_parent() const
{
	return (m_pid > 0);
}

int ats::PrivilegedCommand::send_resp(const ats::String& p_s)
{

	if(m_server_status)
	{
		return -m_server_status;
	}

	const ats::String& s = ats::to_hex(p_s);
	return write(m_response_pipe[1], s.c_str(), s.length());
}

int ats::PrivilegedCommand::run()
{

	if(m_server_status)
	{
		return -m_server_status;
	}

	ReadDataCache_fd rdc(m_command_pipe[0]);
	ats::String name;
	ats::String arg;
	ats::String* part = &name;

	char hexbyte[2] = {0,0};
	char* hexp = hexbyte;

	for(;;)
	{
		const int c = rdc.getc();

		if(c < 0)
		{
			return c;
		}

		if('\r' == c)
		{
			FunctionMap::const_iterator i = m_fn.find(name);

			if(m_fn.end() == i)
			{
				const ats::String error("error=command does not exist");
				write(m_response_pipe[1], ",", 1);
				write(m_response_pipe[1], error.c_str(), error.length());
			}
			else
			{
				Command cmd = i->second;
				const ats::String& emsg = cmd(*this, arg);

				if(!emsg.empty())
				{
					const ats::String& error = "error=" + emsg;
					write(m_response_pipe[1], ",", 1);
					write(m_response_pipe[1], error.c_str(), error.length());
				}

			}

			write(m_response_pipe[1], "\r", 1);

			arg.clear();
			name.clear();
			part = &name;
		}
		else if(' ' == c)
		{

			if((&name) == part)
			{
				part = &arg;
			}

		}
		else
		{
			if((&name) == part)
			{
				(*part) += char(c);
			}
			else
			{

				if(hexp == hexbyte)
				{
					*(hexp++) = g_hextable[(c & 0x7f)];
				}
				else
				{
					*hexp = g_hextable[(c & 0x7f)];
					hexp = hexbyte;
					(*part) += char((hexbyte[0] << 4) | hexbyte[1]);
				}

			}

		}

	}

}

ats::String ats::PrivilegedCommand::add_command(const ats::String& p_name, Command p_cmd)
{
	std::pair <FunctionMap::iterator, bool> r = m_fn.insert(FunctionPair(p_name, p_cmd));

	if(!r.second)
	{
		return "already exists";
	}

	return ats::String();
}

static void process_info(const ats::String& p_info, ats::StringMap& p_sm)
{

	if(p_info.empty())
	{
		return;
	}

	ats::String k;
	ats::String v;
	ats::StringMap::get_key_val(p_info.c_str(), k, v);

	if("error" == k)
	{
		p_sm.set("error", v);
	}

}

ats::String ats::PrivilegedCommand::exec(const ats::String& p_name, const ats::String& p_cmd, std::ostream* p_out)
{

	if(m_server_status)
	{
		return ats::toStr(-m_server_status);
	}

	// Send the command.
	const ats::String& cmd = ats::to_hex(p_cmd);
	const int wfd = m_command_pipe[1];
	write(wfd, p_name.c_str(), p_name.length());
	write(wfd, " ", 1);
	write(wfd, cmd.c_str(), cmd.length());
	write(wfd, "\r", 1);

	// Wait for the command to complete.
	ReadDataCache_fd rdc(m_response_pipe[0]);
	char byte[2] = {0,0};
	char* p = byte;

	ats::String info;
	const int NORMAL_OUTPUT = 0;
	const int INFO_OUTPUT = 1;
	int state = NORMAL_OUTPUT;
	ats::StringMap im;

	for(;;)
	{
		const int c = rdc.getc();

		if(c < 0)
		{
			m_server_status = -c;
			return ats::toStr(c);
		}

		if(',' == c)
		{
			process_info(info, im);
			state = INFO_OUTPUT;
			continue;
		}

		if('\r' == c)
		{
			process_info(info, im);
			break;
		}

		if(NORMAL_OUTPUT == state)
		{

			if(p_out)
			{

				if(p == byte)
				{
					*(p++) = g_hextable[(c & 0x7f)];
				}
				else
				{
					*p = g_hextable[(c & 0x7f)];
					p = byte;
					(*p_out) << char((byte[0] << 4) | byte[1]);
				}

			}

		}
		else
		{
			info += char(c);
		}

	}

	return im.get("error");
}
