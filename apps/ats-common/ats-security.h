#pragma once

#include "ats-common.h"

namespace ats
{

// Description: Class to make it easier to run privileged commands in a more
//	secure context.
//
// XXX: This class is not thread-safe. Therefore proper synchronization techniques must be used
//	in threaded contexts.
class PrivilegedCommand
{
public:
	static const int CHILD = 0;

	PrivilegedCommand();

	// Description: Function prototype for commands.
	typedef ats::String (*Command)(PrivilegedCommand&, const ats::String& p_cmd);

	typedef std::map <const ats::String, Command> FunctionMap;
	typedef std::pair <const ats::String, Command> FunctionPair;

	// Description: Adds a command with name "p_name" and callback function "p_cmd".
	//
	//	"p_cmd" will perform the privileged operation.
	//
	//	"p_name" is simply the name of the command/operation for reference purposes.
	//
	// Return: An empty string is returned on success, and an error message is returned otherwise.
	ats::String add_command(const ats::String& p_name, Command p_cmd);

	// Description: Only called from the parent process, this function requests that the
	//	child execute operation named "p_name" with the following command "p_cmd".
	//
	//	Output (if any) generated by operation "p_name" will be written to "p_out".
	//
	//	"p_cmd" can be a literal command, arguments or raw data. The interpretation of
	//	"p_cmd" is up to operation "p_name".
	//
	//	This function does not return until operation "p_name" indicates that it has
	//	completed.
	//
	// Return: An empty string is returned on success, and an error message is returned otherwise.
	ats::String exec(const ats::String& p_name, const ats::String& p_cmd, std::ostream* p_out=0);

	// Description: Only called from the child process, this function starts the command server.
	//
	// Return: This function does not return unless an error occured. All return values are error
	//	codes. This function will never return 0 (the standard success code).
	int run();

	// Descrition: Works exactly like "fork" from "unistd.h". This member function call simplifies
	//	the use of "fork" by performing house-keeping operations (required when using "unistd.h" fork)
	//	automatically.
	//
	//	When running in the child process, the standard input, output and error wfile descriptors are
	//	closed and remapped to "/dev/null". The standard I/O file descriptors for the parent are
	//	untouched.
	//
	// NOTE: If privileged commands will need to communicate with the parent, then the PrivilegedCommand
	//	"send_resp" feature can be used, or the caller can setup a pipe before calling "fork"
	//	(and performing standard pipe maintenance/setup after the fork). In other words, this
	//	"fork" is the same as "unistd.h" fork, so all Inter-Process Communications (IPC) features
	//	may be used (just pay attention to security implications).
	//
	//	"send_resp" is not high-performance communications. If high-performance is required, then
	//	use other standard IPC.
	//
	// Return: 0 (or PrivilegedCommand::CHILD) is returned if running within the child process. A positive
	//	processed ID is returned if running within the parent process. -1 is returned on error
	//	and "errno" will contain the specific error code.
	int fork();

	bool is_child() const;

	bool is_parent() const;

	// Description: Only called from the child process, this function returns command response information
	//	to the parent.
	int send_resp(const ats::String& p_s);

	// Description: Returns 0 if the parent/child command server is running normally, and an errno code
	//	is returned otherwise. This function must always return 0. If it returns any other value,
	//	then it means the the parent/child command server is no longer functional, and should be
	//	destroyed and re-created.
	//
	// Return: 0 is returned if everything is normal, and an errno code is returned otherwise.
	int server_status() const;

private:
	FunctionMap m_fn;

	int m_command_pipe[2];
	int m_response_pipe[2];
	int m_pid;
	int m_server_status;
};

};