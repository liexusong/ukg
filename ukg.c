/*
 * Copyright (c) 2012 - 2013, YukChung Lee <liexusong@qq.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <getopt.h>

#include "ae.h"


typedef struct {
    int sock;
    short port;
    int daemon;
    aeEventLoop *evloop;

    int worker_id;
    int datacenter_id;
    long sequence;
    long long last_timestamp;
    long long twepoch;

    unsigned char worker_id_bits;
    unsigned char datacenter_id_bits;
    unsigned char sequence_bits;

    int worker_id_shift;
    int datacenter_id_shift;
    int timestamp_left_shift;
    int sequence_mask;
} ukg_context_t;


typedef struct {
    int sock;
    int phase;
    char sndbuf[32];
    char *sndptr;
    char *sndend;
    void *next;
} ukg_connection_t;


#define UKG_GENERATE_UKEY  1
#define UKG_SENDTO_CLIENT  2

#define UKG_MAX_FREE_CONNECTIONS 1000


static ukg_context_t  g_context;
static ukg_context_t *g_ctx = &g_context;
static ukg_connection_t *g_free_connections = NULL;
static int g_free_connections_count = 0;


int ukg_set_nonblocking(int fd)
{
    int flags;

    if ((flags = fcntl(fd, F_GETFL, 0)) < 0 ||
         fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        return -1;
    return 0;
}


void ukg_daemonize(void)
{
    int fd;

    if (fork() != 0) exit(0);

    setsid();

    if ((fd = open("/dev/null", O_RDWR, 0)) != -1) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > STDERR_FILENO) close(fd);
    }
}


ukg_connection_t *ukg_get_connection()
{
    ukg_connection_t *conn;

    if (g_free_connections_count > 0) {
        conn = g_free_connections;
        g_free_connections = conn->next;
        g_free_connections_count--;
    } else {
        conn = (ukg_connection_t *)malloc(sizeof(*conn));
    }

    return conn;
}


void ukg_free_connection(ukg_connection_t *conn)
{
    if (g_free_connections_count < UKG_MAX_FREE_CONNECTIONS) {
        conn->next = g_free_connections;
        g_free_connections = conn;
        g_free_connections_count++;
    } else {
        free(conn);
    }
}


static long long ukg_really_time()
{
    struct timeval tv;
    long long retval;

    if (gettimeofday(&tv, NULL) == -1) {
        return 0LL;
    }

    retval = (long long)tv.tv_sec * 1000ULL + 
             (long long)tv.tv_usec / 1000ULL;

    return retval;
}


long long ukg_next_id(ukg_context_t *ctx)
{
    long long timestamp = ukg_really_time();

    if (timestamp == 0LL) { /* function called error */
        return -1LL;
    }

    if (ctx->last_timestamp == timestamp) {
        ctx->sequence = (ctx->sequence + 1) & ctx->sequence_mask;
        if (ctx->sequence == 0) { /* this cycle deplete, wait for next */
            return -1LL;
        }

    } else {
        ctx->sequence = 0;
    }

    ctx->last_timestamp = timestamp;

    return ((timestamp - ctx->twepoch) << ctx->timestamp_left_shift) |
            (ctx->datacenter_id << ctx->datacenter_id_shift) |
            (ctx->worker_id << ctx->worker_id_shift) |
             ctx->sequence;
}


void ukg_sendto_callback(aeEventLoop *ev, int fd, void *data, int mask)
{
    ukg_connection_t *conn = (ukg_connection_t *)data;
    long long unique_id;
    int ssize, wsize;

    switch (conn->phase)
    {
    case UKG_GENERATE_UKEY:

        unique_id = ukg_next_id(g_ctx);
        if (unique_id == -1LL) { /* wati until return not equals -1 */
            break;
        }

        ssize = sprintf(conn->sndbuf, "%lld\r\n", unique_id);

        conn->sndptr = conn->sndbuf;
        conn->sndend = conn->sndbuf + ssize;
        conn->phase = UKG_SENDTO_CLIENT;

        /* continue */

    case UKG_SENDTO_CLIENT:

        wsize = conn->sndend - conn->sndptr;
        ssize = send(conn->sock, conn->sndptr, wsize, 0);

        /* error or closed or finish */
        if (ssize <= 0 || ssize == wsize) {
            aeDeleteFileEvent(g_ctx->evloop, conn->sock, AE_WRITABLE);
            close(conn->sock);
            ukg_free_connection(conn);
            break;
        }

        conn->sndptr += ssize;

        break;
    }

    return;
}


void ukg_accept_callback(aeEventLoop *ev, int fd, void *data, int mask)
{
    int sock;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    ukg_connection_t *conn;

    sock = accept(fd, (struct sockaddr *)&address, &addrlen);
    if (sock == -1) {
        return;
    }

    /* set socket to nonblocking mode */
    if (ukg_set_nonblocking(sock) == -1) {
        return;
    }

    conn = ukg_get_connection();
    if (conn == NULL) {
        close(sock);
        return;
    }

    conn->sock = sock;
    conn->phase = UKG_GENERATE_UKEY;

    aeCreateFileEvent(g_ctx->evloop, conn->sock, AE_WRITABLE, 
                                                     ukg_sendto_callback, conn);
    return;
}


int ukg_server_startup()
{
    struct sockaddr_in address;

    g_ctx->evloop = aeCreateEventLoop();
    if (!g_ctx) {
        fprintf(stderr, "Failed to create EventLoop\n");
        goto failed;
    }

    g_ctx->sock = socket(AF_INET, SOCK_STREAM, 0);
    if (g_ctx->sock == -1) {
        fprintf(stderr, "Failed to create socket\n");
        goto failed;
    }

    /* set socket to nonblocking mode */
    if (ukg_set_nonblocking(g_ctx->sock) == -1) {
        fprintf(stderr, "Failed to set socket to nonblocking\n");
        goto failed;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(g_ctx->port);

    if (bind(g_ctx->sock, (struct sockaddr*) &address, sizeof(address)) != 0) {
        fprintf(stderr, "Failed to binding to port\n");
        goto failed;
    }

    if (listen(g_ctx->sock, 1024) != 0) {
        fprintf(stderr, "Failed to listen on socket\n");
        goto failed;
    }

    aeCreateFileEvent(g_ctx->evloop, g_ctx->sock, AE_READABLE, 
                                                     ukg_accept_callback, NULL);
    return 0;


failed:

    if (g_ctx->evloop) {
        aeDeleteEventLoop(g_ctx->evloop);
    }

    if (g_ctx->sock != -1) {
        close(g_ctx->sock);
    }

    return -1;
}


void ukg_server_shutdown()
{
    aeDeleteEventLoop(g_ctx->evloop);
    close(g_ctx->sock);
    return;
}


void ukg_server_loop()
{
    aeMain(g_ctx->evloop);
}


/*
 * Default setting for server
 */
void ukg_default_setting()
{
    g_ctx->worker_id = 0;
    g_ctx->datacenter_id = 0;

    g_ctx->sequence = 0;
    g_ctx->last_timestamp = -1LL;

    g_ctx->twepoch = 1288834974657LL;
    g_ctx->worker_id_bits = 5;
    g_ctx->datacenter_id_bits = 5;
    g_ctx->sequence_bits = 12;

    g_ctx->worker_id_shift = g_ctx->sequence_bits;
    g_ctx->datacenter_id_shift = g_ctx->sequence_bits + g_ctx->worker_id_bits;
    g_ctx->timestamp_left_shift = g_ctx->sequence_bits + g_ctx->worker_id_bits
                                                    + g_ctx->datacenter_id_bits;
    g_ctx->sequence_mask = -1 ^ (-1 << g_ctx->sequence_bits);

    g_ctx->port = 7954;
}


void ukg_usage()
{
    printf("Unique Key Generate Server.\n\n"
           "Usage: ukg [options]\n"
           "Options:\n"
           "    -w, --worker        worker ID\n"
           "    -d, --datacenter    datacenter ID\n"
           "    -p, --port          port number\n"
           "    -D, --daemon        run at daemon mode\n");
}


static const struct option options[] = {
    {"worker",     2, NULL, 'w'},
    {"datacenter", 2, NULL, 'd'},
    {"port",       2, NULL, 'p'},
    {"daemon",     0, NULL, 'D'},
    {"help",       0, NULL, 'h'},
};

int main(int argc, char *argv[])
{
    int option, idx;

    memset(g_ctx, 0, sizeof(ukg_context_t));

    ukg_default_setting();

    while ((option = getopt_long(argc, argv, "w:d:p:Dh",
                                                       options, &idx)) != -1) {
        switch (option) {
            case 'w':
                g_ctx->worker_id = atoi(optarg);
                break;
            case 'd':
                g_ctx->datacenter_id = atoi(optarg);
                break;
            case 'p':
                g_ctx->port = atoi(optarg);
                break;
            case 'D':
                g_ctx->daemon = 1;
                break;
            case 'h':
            case '?':
                ukg_usage();
                exit(0);
        }
    }

    /* daemonize mode */
    if (g_ctx->daemon) {
        ukg_daemonize();
    }

    /* startup server */
    if (ukg_server_startup() == -1) {
        fprintf(stderr, "Failed to starting server\n");
        exit(1);
    }

    ukg_server_loop();     /* start main loop */
    ukg_server_shutdown(); /* shutdown server */

    return 0;
}

