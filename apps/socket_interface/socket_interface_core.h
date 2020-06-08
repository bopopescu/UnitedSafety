#pragma once
#include <stdarg.h>
#include <pthread.h>
#include <netinet/in.h>
#include <sys/time.h>

#include "../gcc/gcc.h"

/*
 * Description: The location where TRULink (RedStone) Unix Domain Sockets are stored.
 */
#define TRULINK_SOCKET_DIR "/var/run/redstone"
#define REDSTONE_SOCKET_DIR TRULINK_SOCKET_DIR

#define SOCKET_INTERFACE_ERROR_MAX_LENGTH 64

typedef void *(*client_server_fn)( void *);

struct ServerData;
struct ClientSocket;

struct ClientData
{
	int m_sockfd;
	int m_epipe;
	pthread_t m_thread;
	struct ServerData* m_data;
	int m_join;
	struct sockaddr_in m_addr;
};

struct ClientDataCache
{
	char m_buf[1024];

	// Description: "m_timeout_sec" and "m_timeout_usec" are second, and micro-second timeouts respectively.
	//	By default these values are zero, which means no timeout. If either of them are non-zero, then
	//	that timeout will be applied when reading data into buffer "m_buf" from the the operating system
	//	read/recv system calls.
	time_t m_timeout_sec;
	time_t m_timeout_usec;

	// Description: A pipe (index 0 is the read end, and index 1 is the write end) that can be used to
	//	interrupt a blocking read/recv. When ClientDataCache is initialized, both array indices are set
	//	to -1 (meaning the pipe is not used). The caller can initialize the pipe by calling the "pipe"
	//	system call and passing in "m_interrupt". The caller must make sure to close both ends of
	//	the pipe "m_interrupt" when finished (otherwise there will be a resource leak).
	int m_interrupt[2];

	char* m_c;

	// Description: Number of bytes currently in the buffer. If greater than zero, then any request for
	//	characters will first be served by reading from "m_c" ("m_buf"), otherwise a read/recv
	//	system call will be made to the operating system to get more data.
	int m_remain;
};

typedef struct ClientDataCache ReadDataCache;

typedef void *(*ServerShutdownCallback)(struct ServerData* p_data);
typedef void *(*ServerStartedCallback)(struct ServerData* p_data);

// Description: Callback which is called when the maximum number of connected clients has been reached.
//
//	p_data - The ServerData structure managing the connections.
//
//	p_fd - The client that tried to connect but failed.
typedef void *(*ServerMaxClientCallback)(struct ServerData* p_data, int p_fd);

struct UDPData
{
	int m_sockfd;
};

// ATS FIXME: Structure ServerData does not currently support being destroyed. This is because
//	variables such as "m_mutex" are initialized but never destroyed.
//
//	Move all variables requiring initialization into the "InternalServerData" structure.
struct ServerData
{
	pthread_t m_thread;
	pthread_mutex_t m_mutex;

	client_server_fn m_cs;
	ServerShutdownCallback m_shutdown_callback;
	ServerStartedCallback m_started_callback;

	// Description: If set, is called when the client connection cannot be completed due
	//	to reaching the limit on the maximum number of clients.
	//
	//	The file descriptor of the client that tried to connect is passed in as "p_fd".
	//
	//	See the declaration of "ServerMaxClientCallback" for more information.
	//
	//	Further connection processing is halted until the callback returns. The client
	//	"p_fd" is closed after the callback returns (so it is possible to send error
	//	messages, if desired, back to the client before the socket is closed).
	ServerMaxClientCallback m_max_client_callback;

	struct InternalServerData* m_sdata;
	struct ClientData* m_client;
	struct UDPData* m_udp;
	void* m_hook;

	int m_max_clients;
	int m_port;
	int m_backlog;
	int m_wait_for_server_start;

	char m_emsg[256];
};

EXTERN_C void init_ClientData(struct ClientData* p_data);

EXTERN_C void init_ServerData(struct ServerData* p_data, int p_max_clients);

EXTERN_C void free_ServerData(struct ServerData* p_data);

EXTERN_C int start_server(struct ServerData* p_data);

EXTERN_C int start_unix_domain_server(struct ServerData* p_data, const char* p_path, int p_delete_if_file_exists);

EXTERN_C int start_udp_server(struct ServerData* p_data);

EXTERN_C void stop_udp_server(struct ServerData* p_data);

EXTERN_C void set_unix_domain_socket_uid(struct ServerData* p_sd, int p_uid);
EXTERN_C void set_unix_domain_socket_gid(struct ServerData* p_sd, int p_gid);
EXTERN_C int set_unix_domain_socket_user_group(struct ServerData* p_sd, const char* p_user, const char* p_group);
EXTERN_C void set_unix_domain_socket_chmod(struct ServerData* p_sd, int p_mode);

/*
 * Description: Starts a TRULink (RedStone) Unix Domain server.
 */
#define start_redstone_ud_server start_trulink_ud_server
EXTERN_C int start_trulink_ud_server(struct ServerData* p_data, const char* p_name, int p_delete_if_file_exists);

typedef void (*SocketInterface_OnConnectionCallback)(struct ServerData *p_data, struct ClientData *p_cdata);
typedef void (*SocketInterface_TerminateCallback)(struct ServerData *p_data, int p_ret, int p_status);

// Description: Starts an accept server (accepts network connections).
//
//	When a client connects, the function "p_fn" is called as "p_fn(p_data, p_cdata)", where:
//
//		p_data	- is a pointer to "p_data" passed into the start_accept_server function.
//			  This pointer must never be deleted or freed from p_fn.
//
//		p_cdata	- is a pointer to malloced memory containing the client information.
//			  fn_on_connection is responsible for closing the socket in p_cdata,
//			  and for freeing the memory pointed to by p_cdata.
//
// FIXME: If "start_accept_server" exits, then there will be a zombie thread "static void *start_accept_server_event_thread(void *p)".
//	This thread will continue to handle cleaning of zombie processes. Should terminate this thread when "start_accept_server"
//	terminates.
//
// Return: This function will not return if everything is running normally. If the function returns, then
//	it is always an error value that is returned.
EXTERN_C const char *start_accept_server(
	struct ServerData *p_data,
	SocketInterface_OnConnectionCallback p_fn,
	SocketInterface_TerminateCallback p_term);

EXTERN_C void init_ClientDataCache( struct ClientDataCache*);

EXTERN_C void init_ReadDataCache(ReadDataCache*);

EXTERN_C void init_ClientSocket( struct ClientSocket *p_sock);

EXTERN_C void close_ClientSocket( struct ClientSocket *p_sock);

// Description: Causes the client socket "p_sock" to only connect using interface "p_iface",
//	where "p_iface" is the interface name (such as "can0", "eth0", "wlan0", "ra0").
//
//	To disable the interface restriction, simply set "p_iface" to the empty string.
EXTERN_C void set_client_connect_interface(struct ClientSocket* p_sock, const char* p_iface);

EXTERN_C int is_connected_ClientSocket(struct ClientSocket* p_sock);

EXTERN_C int connect_client(
	struct ClientSocket* p_sock,
	const char* p_ip,
	int p_port);

EXTERN_C int connect_unix_domain_client(
	struct ClientSocket* p_sock,
	const char *p_path);

#define connect_redstone_ud_client connect_trulink_ud_client
EXTERN_C int connect_trulink_ud_client(
	struct ClientSocket* p_sock,
	const char *p_name);

// Description: Closes ClientData "p_client" and returns it back to the ServerData's
//	free/available list.
EXTERN_C void close_client( struct ClientData* p_client);

// Description: Receives a "standard" response of arbitrary length.
//
//	Standard responses for the Socket Interface Library are strings terminated by carriage-return ("\r" or 0x0D).
//	All characters in the range [0x00 to 0xFF] are valid characters within the response string
//	(with the exception of the End-Of-Line character (EOL), which by default is carriage-return).
//
// ATS XXX: This is simply a convenience function. If the operation of this function is not suitable, then it
//          is preferrable to create another implementation of it, rather than extending this function. In some
//          cases, it may be simpler to use a wrapper function or interface, rather than directly using this
//          function.
//
// Parameters:
//
//    p_fd      - The file descriptor to read the response from. "p_fd" must be a file descriptor to a
//                socket.
//
//    p_des     - Must be a non-NULL pointer to a "char*". "p_des" will point to the response received
//                (which will be stored in a newly malloced pointer). "p_des" will point to NULL if an
//                error occured. The caller must "free" pointer "p_des" (if not NULL) when finished it.
//
//    p_max_len - The maximum length that the response may have. If the response would exceed this size,
//                then an error is returned, and errno is set to EFBIG. If "p_max_len" is less than or
//                zero, then it is taken as meaning that the response can have an arbitrary size.
//                If "p_max_len" is greater than zero, it must be at least 2 (1 char for the data, and
//                1 char for the NULL terminator). Note that the NULL terminator is not included in
//                the character count returned (it is a convenience character for character strings).
//
//    p_eol_char - A NULL terminated string containing a list of End-Of-Line (EOL) characters. NULL itself
//                 cannot be used as an EOL character. If "p_eol_char" is NULL, then the default EOL character
//                 will be used. The default EOL character is carriage-return ("\r" or 0x0D). To allow
//                 carriage-return to be used in the response string, simply use an alternate EOL character
//                 for "p_eol_char".
//
//    p_flags - The flags to send to the standard C library "recv" function. See "man recv" for
//              information on the flags available. Zero can be used for p_flags for normal/standard
//              operation.
//
// Return: The number of characters read is returned on success. A negative return value indicates an error,
//	and errno will be set accordingly. The End-Of-Line (EOL) terminating character is not returned in "p_des".
//      This also means that there is no way to know which character caused the line to terminate (if multiple
//      EOL characters are used in "p_eol_char").
//
//	"p_des" will be set to a newly malloced string which will contain the response (the caller must
//	"free" this pointer when finished with it). "p_des" will be set to point to a NULL string on error.
//
//	When the socket "p_fd" is closed, a negative value is returned and errno is set to ENODATA. The
//	partial response is discarded on ENODATA (only responses with end-of-line terminators will be returned).
EXTERN_C int get_standard_response(
	int p_fd,
	char** p_des,
	int p_max_len,
	const char* p_eol_char,
	int p_flags,
	struct ClientDataCache* p_cache);

// Description: Sends the command specified in "p_format" to socket
//	"p_sockfd" using the "send" system call.
//
//	"p_flags" contains the flags that are sent to the "send" system
//	call.
//
// Return: On success the number of characters sent is returned. On error
//	a negative errno value is returned.
EXTERN_C int send_cmd(
	int p_sockfd,
	int p_flags,
	const char *p_format,
	...)
	__attribute__ ((format (printf, 3, 4)));

EXTERN_C int ClientData_send_cmd(
	struct ClientData* p_cd,
	int p_flags,
	const char *p_format,
	...)
	__attribute__ ((format (printf, 3, 4)));

struct SocketError
{
	char m_msg[256];
};

// Description: Sends a message (or multiple messages in) "p_format" to "p_host" on port "p_port".
//	The number of messages in "p_format" is equal to the number of carriage returns (\r) as per
//	the TRULink (RedStone) messaging protocol.
//
// NOTE: The TRULink messaging protocol requires that each command is terminated with
//       a carriage return (\r), otherwise the host will not respond (will consider
//       the message as incomplete). This function does NOT automatically append a
//       carriage return (\r).
//
// NOTE: Each call to "send_msg" represents a single messaging sequence. All commands in "p_format"
//	will be received in order by the host. However there is no guaranty that the host will
//	execute all commands in order (it may cache or otherwise re-arrange the commands received,
//	therefore the caller must consulate the hosts command API).
//
//	Multiple consecutive calls to "send_msg" are guaranteed to be sent to the host in order,
//	however they may be received (from the Kernel's network data buffer) by the host in
//	random order (depending on how many accept/receive threads/processes the host has).
//
//	To guaranty in-order processing of commands, the caller MUST utilize hand-shaking.
//
// XXX: See the socket-interface C++ "SocketQuery" API which resolves some of the issues with this function.
//
// Return: A negative number is returned on error, otherwise the number of characters
//         sent is returned.
//
//         On error, "p_error" will contain the error message.
EXTERN_C int send_msg(
	const char* p_host,
	int p_port,
	struct SocketError* p_error,
	const char* p_format,
	...)
	__attribute__ ((format (printf, 4, 5)));

// Description: Same as "send_single_message", except a Unix Domain Socket ("p_path") is
//	used instead of a host/port combination.
EXTERN_C int send_unix_domain_msg(
	const char* p_path,
	struct SocketError* p_error,
	const char* p_format,
	...)
	__attribute__ ((format (printf, 3, 4)));

// Description: Same as "send_single_message", except an application name ("p_name") is
//	used instead of a host/port combination. The application name "p_name" together
//	with TRULINK_SOCKET_DIR is used to create the path to the Unix Domain Socket.
//	In other words, this function saves the caller from having to figure out the
//	path to where TRULink applications store their Unix Domain Sockets.
#define send_redstone_ud_msg send_trulink_ud_msg
EXTERN_C int send_trulink_ud_msg(
	const char* p_name,
	struct SocketError* p_error,
	const char* p_format,
	...)
	__attribute__ ((format (printf, 3, 4)));

// Description: Same as "send_single_message", except the message is routed through "app-monitor".
EXTERN_C int send_app_msg(
	const char* p_sender_id,
	const char* p_destination,
	struct SocketError* p_error,
	const char* p_format,
	...)
	__attribute__ ((format (printf, 4, 5)));

// Description: Waits (blocking the calling process) for application "p_app_name" to indicate
//	that it is ready. Applications inform "app-monitor" that they are ready, and this
//	function simply queries "app-monitor".
//
//	NOTE: The caller will be blocked indefinitely, unblocking only when the specified
//	      application is ready, or when an error occurs.
//
// Return: 1 is returned if the application is ready. A negative number is returned on error.
EXTERN_C int wait_for_app_ready(const char* p_app_name);

EXTERN_C int signal_app_ready(const char* p_app_name);

EXTERN_C int signal_app_unix_socket_ready(const char* p_app_name, const char* p_uds_name);

EXTERN_C int signal_app_tcp_socket_ready(const char* p_app_name, const char* p_sck_name);

EXTERN_C int client_getc( void *p_data, char *p_emsg, int p_len);

// Description: Reads data from socket connection "p_client" and returns the next character in the
//	stream. Data is read in blocks at a time to reduce system call overhead and thus resulting
//	in a more efficient read of data. Cached characters are stored in "p_cache".
//
//	If an error occurs, "p_emsg" (if not NULL and "p_len" is not zero length) will contain the
//	error message. "p_len" should be large enough to store the longest error message that
//	would be generated by "strerror". A length of SOCKET_INTERFACE_ERROR_MAX_LENGTH is ideal.
//
// Return: A negative errno number is returned on error, otherwise the next character read is returned.
EXTERN_C int client_getc_cached(struct ClientData* p_client, char* p_emsg, int p_len, struct ClientDataCache*);

// Description: Reads data from "p_sockfd" and returns the next character in the stream.
//	Data is read in blocks at a time to reduce system call overhead and thus resulting
//	in a more efficient read of data. Cached characters are stored in "p_cache".
//
// Return: A negative errno number is returned on error, otherwise the next character read is returned.
EXTERN_C int recv_cached(int p_sockfd, int p_flags, struct ClientDataCache* p_cache);

EXTERN_C int read_cached(int p_fd, ReadDataCache* p_cache);

EXTERN_C int client_closed( struct ClientData *p_sock);

#define IF_MAKE_MESSAGE(P_InitialSize, P_PrefixLen, P_format) \
	const int MAKE_MESSAGE_bufsize = P_InitialSize;\
	char MAKE_MESSAGE_buf[MAKE_MESSAGE_bufsize];\
	struct make_message_context mmc;\
	init_make_message_context(&mmc, MAKE_MESSAGE_buf, MAKE_MESSAGE_bufsize, P_PrefixLen, P_format);\
	int mmc_failed;\
\
	for(;;)\
	{\
		va_list ap;\
		va_start(ap, p_format);\
		mmc_failed = make_message(&mmc, ap);\
		va_end(ap);\
\
		if(!mmc_failed || (!(mmc.m_again)))\
		{\
			break;\
		}\
\
	}\
\
	if(!mmc_failed)

struct make_message_context
{
	const char* m_fmt;
	char* m_buf;
	char* m_p;
	int m_bufsize;
	int m_size;
	int m_first;
	int m_again;
	int m_prefix_len;
};

EXTERN_C void init_make_message_context(
	struct make_message_context* p_mmc,
	char* p_buf,
	int p_bufsize,
	int p_prefix_len,
	const char* p_fmt);

EXTERN_C void free_make_message_context(struct make_message_context* p_mmc);

EXTERN_C int make_message(struct make_message_context* p_mmc, va_list p_ap);
