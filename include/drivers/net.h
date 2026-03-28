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

#endif /* NET_H */
