/*
 * Copyright (c) 2003-2009 Untangle, Inc.
 * All rights reserved.
 *
 * This software is the confidential and proprietary information of
 * Untangle, Inc. ("Confidential Information"). You shall
 * not disclose such Confidential Information.
 *
 * $Id: unet.c 22141 2009-02-25 19:59:14Z amread $
 */

/* $Id: unet.c 22141 2009-02-25 19:59:14Z amread $ */
#include "mvutil/unet.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>


#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include "mvutil/errlog.h"
#include "mvutil/uthread.h"
#include "mvutil/debug.h"

#define __QUEUE_LENGTH 2048
#define START_PORT 9500

#define PORT_RANGE_ATTEMPTS 5
#define LISTEN_ATTEMPTS 5

#define ENABLE_BLOCKING 0xDABBA
#define NON_BLOCKING_FLAGS   O_NDELAY | O_NONBLOCK

static struct {
    pthread_key_t tls_key;
} _unet = {
    .tls_key = -1
};

typedef struct {
    int current;
    char buf_array[NTOA_BUF_COUNT][INET_ADDRSTRLEN];
} unet_tls_t;

typedef u_char  u8;
typedef u_short u16;
typedef u_long  u32;

static u_short next_port_tcp = START_PORT;
static u_short next_port_udp = START_PORT;


static void _close_socks( int count, int* fds );

static int _unet_startlisten (struct sockaddr_in* addr);

static u16 _unet_sum_calc ( u16 len, u8 src_addr[],u8 dest_addr[], u8 buff[], u8 proto );

static __inline__ int _unet_blocking_modify( int fd, int if_blocking )
{
    int flags;

    if ( fd < 0 ) return errlog( ERR_CRITICAL, "Invalid FD: %d\n", fd );

    if (( flags = fcntl( fd, F_GETFL )) < 0 ) return perrlog( "fcntl" );

    if ( if_blocking == ENABLE_BLOCKING ) flags &= ~( NON_BLOCKING_FLAGS );
    else                                  flags |= NON_BLOCKING_FLAGS;

    if ( fcntl( fd, F_SETFL, flags ) < 0 ) return perrlog( "fcntl" );

    return 0;
}

static unet_tls_t* _tls_get ( void );
static int         _tls_init( void* buf, size_t size );


/* this is definitely not where this belongs */
static char* _trim( char* buf );


static int _add_range( char* next_matcher, char* divider, struct unet_ip_matcher* ip_matcher );
static int _add_subnet( char* next_matcher, char* divider, struct unet_ip_matcher* ip_matcher );

int     unet_init        ( void )
{
    if ( pthread_key_create( &_unet.tls_key, uthread_tls_free ) < 0 ) {
        return perrlog( "pthread_key_create\n" );
    }

    return 0;
}

int     unet_startlisten (u_short listenport)
{
    struct sockaddr_in listen_addr;

    memset(&listen_addr,0,sizeof(listen_addr));
    listen_addr.sin_port = htons(listenport);
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    return _unet_startlisten(&listen_addr);
}

int     unet_startlisten_addr(u_short listenport, struct in_addr* bind_addr )
{
    struct sockaddr_in listen_addr;

    memset(&listen_addr,0,sizeof(listen_addr));
    listen_addr.sin_port = htons(listenport);
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = bind_addr->s_addr;

    return _unet_startlisten(&listen_addr);
}

int     unet_startlisten_local (u_short listenport)
{
    struct sockaddr_in listen_addr;

    memset(&listen_addr,0,sizeof(listen_addr));
    listen_addr.sin_port = htons(listenport);
    listen_addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, "127.0.0.1", &listen_addr)<0)
        return -1;

    return _unet_startlisten(&listen_addr);
}

int     unet_startlisten_udp (u_short listenport)
{
    int listen_sock=0;

    struct sockaddr_in listen_addr;

    memset(&listen_addr,0,sizeof(listen_addr));
    listen_addr.sin_port = htons(listenport);
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if ((listen_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP )) < 0)
        return -1;

    if (bind(listen_sock, (struct sockaddr*)&listen_addr, sizeof(listen_addr))<0)
        return -1;

    return listen_sock;
}

/* Opens up a consecutive range of ports for a TCP connection, if unable to open up a consecutive
 * range after a 5 attempts, return an error */
int     unet_startlisten_on_portrange(  int count, u_short* base_port, int* socks, char* ip )
{
    int c;

    if ( count < 1 || base_port == NULL || socks == NULL )
        return errlogargs();

    struct in_addr bind_addr = {
        .s_addr = htonl( INADDR_ANY )
    };

    if ( ip != NULL ) {
        if ( inet_pton( AF_INET, ip, &bind_addr ) < 0 ) {
            return errlog( ERR_CRITICAL, "Unable to convert ip '%s' (inet_pton)", ip );
        }
    }

    for ( c = 0 ; c < count ; c++ )
        socks[c] = -1;

    for ( c = 0 ; c < PORT_RANGE_ATTEMPTS; c++ ) {
        int d;
        u_short port;

        /* Get the first port */
        if ( unet_startlisten_on_anyport_tcp( base_port, &socks[0], &bind_addr )) {
            return errlog( ERR_CRITICAL, "unet_startlisten_on_anyport_tcp\n" );
        }

        for ( d = 1 ; d < count ; d++ ) {
            if ( unet_startlisten_on_anyport_tcp( &port, &socks[d], &bind_addr )) {
                _close_socks( d, socks );
                return errlog( ERR_CRITICAL, "unet_startlisten_on_anyport_tcp\n" );
            }

            if ( port == ( *base_port + d ))
                continue;

            _close_socks( d, socks );
            break;
        }
        if ( d == count ) return 0;
    }

    return errlog( ERR_CRITICAL, "Unable to open %d consecutive ports in %d attempts\n", count, c );
}

static void _close_socks( int count, int* socks )
{
    int c;

    for ( c = 0 ; c < count ; c++ ) {
        if ( socks[c] > 0 && close( socks[c] ))
            perrlog( "close" );
    }
}


int     unet_startlisten_on_anyport_tcp( u_short* port, int* fd, struct in_addr* bind_addr )
{
    *fd = -1;
    *port = next_port_tcp++;
    int attempts = LISTEN_ATTEMPTS;

    if (next_port_tcp < 2048 || next_port_tcp > 65000)
        next_port_tcp = START_PORT;

    while (*fd == -1) {

        if ( attempts -- < 0 ) {
            return errlog( ERR_CRITICAL, "Unable to open a port in %d attempts", LISTEN_ATTEMPTS );
        }

        if ((*fd = unet_startlisten_addr(*port, bind_addr ))<0) {

            if (errno == EINVAL  || errno == EADDRINUSE) {
                *port = *port + 1;
                if (*port > 65000)
                    *port = START_PORT;
                *fd = -1;
                continue;
            }
            else
                return perrlog("unet_startlisten");
        }
        else break;
    }

    return 0;
}

int     unet_startlisten_on_anyport_udp (u_short* port, int* fd)
{
    *fd = -1;
    *port = next_port_udp++;

    if (next_port_udp < 2048 || next_port_udp > 65000)
        next_port_udp = START_PORT;

    while (*fd == -1) {

        if ((*fd = unet_startlisten_udp(*port))<0) {

            if (errno == EINVAL  || errno == EADDRINUSE) {
                *port = *port + 1;
                if (*port>65000) *port = START_PORT;
                *fd = -1;
                continue;
            }
            else
                return perrlog("unet_startlisten_udp");
        }
        else break;
    }

    return 0;
}

int     unet_accept (int listensocket)
{
    int tmplen = sizeof(struct sockaddr_in);
    int session_socket=0;
    struct sockaddr_in tmpaddr;

    session_socket = accept(listensocket,
                            (struct sockaddr *)&tmpaddr,
                            (unsigned int*)&tmplen);

    return session_socket;
}

int     unet_open (in_addr_t* destaddr, u_short destport)
{
    int newsocket=-1;
    struct sockaddr_in out_addr;

    if ((newsocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))<0)
        return -1;

    out_addr.sin_family = AF_INET;
    out_addr.sin_port   = htons(destport);
    memcpy(&out_addr.sin_addr,destaddr,sizeof(in_addr_t));

    if (connect(newsocket, (struct sockaddr*)&out_addr, sizeof(out_addr))<0)
        return -1;

    return newsocket;
}

int     unet_readln (int fd, char * buf, int bufsize, int* retval)
{
    int position=0;
    char c;
    int ret=-1;

    if (fd < 0) {
        errno = EINVAL;
        return errlogargs();
    }

    while(position < bufsize - 1) {

        ret = read(fd,&c,1);

        if (ret <  0) {*retval = position; return ret;}
        if (ret == 0) {*retval = position; return ret;}

        buf[position] = c;
        position++;

        if (c == '\n') break;
    }

    buf[position] = '\0';

    *retval = position;
    return 0;
}

ssize_t unet_read_timeout (int fd, void* buf, size_t count, int millitime)
{
    struct pollfd fds[1];
    int n;

    fds[0].fd      = fd;
    fds[0].events  = POLLIN;
    fds[0].revents = 0;

    if ((n = poll(fds,1,millitime))<0) return -1;

    if (n == 0) return -2;

    if (fds[0].revents & POLLHUP)  return 0;
    if (fds[0].revents & POLLERR)  return -1;
    if (fds[0].revents & POLLNVAL) return -1;

    /* else POLLIN */
    return read(fd,buf,count);
}

ssize_t unet_read_loop (int fd, void* buf, size_t* count, int numloop)
{
    int num_read = 0;
    int n,i;

    if (fd < 0) {
        errno = EINVAL;
        return errlogargs();
    }

    for(num_read=0,i=0 ; num_read<*count && i<numloop ; i++) {
        if ((n=read(fd,buf+num_read,*count-num_read))<0) {
            *count = num_read;
            return perrlog("read");
        }
        if (n == 0) {
            *count = num_read;
            return 0;
        }
        num_read += n;
    }

    if (num_read<*count && i >= numloop)
        errlog(ERR_WARNING,"loop expired\n");

    *count = num_read;
    return 0;
}

ssize_t unet_write_loop (int fd, void* buf, size_t* count, int numloop)
{
    int num_write = 0;
    int n,i;

    if (fd < 0) {
        errno = EINVAL;
        return errlogargs();
    }

    for(num_write=0,i=0 ; num_write<*count && i<numloop ; i++) {
        if ((n=write(fd,buf+num_write,*count-num_write))<0) {
            *count = num_write;
            return perrlog("write");
        }
        num_write += n;
    }

    if (num_write<*count && i >= numloop)
        errlog(ERR_WARNING,"loop expired\n");

    *count = num_write;
    return 0;
}

int     unet_poll_dump (struct pollfd* fdset, int size)
{
    int i;
    for(i=0;i<size;i++) {
        if (fdset[i].revents)  {
            errlog(ERR_WARNING,
                   "fdset[%i] = 0x%08x  events: POLLIN:%i POLLPRI:%i POLLOUT:%i POLLERR:%i POLLHUP:%i POLLNVAL:%i \n",
                   i, fdset[i].revents,        fdset[i].revents & POLLIN,
                   fdset[i].revents & POLLPRI, fdset[i].revents & POLLOUT,
                   fdset[i].revents & POLLERR, fdset[i].revents & POLLHUP,
                   fdset[i].revents & POLLNVAL);
        }
    }

    return 0;
}

/**
 * Just reset the connection, but this doesn't close the fd
 */
int     unet_reset ( int fd )
{
    struct linger l = {1,0};

    if ( setsockopt( fd, SOL_SOCKET, SO_LINGER, &l, sizeof( struct linger )) < 0 ) {
        return perrlog("setsockopt");
    }

    return 0;
}

int     unet_reset_and_close (int fd)
{
    if ( unet_reset( fd )  < 0 )
        return errlog( ERR_CRITICAL, "unet_reset" );

    if (close(fd)<0)
        return perrlog("close");

    return 0;
}

/**
 * Close a file descriptor and set its value to negative one.
 */
int     unet_close( int* fd_ptr )
{
    if ( fd_ptr == NULL ) return errlogargs();

    int fd  = *fd_ptr;
    *fd_ptr = -1;

    if (( fd > 0 ) && ( close( fd ) < 0 )) return perrlog( "close" );

    return 0;
}

void    unet_reset_inet_ntoa( void )
{
    unet_tls_t* tls;

    if (( tls = _tls_get()) == NULL ) {
        errlog( ERR_CRITICAL, "_tls_get\n" );
        return;
    }

    tls->current = 0;
}

char*   unet_inet_ntoa (in_addr_t addr)
{
    return unet_next_inet_ntoa( addr );
}

char*   unet_next_inet_ntoa( in_addr_t addr )
{
    unet_tls_t* tls;

    if (( tls = _tls_get()) == NULL ) return errlog_null( ERR_CRITICAL, "_tls_get\n" );

    struct in_addr i;
    memset(&i, 0, sizeof(i));
    i.s_addr = addr;

    if ( tls->current >= NTOA_BUF_COUNT ) {
        debug( 10, "UNET: Cycled buffer\n" );
        tls->current = 0;
    }

    strncpy( tls->buf_array[tls->current], inet_ntoa( i ), INET_ADDRSTRLEN );

    /* Increment after using */
    return tls->buf_array[tls->current++];
}

static unet_tls_t* _tls_get( void )
{
    unet_tls_t* tls = NULL;

    if (( tls = uthread_tls_get( _unet.tls_key, sizeof( unet_tls_t ), _tls_init )) == NULL ) {
        return errlog_null( ERR_CRITICAL, "uthread_get_tls\n" );
    }

    return tls;
}

static int         _tls_init( void* buf, size_t size )
{
    unet_tls_t* tls = buf;

    if (( size != sizeof( unet_tls_t )) || ( tls == NULL )) return errlogargs();

    /* Initialize to zero */
    tls->current = 0;

    return 0;
}

u16     unet_in_cksum ( u16* addr, int len)
{
    int nleft = len;
    u_int16_t *w = addr;
    u_int32_t sum = 0;
    u_int16_t answer = 0;

    /*
     * Our algorithm is simple, using a 32 bit accumulator (sum), we add
     * sequential 16 bit words to it, and at the end, fold back all the
     * carry bits from the top 16 bits into the lower 16 bits.
     */
    while (nleft > 1)  {
        sum += *w++;
        nleft -= 2;
    }

    /* mop up an odd byte, if necessary */
    if (nleft == 1) {
        answer=0;
        *(u_char *)(&answer) = *(u_char *)w ;
        sum += answer;
    }

    /* add back carry outs from top 16 bits to low 16 bits */
    sum = (sum >> 16) + (sum & 0xffff); /* add hi 16 to low 16 */
    sum += (sum >> 16);         /* add carry */
    answer = ~sum;              /* truncate to 16 bits */
    return(answer);
}

u16     unet_udp_sum_calc ( u16 len_udp, u8 src_addr[],u8 dest_addr[], u8 buff[] )
{
    return _unet_sum_calc( len_udp, src_addr, dest_addr, buff, IPPROTO_UDP );
}

u16     unet_tcp_sum_calc ( u16 len_tcp, u8 src_addr[],u8 dest_addr[], u8 buff[] )
{
    return _unet_sum_calc( len_tcp, src_addr, dest_addr, buff, IPPROTO_TCP );
}

static u16 _unet_sum_calc ( u16 len, u8 src_addr[],u8 dest_addr[], u8 buff[], u8 proto )
{
    u16 word16;
    u32 sum;
    int i;

    sum=0;

    /* Handle the case where the length is odd */
    for ( i = 0 ; i < ( len & (~1)) ; i += 2 ) {
        word16 =((buff[i]<<8)&0xFF00)+(buff[i+1]&0xFF);
        sum = sum + word16;
    }

    /* If it is an odd length, pad properly */
    if ( len & 1 ) {
        word16 = ( buff[i] << 8) & 0xFF00;
        sum+= word16;
    }

    for (i=0;i<4;i=i+2){
        word16 =((src_addr[i]<<8)&0xFF00)+(src_addr[i+1]&0xFF);
        sum = sum + word16;
    }
    for (i=0;i<4;i=i+2){
        word16 =((dest_addr[i]<<8)&0xFF00)+(dest_addr[i+1]&0xFF);
        sum = sum + word16;
    }
    sum = sum + proto + len;

    while (sum>>16)
        sum = (sum & 0xFFFF)+(sum >> 16);

    sum = ~sum;

    return htons((u16) sum);
}

static int _unet_startlisten (struct sockaddr_in* addr)
{
    int one         = 1;
    int listen_sock = 0;

    if ((listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))<0)
        return -1;

    setsockopt(listen_sock, SOL_SOCKET,SO_REUSEADDR,(char *)&one,sizeof(one));

    if (bind(listen_sock, (struct sockaddr*)addr, sizeof(struct sockaddr))<0)
        return -1;

    if (listen(listen_sock, __QUEUE_LENGTH) < 0)
        return -1;

    return listen_sock;
}

/**
 * Disable blocking on a filedescriptor
 */
int unet_blocking_disable( int fd )
{
    return _unet_blocking_modify( fd, ~ENABLE_BLOCKING );
}

/**
 * Enable blocking on a filedescriptor
 */
int unet_blocking_enable( int fd )
{
    return _unet_blocking_modify( fd, ENABLE_BLOCKING );
}

/**
 * Initialize a sockaddr structure
 */
int unet_sockaddr_in_init( struct sockaddr_in* sockaddr, in_addr_t host, u_short port )
{
    if ( sockaddr == NULL ) return errlogargs();

    sockaddr->sin_family = AF_INET;
    sockaddr->sin_port   = htons( port );
    memcpy( &sockaddr->sin_addr, &host, sizeof(in_addr_t));

    return 0;
}

unet_ip_matchers_t* unet_ip_matchers_malloc( void )
{
    unet_ip_matchers_t* ip_matchers = NULL;
    if (( ip_matchers = calloc( 1, sizeof( unet_ip_matchers_t ))) == NULL ) {
        return errlogmalloc_null();
    }

    return ip_matchers;
}

int unet_ip_matchers_init( unet_ip_matchers_t* ip_matchers, char* matcher_string )
{
    if ( matcher_string == NULL ) return errlogargs();
    if ( ip_matchers == NULL ) return errlogargs();

    int string_len = 0;
    int num_matchers = 0;
    char *matcher_string_copy = NULL;

    if ( ip_matchers->matchers != NULL ) {
        errlog( ERR_WARNING, "Ignoring existing non-null matchers\n" );
        ip_matchers->matchers = NULL;
    }
    int _critical_section()
    {
        if (( string_len = strnlen( matcher_string, UNET_IP_MATCHERS_MAX_LEN )) < 0 ) {
            return errlog( ERR_CRITICAL, "Matcher string is longer than %d.\n", UNET_IP_MATCHERS_MAX_LEN );
        }
        
        /* Given a string that long, the largest number of matches is length/10,
         * that would be 1.2.3.1,1.2.3.2,.. */
        num_matchers = string_len / 8 + 1;
        
        if (( ip_matchers->matchers = calloc( num_matchers, sizeof( struct unet_ip_matcher ))) == NULL ) {
            return errlogmalloc();
        }
        
        ip_matchers->num_matchers = 0;
        
        /* Now it is time to parse the matcher string. */
        char* saveptr = NULL;
        if (( matcher_string_copy = strndup( matcher_string, string_len )) == NULL ) {
            if ( errno == ENOMEM ) {
                return errlogmalloc();
            } else {
                return perrlog( "strndup" );
            }
        }

        char* matcher_string_ptr = matcher_string_copy;
        int c = 0;
        struct unet_ip_matcher* ip_matcher = NULL;
        char *next_matcher;
        struct in_addr address;

        while (( next_matcher = strtok_r( matcher_string_ptr, ",", &saveptr )) != NULL ) {
            matcher_string_ptr = NULL;

            if ( ip_matchers->num_matchers >= num_matchers ) {
                return errlog( ERR_CRITICAL, "too many matchers: '%s'\n", matcher_string );
            }

            ip_matcher = &ip_matchers->matchers[ip_matchers->num_matchers++];

            ip_matcher->type = UNET_IP_MATCHERS_UNKNOWN;
            
            next_matcher = _trim( next_matcher );
            if ( next_matcher[0] == '\0' ) {
                return errlog( ERR_CRITICAL, "Invalid matcher string '%s'\n", matcher_string );
            }

            if (( strcasecmp( next_matcher, "any" ) == 0 ) ||
                ( strcasecmp( next_matcher, "*" ) == 0 )) {
                ip_matcher->type = UNET_IP_MATCHERS_ANY;
                continue;
            }

            for ( c = 0 ; next_matcher[c] != '\0' ; c++ ) {
                if (( next_matcher[c] == '-' ) && 
                    ( _add_range( next_matcher, &next_matcher[c], ip_matcher ) < 0 )) {
                    return errlog( ERR_CRITICAL, "_add_range\n" );
                }

                if (( next_matcher[c] == '/' ) && 
                    ( _add_subnet( next_matcher, &next_matcher[c], ip_matcher ) < 0 )) {
                    return errlog( ERR_CRITICAL, "_add_subnet\n" );
                }
            }

            if ( ip_matcher->type != UNET_IP_MATCHERS_UNKNOWN ) {
                continue;
            }
            
            /* Must be an individual address */
            if ( inet_pton( AF_INET, next_matcher, &address ) == 0 ) {
                return errlog( ERR_CRITICAL, "inet_pton[%s]\n", next_matcher );
            }
            
            ip_matcher->type = UNET_IP_MATCHERS_SINGLE;
            ip_matcher->data.address = address.s_addr;
        }
        
        return 0;
    }
    
    int ret = _critical_section();
    if ( matcher_string_copy != NULL ) {
        free( matcher_string_copy );
    }

    if ( ret < 0 ) {
        if ( ip_matchers->matchers != NULL ) {
            free( ip_matchers->matchers );
        }
        ip_matchers->matchers = NULL;
        return errlog( ERR_CRITICAL, "_critical_section\n" );
    }

    return 0;
}

unet_ip_matchers_t* unet_ip_matchers_create( char* matcher_string )
{
    unet_ip_matchers_t* ip_matchers = NULL;
    
    if (( ip_matchers = unet_ip_matchers_malloc()) == NULL ) {
        return errlog_null( ERR_CRITICAL, "unet_ip_matchers_malloc\n" );
    }

    if ( unet_ip_matchers_init( ip_matchers, matcher_string ) < 0 ) {
        free( ip_matchers );
        return errlog_null( ERR_CRITICAL, "unet_ip_matchers_init\n" );
    }

    return ip_matchers;
}


int unet_ip_matchers_is_match( unet_ip_matchers_t* ip_matchers, in_addr_t address, int* is_match )
{
    if ( ip_matchers == NULL ) {
        return errlogargs();
    }

    if ( is_match == NULL ) {
        return errlogargs();
    }

    if ( ip_matchers->num_matchers < 0 ) {
        return errlogargs();
    }

    *is_match = 0;

    if ( ip_matchers->num_matchers == 0 ) {
        return 0;
    }

    int c = 0;
    struct unet_ip_matcher* ip_matcher = NULL;
    for ( c = 0 ; c < ip_matchers->num_matchers ; c++ ) {
        ip_matcher = &ip_matchers->matchers[c];
        switch ( ip_matcher->type ) {
        case UNET_IP_MATCHERS_ANY:
            *is_match = 1;
            break;
            
        case UNET_IP_MATCHERS_SINGLE:
            if ( ip_matcher->data.address == address ) {
                *is_match = 1;
            }
            break;

        case UNET_IP_MATCHERS_RANGE: {
            in_addr_t h = ntohl( address );
            if (( ip_matcher->data.range.start <= h ) && ( h <= ip_matcher->data.range.end )) {
                *is_match = 1;
            }
            break;
        }

        case UNET_IP_MATCHERS_SUBNET:
            if ( ip_matcher->data.subnet.network == (address & ip_matcher->data.subnet.netmask)) {
                *is_match = 1;
            }
            break;
            
        default:
            return errlog( ERR_CRITICAL, "Invalid IP Matcher type: %d\n", ip_matcher->type );
        }
        
        /* First match is okay */
        if ( *is_match != 0 ) break;
    }
    
    return 0;
}

int unet_ip_matchers_to_string( unet_ip_matchers_t* ip_matchers, char* buffer, int buffer_len )
{
    if ( ip_matchers == NULL ) {
        return errlogargs();
    }

    if ( ip_matchers->num_matchers < 0 ) {
        return errlogargs();
    }

    if ( buffer == NULL ) {
        return errlogargs();
    }

    if ( buffer_len <= 8 ) {
        return errlogargs();
    }

    int len = 0;
    int c = 0;
    char comma[] = ",";
    char *comma_str = "";
    for ( c = 0 ; c < ip_matchers->num_matchers ; c++ ) {
        struct unet_ip_matcher* ip_matcher = &ip_matchers->matchers[c];
        len = 0;
        switch ( ip_matcher->type ) {
        case UNET_IP_MATCHERS_ANY:
            len = snprintf( buffer, buffer_len, "%sany", comma_str );
            break;
            
        case UNET_IP_MATCHERS_SINGLE:
            len = snprintf( buffer, buffer_len, "%s%s", comma_str, 
                            unet_next_inet_ntoa( ip_matcher->data.address ));
            break;

        case UNET_IP_MATCHERS_RANGE:
            len = snprintf( buffer, buffer_len, "%s%s-%s", comma_str, 
                            unet_next_inet_ntoa( htonl( ip_matcher->data.range.start )),
                            unet_next_inet_ntoa( htonl( ip_matcher->data.range.end )));
            break;
            

        case UNET_IP_MATCHERS_SUBNET:
            len = snprintf( buffer, buffer_len, "%s%s/%s", comma_str, 
                            unet_next_inet_ntoa( htonl( ip_matcher->data.range.start )),
                            unet_next_inet_ntoa( htonl( ip_matcher->data.range.end )));
            break;
            
        default:
            errlog( ERR_WARNING, "Ignoring unknown matcher: %d\n", ip_matcher->type );
        }
        
        /* some badness somewheres */
        if ( len < 0 ) {
            break;
        }
        /* something was appended, start printing commas */
        if ( len != 0 ) {
            comma_str = comma;
        }
        buffer_len -= len;
        if ( buffer_len <= 0 ) {
            break;
        }
        buffer+=len;

        
    }

    return 0;
}



int unet_ip_matchers_raze( unet_ip_matchers_t* ip_matchers )
{
    if ( ip_matchers == NULL ) return errlogargs();

    if ( unet_ip_matchers_destroy( ip_matchers ) < 0 ) errlog( ERR_CRITICAL, "unet_ip_matchers_destroy\n" );
    if ( unet_ip_matchers_free( ip_matchers ) < 0 ) errlog( ERR_CRITICAL, "unet_ip_matchers_destroy\n" );
    
    return 0;
}

int unet_ip_matchers_destroy( unet_ip_matchers_t* ip_matchers )
{
    if ( ip_matchers == NULL ) return errlogargs();
    if ( ip_matchers->matchers != NULL ) {
        free( ip_matchers->matchers );
    }
    ip_matchers->matchers = NULL;
    return 0;
}

int unet_ip_matchers_free( unet_ip_matchers_t* ip_matchers )
{
    if ( ip_matchers == NULL ) return errlogargs();
    free( ip_matchers );
    return 0;
}

static char* _trim( char* buffer )
{
    int c=0;
    for ( c = 0 ; isspace(buffer[c]) ;c++ ) {}
    buffer = &buffer[c];
    if (( c = strnlen( buffer, UNET_IP_MATCHERS_MAX_LEN )) < 0 ) {
        return errlog_null( ERR_CRITICAL, "individual matcher string is longer than %d.\n", 
                            UNET_IP_MATCHERS_MAX_LEN );
    }

    for ( c-- ; ( c >=0 ) && isspace(buffer[c]) ; c-- ) {
        buffer[c] = '\0';
    }
    
    return buffer;
}

static int _add_range( char* next_matcher, char* divider, struct unet_ip_matcher* ip_matcher )
{
    divider[0] = '\0';
    divider++;

    if (( next_matcher = _trim( next_matcher )) == NULL ) {
        return errlog( ERR_CRITICAL, "_trim\n" );
    }
    
    if (( divider = _trim( divider )) == NULL ) {
        return errlog( ERR_CRITICAL, "_trim\n" );
    }
    
    struct in_addr address;

    if ( inet_pton( AF_INET, next_matcher, &address ) == 0 ) {
        return errlog( ERR_CRITICAL, "inet_pton[%s]\n", next_matcher );
    }
    ip_matcher->data.range.start = ntohl( address.s_addr );

    if ( inet_pton( AF_INET, divider, &address ) == 0 ) {
        return errlog( ERR_CRITICAL, "inet_pton[%s]\n", divider );
    }
    ip_matcher->data.range.end = ntohl( address.s_addr );
    if ( ip_matcher->data.range.end < ip_matcher->data.range.start ) {
        ip_matcher->data.range.end = ip_matcher->data.range.start;
        ip_matcher->data.range.start = ntohl( address.s_addr );
    }

    ip_matcher->type = UNET_IP_MATCHERS_RANGE;

    return 0;
}

static int _add_subnet( char* next_matcher, char* divider, struct unet_ip_matcher* ip_matcher )
{
    divider[0] = '\0';
    divider++;

    if (( next_matcher = _trim( next_matcher )) == NULL ) {
        return errlog( ERR_CRITICAL, "_trim\n" );
    }
    
    if (( divider = _trim( divider )) == NULL ) {
        return errlog( ERR_CRITICAL, "_trim\n" );
    }
    
    struct in_addr address;

    if ( inet_pton( AF_INET, next_matcher, &address ) == 0 ) {
        return errlog( ERR_CRITICAL, "inet_pton[%s]\n", next_matcher );
    }
    ip_matcher->data.subnet.network = address.s_addr;

    if ( divider[0] == '\0' ) {
        return errlog( ERR_CRITICAL, "empty divider\n" );
    }

    char *endptr = NULL;
    int netmask = strtol( divider, &endptr, 10 );
    if ( endptr != NULL && endptr[0] == '\0' && netmask >= 0 && netmask <= 32 ) {
        if ( netmask == 0 ) {
            ip_matcher->data.subnet.netmask = 0;
        } else {
            ip_matcher->data.subnet.netmask = htonl( 0xFFFFFFFF << ( 32 - netmask ));
        }
    } else {
        if ( inet_pton( AF_INET, divider, &address ) == 0 ) {
            return errlog( ERR_CRITICAL, "inet_pton[%s]\n", divider );
        }
        ip_matcher->data.subnet.netmask = address.s_addr;
    }
    ip_matcher->data.subnet.network &= ip_matcher->data.subnet.netmask;    
    ip_matcher->type = UNET_IP_MATCHERS_SUBNET;

    return 0;
}
