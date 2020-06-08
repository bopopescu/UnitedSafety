/***************************************************************************
 * Author: Amour Hassan <amour@absolutetrac.com>
 * Date: May 12, 2013
 * Copyright 2008-2009 Siconix
 * Copyright 2008-2013 AbsoluteTrac
 *
 * Description:
 *
 * Network code derived from:
 *      "Beej's Guide to Network Programming":
 *      http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html#socket
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <pthread.h>
#include <pwd.h>
#include <grp.h>
#include <resolv.h>
#include <ifaddrs.h>
#include <netinet/tcp.h>

// BEGIN SERVER INCLUDES
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <linux/un.h>
#include <signal.h>
// END SERVER INCLUDES

#include "socket_interface.h"

#define STRERROR( MAC_errno, MAC_buf_name, MAC_buf_len) \
		char MAC_buf_name[ MAC_buf_len]; \
		strerror_r( MAC_errno, MAC_buf_name, MAC_buf_len - 1); \
		MAC_buf_name[ MAC_buf_len - 1] = '\0'

static sem_t g_sem;

struct InternalServerData
{
	struct sockaddr_un m_addr;
	int m_domain;
	int m_uid;
	int m_gid;
	int m_chmod;
	sem_t m_wait_sem;
};

static struct InternalServerData* init_InternalServerData(struct InternalServerData* p_isd)
{
	memset(p_isd, 0, sizeof(*p_isd));
	p_isd->m_domain = AF_INET;
	p_isd->m_chmod = 0770;
	sem_init(&p_isd->m_wait_sem, 0, 0);
	return p_isd;
}

static void free_InternalServerData(struct InternalServerData* p_isd)
{
	sem_destroy(&p_isd->m_wait_sem);
	free(p_isd);
}

void init_ClientData(struct ClientData* p_data)
{
	p_data->m_sockfd = -1;
	p_data->m_epipe = 0;
	p_data->m_data = 0;
	p_data->m_join = 0;
}

void init_ServerData(struct ServerData* p_data, int p_max_clients)
{
	p_data->m_port = 0;
	p_data->m_backlog = 20;
	p_data->m_cs = 0;
	p_data->m_udp = 0;
	p_data->m_hook = 0;
	p_data->m_shutdown_callback = 0;
	p_data->m_emsg[0] = '\0';
	p_data->m_max_clients = p_max_clients;
	p_data->m_wait_for_server_start = 1;

	p_data->m_sdata = init_InternalServerData( (struct InternalServerData*)malloc(sizeof(struct InternalServerData)));

	pthread_mutex_init( &(p_data->m_mutex), (const pthread_mutexattr_t*)0);

	p_data->m_client = (struct ClientData*)(malloc(sizeof(struct ClientData) * p_data->m_max_clients));
	int i;

	for( i = 0; i < p_data->m_max_clients; ++i)
	{
		init_ClientData( p_data->m_client + i);
	}

}

void free_ServerData( struct ServerData *p_data)
{

	if(p_data->m_client)
	{
		free(p_data->m_client);
		p_data->m_client = 0;
	}

	if(p_data->m_sdata)
	{
		free_InternalServerData(p_data->m_sdata);
		p_data->m_sdata = 0;
	}

}

void init_ClientSocket(struct ClientSocket* p_sock)
{

	if(!p_sock)
	{
		return;
	}

	memset(p_sock, 0, sizeof(*p_sock));
	p_sock->m_fd = -1;
	p_sock->m_domain = AF_INET;
}

void close_ClientSocket(struct ClientSocket* p_sock)
{
	if(!p_sock) return;
	if(p_sock->m_fd >= 0)
	{
		close(p_sock->m_fd);
	}
	init_ClientSocket(p_sock);
}

// Description: Returns a free ClientData structure pointer (only if there is a free ClientData
//	structure available. The number of free/available ClientData structures is determined by
//	"p_data".
//
// Return: A new (free) ClientData structure pointer is returned on success. NULL is returned
//	if there are no more ClientData structures available.
static struct ClientData* new_client(struct ServerData* p_data)
{
	pthread_mutex_lock( &(p_data->m_mutex));
	int i;

	for( i = 0; i < p_data->m_max_clients; ++i)
	{
		struct ClientData* c = p_data->m_client + i;

		if(!(c->m_data))
		{

			if(c->m_join)
			{
				c->m_join = 0;
				pthread_join(c->m_thread, 0); // XXX: Shall not block
			}

			c->m_data = p_data;
			pthread_mutex_unlock( &(p_data->m_mutex));
			return c;
		}

	}

	pthread_mutex_unlock( &(p_data->m_mutex));
	return 0;
}

static void h_close_client(struct ClientData* p_data)
{
	p_data->m_epipe = 0;

	if(p_data->m_sockfd >= 0)
	{
		close(p_data->m_sockfd);
		p_data->m_sockfd = -1;
	}

}

// Description: Returns ClientData "p_data" back to the list of free/available ClientData
//	structures.
//
//	XXX: "p_data" shall not be NULL.
static void free_client( struct ClientData* p_data)
{
	struct ServerData* data = p_data->m_data;
	pthread_mutex_lock( &(data->m_mutex));
	h_close_client(p_data);
	p_data->m_data = 0;
	pthread_mutex_unlock(&(data->m_mutex));
}

static void h_cleanup(void* p_arg)
{
	struct ClientData* data = (struct ClientData*)p_arg;
	data->m_join = 1; // XXX: No lock needed (ONLY h_cleanup can write, and ONLY read after "free-client")
	free_client(data);
}

static void* run_client_server(void* p_arg)
{
	struct ClientData* data = (struct ClientData*)p_arg;

	// XXX: "pthread_cleanup_pop" MUST appear together with
	//	"pthread_cleanup_push", otherwise "pthread_cleanup_push" will
	//	generate random/nonsense compiler errors.
	//
	// XXX: The non-standard/corrupt macro implementation of
	//	pthread_cleanup_* causes numerous undesirable side-effects
	//	and errors (if strange errors are generated with code
	//	containing pthread_cleanup_*, then double check the use of
	//	those macros).
	void* p;
	{
		pthread_cleanup_push(h_cleanup, data);
		p = data->m_data->m_cs(data);
		pthread_cleanup_pop(1);
	}
	return p;
}

void set_unix_domain_socket_uid(struct ServerData* p_sd, int p_uid)
{
	((struct InternalServerData*)(p_sd->m_sdata))->m_uid = p_uid;
}

void set_unix_domain_socket_gid(struct ServerData* p_sd, int p_gid)
{
	((struct InternalServerData*)(p_sd->m_sdata))->m_gid = p_gid;
}

int set_unix_domain_socket_user_group(struct ServerData* p_sd, const char* p_user, const char* p_group)
{
	struct passwd* p = getpwnam(p_user);

	if(!p)
	{
		return -errno;
	}

	// ATS FIXME: Is "initgroups" needed? Why not just always call "getgrnam"?
	//	For now support back-wards compatibility by using initgroups in for the cases
	//	where "username" == "groupname".
	if(initgroups(p->pw_name, p->pw_gid))
	{
		return -errno;
	}

	if(p_group && (!strcmp(p_user, p_group)))
	{
		((struct InternalServerData*)(p_sd->m_sdata))->m_uid = p->pw_uid;
		((struct InternalServerData*)(p_sd->m_sdata))->m_gid = p->pw_gid;
	}
	else
	{
		struct group* g = getgrnam(p_group);

		if(!g)
		{
			return -errno;
		}

		((struct InternalServerData*)(p_sd->m_sdata))->m_uid = p->pw_uid;
		((struct InternalServerData*)(p_sd->m_sdata))->m_gid = g->gr_gid;
	}

	return 0;
}

void set_unix_domain_socket_chmod(struct ServerData* p_sd, int p_mode)
{
	((struct InternalServerData*)(p_sd->m_sdata))->m_chmod = p_mode;
}

static void wait_for_server_start(struct ServerData* p_data)
{

	if(p_data->m_wait_for_server_start)
	{
		sem_wait(&(p_data->m_sdata->m_wait_sem));
	}

}

static void signal_end_of_server_wait(struct ServerData* p_data)
{

	if(p_data->m_wait_for_server_start)
	{
		sem_post(&(p_data->m_sdata->m_wait_sem));
	}

}

// Description:
//
// XXX: Modified From "Beej's Guide to Network Programming":
//    http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html#socket
static void* listen_server(void* p_arg)
{
	struct ServerData* data = (struct ServerData*)p_arg;
	int sockfd; // listen on sock_fd
	char* emsg = data->m_emsg;
	const int elen = sizeof(data->m_emsg) - 1;
	emsg[elen] = '\0';

	if((sockfd = socket(data->m_sdata->m_domain, SOCK_STREAM, 0)) == -1)
	{
		const int e = errno;
		STRERROR( e, err, 81);
		snprintf( emsg, elen,
			"%s,%d: Failed to create \"accept\" socket. (%d) %s",
			__FILE__, __LINE__,
			e, err);

		if(data->m_shutdown_callback)
		{
			data->m_shutdown_callback(data);
		}

		signal_end_of_server_wait(data);
		return emsg;
	}

	{
		int bind_result;

		if(AF_UNIX == data->m_sdata->m_domain)
		{
			struct InternalServerData* isd = data->m_sdata;
			struct sockaddr_un* addr = &(data->m_sdata->m_addr);

			// ATS FIXME: fchown on a socket does not work (still keeps root:root as owner for root
			//	created sockets). Using "chown" on the actual file instead, however this is done
			//	AFTER the bind, which results in a race condition (Time of check to time of use race condition).
			#if 0
			if(-1 == fchown(sockfd, (isd->m_uid > 0) ? isd->m_uid : -1, (isd->m_gid > 0) ? isd->m_gid : -1))
			{
				close(sockfd);
				snprintf(emsg, elen, "%s,%d: Failed to fchown(%d, %d,%d) socket", __FILE__, __LINE__,
					sockfd,
					isd->m_uid,
					isd->m_gid);

				if(data->m_shutdown_callback)
				{
					data->m_shutdown_callback(data);
				}

				signal_end_of_server_wait(data);
				return emsg;
			}
			#endif

			bind_result = bind(sockfd, (struct sockaddr*)addr, sizeof(*addr));

			{
				// ATS FIXME: (Time of check to time of use race condition) for this operation. Also note
				//	that "chmod" happens before "chown". The hope is that the "less secure" operation
				//	(chmod) is done first in a secure state, before permission changes happen. That is,
				//	"chmod" applies to root (which is secure), before it applies to a new owner (chown).
				if(-1 == chmod(isd->m_addr.sun_path, isd->m_chmod))
				{
					close(sockfd);
					snprintf(emsg, elen, "%s,%d: Failed to chmod(%s, %d) socket", __FILE__, __LINE__,
						isd->m_addr.sun_path,
						isd->m_chmod);

					if(data->m_shutdown_callback)
					{
						data->m_shutdown_callback(data);
					}

					signal_end_of_server_wait(data);
					return emsg;
				}

			}

			if(isd->m_uid || isd->m_gid)
			{
				// ATS FIXME: (Time of check to time of use race condition) for this operation, however losing
				//	the race is "fail safe". This is because one can only lower permissions, (never raise them).
				//	Also note that user and group IDs with the same privilege level may get mixed up, but they
				//	will never be elevated to higher privileges.
				if(-1 == chown(isd->m_addr.sun_path, (isd->m_uid > 0) ? isd->m_uid : -1, (isd->m_gid > 0) ? isd->m_gid : -1))
				{
					close(sockfd);
					snprintf(emsg, elen, "%s,%d: Failed to chown(%s,%d,%d) socket", __FILE__, __LINE__,
						isd->m_addr.sun_path,
						isd->m_uid,
						isd->m_gid);

					if(data->m_shutdown_callback)
					{
						data->m_shutdown_callback(data);
					}

					signal_end_of_server_wait(data);
					return emsg;
				}

			}

		}
		else
		{
			const int yes = 1;

			if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
			{
				const int e = errno;
				close( sockfd);
				STRERROR( e, err, 81);
				snprintf( emsg, elen,
					"%s,%d: Failed to set socket option. (%d) %s",
					__FILE__, __LINE__, e, err);

				if(data->m_shutdown_callback)
				{
					data->m_shutdown_callback(data);
				}

				signal_end_of_server_wait(data);
				return emsg;
			}

			struct sockaddr_in addr;
			memset(&addr, 0, sizeof(addr));
			addr.sin_family = AF_INET;		// host byte order
			addr.sin_port = htons(data->m_port);	// short, network byte order
			addr.sin_addr.s_addr = INADDR_ANY;	// auto-fill with my IP
			bind_result = bind(sockfd, (struct sockaddr*)&addr, sizeof(addr));
		}

		if(-1 == bind_result)
		{
			close(sockfd);
			snprintf(emsg, elen, "%s,%d: Failed to bind to socket", __FILE__, __LINE__);

			if(data->m_shutdown_callback)
			{
				data->m_shutdown_callback(data);
			}

			signal_end_of_server_wait(data);
			return emsg;
		}

	}

	if (listen(sockfd, data->m_backlog) == -1)
	{
		const int e = errno;
		close( sockfd);
		STRERROR( e, err, 81);
		snprintf( emsg, elen,
			"%s,%d: Failed to create listen socket. (%d) %s",
			__FILE__, __LINE__, e, err);

		if(data->m_shutdown_callback)
		{
			data->m_shutdown_callback(data);
		}

		signal_end_of_server_wait(data);
		return emsg;
	}

	signal_end_of_server_wait(data);

	for(;;)
	{ // main accept() loop
		struct ClientData* cdata;

		{
			struct sockaddr_in their_addr;	// connector's address information
			int new_fd; // new connection on new_fd

			if(AF_UNIX == data->m_sdata->m_domain)
			{
				struct sockaddr_un* addr = &(data->m_sdata->m_addr);
				socklen_t len = sizeof(*addr);
				new_fd = accept(sockfd, (struct sockaddr*)addr, &len);
			}
			else
			{
				socklen_t len = sizeof(their_addr);
				new_fd = accept(sockfd, (struct sockaddr*)&their_addr, &len);
			}

			if(-1 == new_fd)
			{
				const int e = errno;
				close( sockfd);
				STRERROR( e, err, 81);
				snprintf( emsg, elen,
					"%s,%d: accept error. (%d) %s",
					__FILE__, __LINE__, e, err);

				if(data->m_shutdown_callback)
				{
					data->m_shutdown_callback(data);
				}

				return emsg;
			}

			cdata = new_client(data);

			if(!cdata)
			{

				if(data->m_max_client_callback)
				{
					data->m_max_client_callback(data, new_fd);
				}

				shutdown(new_fd, SHUT_RDWR);
				close(new_fd);
				continue;
			}

			if(data->m_sdata->m_domain != AF_UNIX)
			{
				memcpy(&(cdata->m_addr), &their_addr, sizeof(cdata->m_addr));
			}

			cdata->m_sockfd = new_fd;
		}

		const int retval = pthread_create(
			&(cdata->m_thread),
			(pthread_attr_t*)0,
			run_client_server,
			cdata);

		if( retval)
		{
			free_client( cdata);
			snprintf( emsg, elen, "%s,%d: %d", __FILE__, __LINE__, retval);

			if(data->m_shutdown_callback)
			{
				data->m_shutdown_callback(data);
			}

			return emsg;
		}

	}

	if(data->m_shutdown_callback)
	{
		data->m_shutdown_callback(data);
	}

	return ((void *)0);
}

struct StartAcceptServerStruct
{
	struct ServerData *m_data;
	SocketInterface_TerminateCallback m_term;
};

// Description: Waits for child process spawned from "start_accept_server" to exit.
//
//	When a child process terminates, the StartAcceptServerStruct->m_term function is called (if not null).
//
// Return: This function never returns.
static void* start_accept_server_event_thread(void* p)
{
	struct StartAcceptServerStruct* s = (struct StartAcceptServerStruct*)p;
	struct ServerData* data = s->m_data;

	for(;;)
	{
		int status;
		sem_wait(&g_sem);
		const int ret = wait4(-1, &status, 0, 0);

		if(s->m_term)
		{
			s->m_term(data, ret, status);
		}

	}

	return 0;
}

const char* start_accept_server(
	struct ServerData* p_data,
	SocketInterface_OnConnectionCallback p_fn,
	SocketInterface_TerminateCallback p_term)
{

	if(!p_data)
	{
		return "parameter p_data is NULL";
	}

	if(!p_fn)
	{
		return "parameter p_fn is NULL";
	}

	struct ServerData* data = p_data;

	struct StartAcceptServerStruct* s = (struct StartAcceptServerStruct *)malloc(sizeof(struct StartAcceptServerStruct));
	s->m_data = data;
	s->m_term = p_term;

	sem_init(&g_sem, 0, 0);

	pthread_t thread;
	{
		const int retval = pthread_create(
			&thread,
			(pthread_attr_t*)0,
			start_accept_server_event_thread,
			s);

		if(retval)
		{
			return "%s,%d: Failed to create start_accept_server_event_thread";
		}

	}

	int sockfd;			// listen on sock_fd

	char* emsg = data->m_emsg;
	const int elen = sizeof(data->m_emsg);

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		const int e = errno;
		STRERROR( e, err, 81);
		snprintf( emsg, elen,
			"%s,%d: Failed to create \"accept\" socket. (%d) %s",
			__FILE__, __LINE__,
			e, err);
		return emsg;
	}

	{
		const int yes = 1;

		if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
		{
			const int e = errno;
			close( sockfd);
			STRERROR( e, err, 81);
			snprintf( emsg, elen,
				"%s,%d: Failed to set socket option. (%d) %s",
				__FILE__, __LINE__, e, err);
			return emsg;
		}
	
	}

	struct sockaddr_in my_addr;		// my address information
	my_addr.sin_family = AF_INET;		// host byte order
	my_addr.sin_port = htons(data->m_port);	// short, network byte order
	my_addr.sin_addr.s_addr = INADDR_ANY;	// auto-fill with my IP
	memset(my_addr.sin_zero, '\0', sizeof my_addr.sin_zero);

	if(bind(sockfd, (struct sockaddr *)&my_addr, sizeof my_addr) == -1)
	{
		close( sockfd);
		snprintf( emsg, elen,
			"%s,%d: Failed to bind to socket",
			__FILE__, __LINE__);
		return emsg;
	}

	if(listen(sockfd, data->m_backlog) == -1)
	{
		const int e = errno;
		close( sockfd);
		STRERROR( e, err, 81);
		snprintf( emsg, elen,
			"%s,%d: Failed to create listen socket. (%d) %s",
			__FILE__, __LINE__, e, err);
		return emsg;
	}

	for(;;)
	{ // main accept() loop
		struct sockaddr_in their_addr;	// connector's address information
		socklen_t sin_size = sizeof(their_addr);
		int new_fd;			// new connection on new_fd

		if((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size)) == -1)
		{
			const int e = errno;
			close( sockfd);
			STRERROR( e, err, 81);
			snprintf( emsg, elen,
				"%s,%d: accept error. (%d) %s",
				__FILE__, __LINE__, e, err);
			return emsg;
		}

		struct ClientData* cdata = (struct ClientData*)malloc(sizeof(struct ClientData));
		init_ClientData(cdata);
		cdata->m_data = p_data;
		memcpy(&(cdata->m_addr), &their_addr, sizeof(cdata->m_addr));
		cdata->m_sockfd = new_fd;

		sem_post(&g_sem);

		p_fn(p_data, cdata);
		close(new_fd);
		free(cdata);
	}

	return ""; // Never reached
}

int start_unix_domain_server(struct ServerData* p_data, const char* p_path, int p_delete_if_file_exists)
{

	if(!p_data)
	{
		return -EINVAL;
	}

	if(!(p_data->m_sdata))
	{
		snprintf( p_data->m_emsg, sizeof( p_data->m_emsg), "p_data->m_sdata is NULL");
		return -EINVAL;
	}

	if(p_delete_if_file_exists)
	{
		unlink(p_path);
	}

	memset(&(p_data->m_sdata->m_addr), 0, sizeof(p_data->m_sdata->m_addr));
	p_data->m_sdata->m_domain = AF_UNIX;
	p_data->m_sdata->m_addr.sun_family = AF_UNIX;
	strncpy(p_data->m_sdata->m_addr.sun_path, p_path, sizeof(p_data->m_sdata->m_addr.sun_path) - 1);
	return start_server(p_data);
}

static void h_build_trulink_ud_path(char* p_buf, size_t p_len, const char* p_name)
{
	snprintf(p_buf, p_len - 1, TRULINK_SOCKET_DIR "/%s", p_name);
	p_buf[p_len - 1] = '\0';
}

int start_trulink_ud_server(struct ServerData* p_data, const char* p_name, int p_delete_if_file_exists)
{
	char buf[1024];
	h_build_trulink_ud_path(buf, sizeof(buf), p_name);
	return start_unix_domain_server(p_data, buf, p_delete_if_file_exists);
}

static void* udp_server(void* p_arg)
{
	struct ServerData* data = (struct ServerData*)p_arg;
	int sockfd; // listen on sock_fd
	char* emsg = data->m_emsg;
	const int elen = sizeof(data->m_emsg) - 1;
	emsg[elen] = '\0';

	if((sockfd = socket(data->m_sdata->m_domain, SOCK_STREAM, 0)) == -1)
	{
		const int e = errno;
		STRERROR( e, err, 81);
		snprintf( emsg, elen,
			"%s,%d: Failed to create UDP socket. (%d) %s",
			__FILE__, __LINE__,
			e, err);

		if(data->m_shutdown_callback)
		{
			data->m_shutdown_callback(data);
		}

		signal_end_of_server_wait(data);
		return emsg;
	}

	{
		int bind_result;

		{
			const int yes = 1;

			if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
			{
				const int e = errno;
				close( sockfd);
				STRERROR( e, err, 81);
				snprintf( emsg, elen,
					"%s,%d: Failed to set socket option. (%d) %s",
					__FILE__, __LINE__, e, err);

				if(data->m_shutdown_callback)
				{
					data->m_shutdown_callback(data);
				}

				signal_end_of_server_wait(data);
				return emsg;
			}

			struct sockaddr_in addr;
			memset(&addr, 0, sizeof(addr));
			addr.sin_family = AF_INET;		// host byte order
			addr.sin_port = htons(data->m_port);	// short, network byte order
			addr.sin_addr.s_addr = INADDR_ANY;	// auto-fill with my IP
			bind_result = bind(sockfd, (struct sockaddr*)&addr, sizeof(addr));
		}

		if(-1 == bind_result)
		{
			close(sockfd);
			snprintf(emsg, elen, "%s,%d: Failed to bind to socket", __FILE__, __LINE__);

			if(data->m_shutdown_callback)
			{
				data->m_shutdown_callback(data);
			}

			signal_end_of_server_wait(data);
			return emsg;
		}

	}

	signal_end_of_server_wait(data);

	data->m_udp->m_sockfd = sockfd;
	data->m_cs(data);

	if(data->m_shutdown_callback)
	{
		data->m_shutdown_callback(data);
	}

	return ((void *)0);
}

int start_server(struct ServerData* p_data)
{

	if(!p_data)
	{
		return -EINVAL;
	}

	if( !(p_data->m_cs))
	{
		snprintf( p_data->m_emsg, sizeof( p_data->m_emsg), "client_server_fn not specified");
		return -EINVAL;
	}

	if(!p_data->m_sdata)
	{
		snprintf( p_data->m_emsg, sizeof( p_data->m_emsg), "p_data->m_sdata is NULL");
		return -EINVAL;
	}

	const int retval = pthread_create(
		&(p_data->m_thread),
		(pthread_attr_t *)0,
		(p_data->m_udp) ? udp_server : listen_server,
		p_data);

	if(retval)
	{
		snprintf( p_data->m_emsg, sizeof( p_data->m_emsg), "%s,%d: %d", __FILE__, __LINE__, retval);
		return -1;
	}

	wait_for_server_start(p_data);
	return 0;
}

int start_udp_server(struct ServerData* p_data)
{

	if(!p_data)
	{
		return -EINVAL;
	}

	if(!(p_data->m_udp = malloc(sizeof(struct ServerData))))
	{
		return -errno;
	}

	const int ret = start_server(p_data);

	if(ret < 0)
	{
		free(p_data->m_udp);
		p_data->m_udp = 0;
	}

	return ret;
}

void stop_udp_server(struct ServerData* p_data)
{

	if(!(p_data && p_data->m_udp))
	{
		return;
	}

	shutdown(p_data->m_udp->m_sockfd, SHUT_RDWR);
	close(p_data->m_udp->m_sockfd);

	pthread_join(p_data->m_thread, 0);
}

int connect_unix_domain_client(
	struct ClientSocket* p_sock,
	const char *p_path)
{

	if(!(p_sock && p_path))
	{
		return EINVAL;
	}

	p_sock->m_domain = AF_UNIX;

	return connect_client(p_sock, p_path, 0);
}

int connect_trulink_ud_client(
	struct ClientSocket* p_sock,
	const char *p_name)
{
	char buf[1024];
	h_build_trulink_ud_path(buf, sizeof(buf), p_name);
	return connect_unix_domain_client(p_sock, buf);
}

void set_client_connect_interface(struct ClientSocket* p_sock, const char* p_name)
{

	if(p_sock)
	{
		strncpy(p_sock->m_ifr.ifr_name, p_name, sizeof(p_sock->m_ifr.ifr_name));
		p_sock->m_ifr.ifr_name[sizeof(p_sock->m_ifr.ifr_name) - 1] = '\0';
	}

}

int is_connected_ClientSocket(struct ClientSocket* p_sock)
{
	return (p_sock && (p_sock->m_fd >= 0)) ? 1 : 0;
}

int connect_client(
	struct ClientSocket* p_sock,
	const char* p_ip,
	int p_port)
{

	if(!(p_sock && p_ip) || (p_port < 0))
	{
		return EINVAL;
	}

	if(p_sock->m_fd >= 0)
	{
		snprintf( p_sock->m_emsg, sizeof( p_sock->m_emsg), "%s,%d: Socket is already opened as file descriptor %d", __FILE__, __LINE__, p_sock->m_fd);
		return EINVAL;
	}

	int fd;
	int ret;

	if(AF_UNIX == p_sock->m_domain)
	{

		if((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		{
			const int e = errno;
			STRERROR( e, err, 81);
			snprintf( p_sock->m_emsg, sizeof( p_sock->m_emsg),
				"%s,%d: Failed to create AF_UNIX \"accept\" socket. (%d) %s",
				__FILE__, __LINE__, e, err);
			return e;
		}

		struct sockaddr_un addr;
		memset(&addr, 0, sizeof(addr));
		addr.sun_family = p_sock->m_domain;
		strncpy((char*)(&addr.sun_path), p_ip, UNIX_PATH_MAX);
		ret = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
	}
	else
	{

		if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		{
			const int e = errno;
			STRERROR( e, err, 81);
			snprintf( p_sock->m_emsg, sizeof( p_sock->m_emsg),
				"%s,%d: Failed to create AF_INET \"accept\" socket. (%d) %s",
				__FILE__, __LINE__, e, err);
			return e;
		}

		struct hostent* he;

		res_init();

		struct sockaddr_in addr;	// my address information
		addr.sin_family = AF_INET;	// host byte order
		addr.sin_port = htons(p_port);	// short, network byte order
		memset(addr.sin_zero, '\0', sizeof(addr.sin_zero));

		if(NULL == (he = gethostbyname( p_ip)))
		{
			const int e = errno;
			close(fd);
			STRERROR( e, err, 81);
			snprintf( p_sock->m_emsg, sizeof( p_sock->m_emsg),
				"%s,%d: Could not get host by name for ip=\"%s\". (%d) %s", __FILE__, __LINE__, p_ip, e, err);
			return e;
		}

		if('\0' != (p_sock->m_ifr.ifr_name[0]))
		{

			if(-1 == setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, (void*)&(p_sock->m_ifr), sizeof(p_sock->m_ifr)))
			{
				const int e = errno;
				close(fd);
				STRERROR( e, err, 81);
				snprintf( p_sock->m_emsg, sizeof( p_sock->m_emsg),
					"Failed to bind socket to interface \"%s\". (%d) %s", p_sock->m_ifr.ifr_name, e, err);
				return e;
			}

		}

		int use_keepalive = 0;

		if(p_sock->m_keepalive_count > 0)
		{

			if(-1 == setsockopt(fd, SOL_TCP, TCP_KEEPCNT, &(p_sock->m_keepalive_count), sizeof(p_sock->m_keepalive_count)))
			{
				const int e = errno;
				close(fd);
				STRERROR( e, err, 81);
				snprintf( p_sock->m_emsg, sizeof( p_sock->m_emsg),
					"Could not set TCP_KEEPCNT to %d. (%d) %s", p_sock->m_keepalive_count, e, err);
				return e;
			}

			use_keepalive = 1;
		}

		if(p_sock->m_keepalive_idle > 0)
		{

			if(-1 == setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &(p_sock->m_keepalive_idle), sizeof(p_sock->m_keepalive_idle)))
			{
				const int e = errno;
				close(fd);
				STRERROR( e, err, 81);
				snprintf( p_sock->m_emsg, sizeof( p_sock->m_emsg),
					"Could not set TCP_KEEPIDLE to %d. (%d) %s", p_sock->m_keepalive_idle, e, err);
				return e;
			}

			use_keepalive = 1;
		}

		if(p_sock->m_keepalive_interval > 0)
		{

			if(-1 == setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &(p_sock->m_keepalive_interval), sizeof(p_sock->m_keepalive_interval)))
			{
				const int e = errno;
				close(fd);
				STRERROR( e, err, 81);
				snprintf( p_sock->m_emsg, sizeof( p_sock->m_emsg),
					"Could not set TCP_KEEPINTVL to %d. (%d) %s", p_sock->m_keepalive_interval, e, err);
				return e;
			}

			use_keepalive = 1;
		}

		if(use_keepalive)
		{

			if(-1 == setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &use_keepalive, sizeof(use_keepalive)))
			{
				const int e = errno;
				close(fd);
				STRERROR( e, err, 81);
				snprintf( p_sock->m_emsg, sizeof( p_sock->m_emsg),
					"Could not set SO_KEEPALIVE. (%d) %s", e, err);
				return e;
			}

		}

		addr.sin_addr = *((struct in_addr *)(he->h_addr));
		ret = connect(fd, (struct sockaddr *)&addr, sizeof(addr));

		inet_ntop(addr.sin_family, &(addr.sin_addr), p_sock->m_resolved_ip, sizeof(p_sock->m_resolved_ip) - 1);
		p_sock->m_resolved_ip[sizeof(p_sock->m_resolved_ip) - 1] = '\0';
	}

	if(ret)
	{
		const int e = errno;
		close(fd);
		STRERROR( e, emsg, 81);
		snprintf( p_sock->m_emsg, sizeof( p_sock->m_emsg), "%s,%d: connect failed (uid=%d, gid=%d): (%d) %s", __FILE__, __LINE__, getuid(), getgid(), e, emsg);
		return e;
	}

	p_sock->m_fd = fd;
	return 0;
}

void close_client(struct ClientData* p_sock)
{

	if(p_sock)
	{
		pthread_mutex_lock( &(p_sock->m_data->m_mutex));
		h_close_client( p_sock);
		pthread_mutex_unlock( &(p_sock->m_data->m_mutex));
	}

}

int client_closed(struct ClientData* p_sock)
{
	pthread_mutex_lock( &(p_sock->m_data->m_mutex));
	const int b = (p_sock->m_sockfd < 0);
	pthread_mutex_unlock( &(p_sock->m_data->m_mutex));
	return b;
}

int get_standard_response(
	int p_fd,
	char** p_des,
	int p_max_len,
	const char* p_eol_char,
	int p_flags,
	struct ClientDataCache* p_cache)
{

	if(!(p_des && p_cache))
	{
		return -EINVAL;
	}

	*p_des = NULL;

	const int default_len = 992;
	int len = ((p_max_len > 0) && (p_max_len < default_len)) ? p_max_len : default_len;
	char* buf =  (char*)malloc(len);

	if(!buf)
	{
		free(buf);
		return -ENOMEM;
	}

	static const char *eol_char = "\r";

	if(!p_eol_char)
	{
		p_eol_char = eol_char;
	}

	char *p = buf;
	int total = 0;

	const int space_for_last_char_and_null = 2;

	for(;;)
	{

		if((p_max_len > 0) && (total > (p_max_len - space_for_last_char_and_null)))
		{
			free(buf);
			return -EFBIG;
		}

		if(total > (len - space_for_last_char_and_null))
		{
			len *= 2;
			buf = (char*)realloc(buf, len);
			p = buf + total;

			if(!buf)
			{
				free(buf);
				return -ENOMEM;
			}

		}

		const int c = recv_cached(p_fd, p_flags, p_cache);

		if(c < 0)
		{
			free(buf);
			return c;
		}

		*p = (char)c;

		{
			int eol = 0;
			const char* s = p_eol_char;

			while(*s)
			{

				if(*s == *p)
				{
					eol = 1;
					break;
				}

				++s;
			}

			if(eol)
			{
				break;
			}

		}

		++p;
		++total;
	}

	*p = '\0';
	*p_des = buf;
	return total;
}

void init_make_message_context(
	struct make_message_context* p_mmc,
	char* p_buf,
	int p_bufsize,
	int p_prefix_len,
	const char* p_fmt)
{
	p_mmc->m_first = 1;
	p_mmc->m_buf = p_buf;
	p_mmc->m_bufsize = p_bufsize;
	p_mmc->m_fmt = p_fmt;
	p_mmc->m_prefix_len = p_prefix_len;
}

void free_make_message_context(struct make_message_context* p_mmc)
{

	if(p_mmc->m_p && (p_mmc->m_p != p_mmc->m_buf))
	{
		free(p_mmc->m_p);
	}

}

int make_message(struct make_message_context* p_mmc, va_list p_ap)
{
	char* p;
	int size;

	if(p_mmc->m_first)
	{
		p = p_mmc->m_buf;
		size = p_mmc->m_bufsize;
		p_mmc->m_first = 0;

		if(!p)
		{
			p = malloc(size);

			if(!p)
			{
				p_mmc->m_again = 0;
				return -ENOMEM;
			}

		}

	}
	else
	{
		p = p_mmc->m_p;
		size = p_mmc->m_size;

		if(p == p_mmc->m_buf)
		{
			p = malloc(size);

			if(!p)
			{
				p_mmc->m_again = 0;
				return -ENOMEM;
			}

		}
		else
		{
			char* np = realloc(p, size);

			if(np == NULL)
			{
				free(p);
				p_mmc->m_again = 0;
				return -ENOMEM;
			}

			p = np;
		}

	}

	const int write_size = size - p_mmc->m_prefix_len;
	const int n = vsnprintf(p + p_mmc->m_prefix_len, (write_size > 0) ? write_size : 0, p_mmc->m_fmt, p_ap);

	if((n > -1) && (n < (size - p_mmc->m_prefix_len)))
	{
		p_mmc->m_p = p;
		p_mmc->m_size = n + p_mmc->m_prefix_len;
		return 0;
	}

	if(n > -1)
	{ /* glibc 2.1 */
		size = n + 1 + p_mmc->m_prefix_len; /* precisely what is needed */
	}
	else
	{ /* glibc 2.0 */
		size *= 2; /* twice the old size */
	}

	p_mmc->m_p = p;
	p_mmc->m_size = size;
	p_mmc->m_again = 1;
	return -ENOMEM;
}

int send_cmd(
	int p_sockfd,
	int p_flags,
	const char* p_format,
	...)
{

	if(p_sockfd < 0)
	{
		return -EINVAL;
	}

	{
		const int initial_bufsize = 240;
		const int prefix_len = 0;

		IF_MAKE_MESSAGE(initial_bufsize, prefix_len, p_format)
		{
			const int ret = send(p_sockfd, mmc.m_p, mmc.m_size, p_flags);
			free_make_message_context(&mmc);
			return ret;
		}

	}

	return -ENOMEM;
}

int ClientData_send_cmd(
	struct ClientData* p_cd,
	int p_flags,
	const char* p_format,
	...)
{

	if((!p_cd) || p_cd->m_sockfd < 0)
	{
		return -EINVAL;
	}

	if(p_cd->m_epipe)
	{
		return p_cd->m_epipe;
	}

	{
		const int initial_bufsize = 240;
		const int prefix_len = 0;

		IF_MAKE_MESSAGE(initial_bufsize, prefix_len, p_format)
		{
			const int ret = send(p_cd->m_sockfd, mmc.m_p, mmc.m_size, p_flags);
			free_make_message_context(&mmc);

			if((ret < 0) && (EPIPE == errno))
			{
				p_cd->m_epipe = errno;
			}

			return ret;
		}

	}

	return -ENOMEM;
}

int client_getc( void *p_data, char *p_emsg, int p_len)
{
	struct ClientData *data = (struct ClientData *)p_data;
	char c;
	const ssize_t nread = recv( data->m_sockfd, &c, 1, 0);

	if( nread < 0)
	{
		STRERROR( errno, e, p_len);
		strcpy( p_emsg, e);
		return -EIO;
	}

	if( !nread)
	{
		strncpy( p_emsg, "EOF", p_len);
		return -ENODATA;
	}

	return c;
}

void init_ClientDataCache(struct ClientDataCache* p_cache)
{
	init_ReadDataCache((ReadDataCache*)p_cache);
}

void init_ReadDataCache(ReadDataCache* p_cache)
{
	memset(p_cache, 0, sizeof(*p_cache));
	p_cache->m_interrupt[0] = -1;
	p_cache->m_interrupt[1] = -1;
}

int client_getc_cached(struct ClientData* p_data, char* p_emsg, int p_len, struct ClientDataCache* p_cache)
{
	struct ClientData* data = p_data;
	const int ret = recv_cached(data->m_sockfd, 0, p_cache);

	if(p_emsg && (p_len > 0))
	{
		p_emsg[0] = '\0';
	}

	if(!ret)
	{

		if(p_emsg && (p_len > 0))
		{
			strncpy(p_emsg, "EOF", p_len);
			p_emsg[p_len - 1] = '\0';
		}

		return -ENODATA;
	}
	else if(ret < 0)
	{

		if(p_emsg && (p_len > 0))
		{
			STRERROR((-ret), e, p_len);
			strcpy(p_emsg, e);
		}

	}

	return ret;
}

int recv_cached(int p_sockfd, int p_flags, struct ClientDataCache* p_cache)
{

	if(!(p_cache->m_remain))
	{
		const int select_with_time = (p_cache->m_timeout_sec || p_cache->m_timeout_usec);
		const int select_with_pipe = (p_cache->m_interrupt[0] >= 0);

		if(select_with_time || select_with_pipe)
		{
			fd_set fds;
			FD_ZERO(&fds);
			FD_SET(p_sockfd, &fds);
			int max = p_sockfd;

			if(select_with_pipe)
			{
				FD_SET(p_cache->m_interrupt[0], &fds);
				max = ((p_cache->m_interrupt[0]) > max) ? (p_cache->m_interrupt[0]) : max;
			}

			struct timeval tv_buf;
			struct timeval* tv = NULL;

			if(select_with_time)
			{
				tv = &tv_buf;
				tv->tv_sec = p_cache->m_timeout_sec;
				tv->tv_usec = p_cache->m_timeout_usec;
			}

			const int retval = select(max + 1, &fds, NULL, NULL, tv);

			switch(retval)
			{
			case -1: return -errno;
			case 0: return -ETIMEDOUT;
			}

			if(select_with_pipe && FD_ISSET(p_cache->m_interrupt[0], &fds))
			{
				return -EINTR;
			}

		}

		const ssize_t nread = recv(p_sockfd, p_cache->m_buf, sizeof(p_cache->m_buf), p_flags);

		if(nread < 0)
		{
			return -errno;
		}

		if(!nread)
		{
			return -ENODATA;
		}

		p_cache->m_remain = (int)nread;
		p_cache->m_c = p_cache->m_buf;
	}

	const unsigned char c = *((p_cache->m_c)++);
	--(p_cache->m_remain);

	return c;
}

int read_cached(int p_fd, ReadDataCache* p_cache)
{

	if(!(p_cache->m_remain))
	{
		const ssize_t nread = read(p_fd, p_cache->m_buf, sizeof(p_cache->m_buf));

		if(nread < 0)
		{
			return -errno;
		}

		if(!nread)
		{
			return -ENODATA;
		}

		p_cache->m_remain = (int)nread;
		p_cache->m_c = p_cache->m_buf;
	}

	const unsigned char c = *((p_cache->m_c)++);
	--(p_cache->m_remain);

	return c;
}

static void set_socket_error_msg(struct SocketError* p_error, const char* p_msg)
{

	if(p_error)
	{

		if(!p_msg)
		{
			p_error->m_msg[0] = '\0';
		}
		else
		{
			strncpy(p_error->m_msg, p_msg, sizeof(p_msg) - 1);
			p_error->m_msg[sizeof(p_error->m_msg) - 1] = '\0';
		}

	}

}

static ssize_t h_send_msg(const char* p_host, int p_port, struct SocketError* p_error, const char* p_msg, size_t p_len)
{

	if(!p_len)
	{
		return 0;
	}

	struct ClientSocket cs;
	init_ClientSocket(&cs);
	const int ret = connect_client(&cs, p_host, p_port);

	if(ret)
	{
		set_socket_error_msg(p_error, cs.m_emsg);
		return (ret > 0) ? (-ret) : ret;
	}

	const ssize_t nwrite = send(cs.m_fd, p_msg, p_len, MSG_NOSIGNAL);
	close_ClientSocket(&cs);

	if(nwrite < 0)
	{
		set_socket_error_msg(p_error, strerror(errno));
	}

	return nwrite;
}

int send_msg(const char* p_host, int p_port, struct SocketError* p_error, const char* p_format, ...)
{

	if(p_error)
	{
		p_error->m_msg[0] = '\0';
	}

	if(!p_host)
	{
		set_socket_error_msg(p_error, "p_host is NULL");
		return -EINVAL;
	}

	if(p_port <= 0 || p_port > 65535)
	{
		set_socket_error_msg(p_error, "p_port is out of range");
		return -EINVAL;
	}

	if(!p_format)
	{
		set_socket_error_msg(p_error, "p_format is NULL");
		return -EINVAL;
	}

	int n, size = 224;
	char *p, *np;
	va_list ap;

	if ((p = (char *)malloc(size)) == NULL)
	{
		set_socket_error_msg(p_error, "out of memory");
		return -ENOMEM;
	}

	for(;;)
	{
		/* Try to print in the allocated space. */
		va_start(ap, p_format);
		n = vsnprintf(p, size, p_format, ap);
		va_end(ap);

		/* If that worked, return the string. */
		if(n > -1 && n < size)
		{
			const int ret = h_send_msg(p_host, p_port, p_error, p, n);
			free(p);
			return ret;
		}

		/* Else try again with more space. */
		if (n > -1)
		{    /* glibc 2.1 */
			size = n+1; /* precisely what is needed */
		}
		else
		{           /* glibc 2.0 */
			size *= 2;  /* twice the old size */
		}

		if ((np = (char *)realloc (p, size)) == NULL)
		{
			free(p);
			set_socket_error_msg(p_error, "out of memory");
			return -ENOMEM;
		}
		else
		{
			p = np;
		}

	}

}

// Description: Does the actual "send" system call for the given Unix Domain Socket message "p_msg".
//
// Return: The number of characters sent is returned. A negative "errno" number is returned on error.
static ssize_t h_do_send_unix_domain_msg(const char* p_path, struct SocketError* p_error, const char* p_msg, size_t p_len)
{

	if(!p_len)
	{
		return 0;
	}

	struct ClientSocket cs;
	init_ClientSocket(&cs);
	const int ret = connect_unix_domain_client(&cs, p_path);

	if(ret)
	{
		set_socket_error_msg(p_error, cs.m_emsg);
		return (ret > 0) ? (-ret) : ret;
	}

	const ssize_t nwrite = send(cs.m_fd, p_msg, p_len, MSG_NOSIGNAL);

	if(nwrite < 0)
	{
		const int err = errno;
		close_ClientSocket(&cs);
		set_socket_error_msg(p_error, strerror(err));
		return -err;
	}

	close_ClientSocket(&cs);
	return nwrite;
}

int send_unix_domain_msg(const char* p_path, struct SocketError* p_error, const char* p_format, ...)
{
	{
		const int initial_bufsize = 240;
		const int prefix_len = 0;

		IF_MAKE_MESSAGE(initial_bufsize, prefix_len, p_format)
		{
			const int ret = h_do_send_unix_domain_msg(p_path, p_error, mmc.m_p, mmc.m_size);
			free_make_message_context(&mmc);
			return ret;
		}

	}

	return -ENOMEM;
}

int send_trulink_ud_msg(const char* p_name, struct SocketError* p_error, const char* p_format, ...)
{
	char path[1024];
	h_build_trulink_ud_path(path, sizeof(path), p_name);

	{
		const int initial_bufsize = 240;
		const int prefix_len = 0;

		IF_MAKE_MESSAGE(initial_bufsize, prefix_len, p_format)
		{
			const int ret = h_do_send_unix_domain_msg(path, p_error, mmc.m_p, mmc.m_size);
			free_make_message_context(&mmc);
			return ret;
		}

	}

	return -ENOMEM;
}

int send_app_msg(
	const char* p_sender_id,
	const char* p_destination,
	struct SocketError* p_error,
	const char* p_format,
	...)
{

	if((!p_sender_id) || (!(*p_sender_id)))
	{
		set_socket_error_msg(p_error, "p_sender_id is not set");
		return -EINVAL;
	}

	if((!p_destination) || (!(*p_destination)))
	{
		set_socket_error_msg(p_error, "p_destination is not set");
		return -EINVAL;
	}

	char prefix[256];
	const int prefix_len = snprintf(prefix, sizeof(prefix) - 1, "%s,%s,", p_sender_id, p_destination);
	prefix[sizeof(prefix) - 1] = '\0';

	if(prefix_len >= sizeof(prefix))
	{
		set_socket_error_msg(p_error, "sender id and destination are too long");
		return -ENOMEM;
	}

	if(prefix_len < 0)
	{
		set_socket_error_msg(p_error, strerror(errno));
		return prefix_len;
	}

	{
		const int initial_bufsize = 240;

		IF_MAKE_MESSAGE(initial_bufsize, prefix_len, p_format)
		{
			memcpy(mmc.m_p, prefix, prefix_len);
			const int ret = h_do_send_unix_domain_msg(TRULINK_SOCKET_DIR "/app-monitor-msg", p_error, mmc.m_p, mmc.m_size);
			free_make_message_context(&mmc);
			return ret;
		}

	}

	return -ENOMEM;
}

int wait_for_app_ready(const char* p_app_name)
{

	if(!p_app_name)
	{
		return -EINVAL;
	}

	struct ClientSocket cs;
	init_ClientSocket(&cs);

	{
		const int ret = connect_trulink_ud_client(&cs, "app-monitor-ready");

		if(ret)
		{
			close_ClientSocket(&cs);
			return -ret;
		}

	}

	{
		const int ret = send_cmd(cs.m_fd, MSG_NOSIGNAL, "%s\r", p_app_name);

		if(ret < 0)
		{
			close_ClientSocket(&cs);
			return ret;
		}

	}

	char c;
	const ssize_t ret = recv(cs.m_fd, &c, 1, 0);

	if(ret != 1)
	{
		close_ClientSocket(&cs);
		return -errno;
	}

	close_ClientSocket(&cs);
	return 1;
}

int signal_app_ready(const char* p_app_name)
{
	return send_trulink_ud_msg("app-monitor", 0, "ready \"%s\"\r", p_app_name);
}

int signal_app_unix_socket_ready(const char* p_app_name, const char* p_uds_name)
{
	return send_trulink_ud_msg("app-monitor", 0, "uds \"%s\" \"%s\"\r", p_app_name, p_uds_name);
}

int signal_app_tcp_socket_ready(const char* p_app_name, const char* p_sck_name)
{
	return send_trulink_ud_msg("app-monitor", 0, "sck \"%s\" \"%s\"\r", p_app_name, p_sck_name);
}
