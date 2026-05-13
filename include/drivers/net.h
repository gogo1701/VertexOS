#ifndef NET_H
#define NET_H

#include "types.h"

typedef struct {
    u8 mac[6];
    u32 ip;
    u32 subnet;
    u32 gateway;
    u32 dns;
    u8 link_up;
    u8 dhcp_configured;
} net_config;

typedef enum {
    NET_HTTP_OK = 0,
    NET_HTTP_ERR_INVALID_ARG,
    NET_HTTP_ERR_BAD_URL,
    NET_HTTP_ERR_UNSUPPORTED_SCHEME,
    NET_HTTP_ERR_LINK_DOWN,
    NET_HTTP_ERR_NO_IP,
    NET_HTTP_ERR_DNS_FAILED,
    NET_HTTP_ERR_TCP_UNAVAILABLE,
    NET_HTTP_ERR_TIMEOUT,
    NET_HTTP_ERR_PROTOCOL
} net_http_result;

typedef struct {
    net_http_result result;
    u32 resolved_ip;
    u16 port;
    u16 status_code;
    u32 body_len;
    u8 body_truncated;
} net_http_response;

void net_init(void);
void net_poll(void);
u8 net_is_ready(void);

const net_config* net_get_config(void);
void net_print_config(void);

u8 net_dhcp_request(void);
u8 net_ping(u32 target_ip, u32 timeout_ms, u32* out_rtt_ms);
u8 net_resolve_ipv4(const char* host, u32 timeout_ms, u32* out_ip);

u8 net_parse_ipv4(const char* text, u32* out_ip);
void net_format_ipv4(u32 ip, char* out, u32 out_size);

net_http_result net_http_get(const char* url,
                             u32 timeout_ms,
                             char* body_out,
                             u32 body_out_size,
                             net_http_response* out_response);

#endif /* NET_H */
