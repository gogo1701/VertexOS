#include "net.h"

#include "display.h"
#include "pit.h"
#include "rtl8139.h"

#define ETH_TYPE_ARP  0x0806u
#define ETH_TYPE_IPV4 0x0800u

#define IP_PROTO_ICMP 1u
#define IP_PROTO_TCP  6u
#define IP_PROTO_UDP  17u

#define DHCP_CLIENT_PORT 68u
#define DHCP_SERVER_PORT 67u
#define DNS_CLIENT_PORT 43210u
#define DNS_SERVER_PORT 53u

#define ICMP_ECHO_REPLY 0u
#define ICMP_ECHO_REQUEST 8u

#define ARP_OP_REQUEST 1u
#define ARP_OP_REPLY   2u

#define NET_DEBUG 0

typedef struct {
    u8 dst[6];
    u8 src[6];
    u16 ethertype;
} __attribute__((packed)) eth_header;

typedef struct {
    u16 hw_type;
    u16 proto_type;
    u8 hw_len;
    u8 proto_len;
    u16 op;
    u8 sender_mac[6];
    u32 sender_ip;
    u8 target_mac[6];
    u32 target_ip;
} __attribute__((packed)) arp_packet;

typedef struct {
    u8 version_ihl;
    u8 dscp_ecn;
    u16 total_len;
    u16 ident;
    u16 flags_frag;
    u8 ttl;
    u8 proto;
    u16 checksum;
    u32 src_ip;
    u32 dst_ip;
} __attribute__((packed)) ipv4_header;

typedef struct {
    u8 type;
    u8 code;
    u16 checksum;
    u16 ident;
    u16 seq;
} __attribute__((packed)) icmp_echo;

typedef struct {
    u16 src_port;
    u16 dst_port;
    u16 len;
    u16 checksum;
} __attribute__((packed)) udp_header;

typedef struct {
    u16 src_port;
    u16 dst_port;
    u32 seq_num;
    u32 ack_num;
    u8 data_offset_reserved;
    u8 flags;
    u16 window;
    u16 checksum;
    u16 urgent_ptr;
} __attribute__((packed)) tcp_header;

typedef struct {
    u16 id;
    u16 flags;
    u16 qdcount;
    u16 ancount;
    u16 nscount;
    u16 arcount;
} __attribute__((packed)) dns_header;

typedef struct {
    u8 op;
    u8 htype;
    u8 hlen;
    u8 hops;
    u32 xid;
    u16 secs;
    u16 flags;
    u32 ciaddr;
    u32 yiaddr;
    u32 siaddr;
    u32 giaddr;
    u8 chaddr[16];
    u8 sname[64];
    u8 file[128];
    u32 cookie;
    u8 options[312];
} __attribute__((packed)) dhcp_packet;

static net_config cfg;

static struct {
    u8 valid;
    u32 ip;
    u8 mac[6];
} arp_cache;

static struct {
    u8 waiting;
    u16 ident;
    u16 seq;
    u32 target;
    u32 sent_ticks;
    u8 got_reply;
    u32 rtt_ms;
} ping_state;

static struct {
    u8 waiting_offer;
    u8 waiting_ack;
    u32 xid;
    u32 offered_ip;
    u32 server_ip;
    u32 subnet;
    u32 router;
    u32 dns;
    u32 lease_end_ticks;
} dhcp_state;

static struct {
    u8 waiting;
    u8 got_reply;
    u16 xid;
    u32 result_ip;
} dns_state;

static u8 ipv4_tx_buf[1600];
static u8 dhcp_body_buf[576];
static u8 dhcp_udp_buf[700];
static u8 dhcp_ip_buf[700];
static u8 eth_tx_frame_buf[1600];
static u8 dns_query_buf[512];
static u8 udp_tx_buf[1600];
static u8 tcp_tx_buf[1600];

#define TCP_FLAG_FIN 0x01u
#define TCP_FLAG_SYN 0x02u
#define TCP_FLAG_RST 0x04u
#define TCP_FLAG_PSH 0x08u
#define TCP_FLAG_ACK 0x10u

typedef struct {
    u8 active;
    u8 connecting;
    u8 established;
    u8 failed;
    u8 fin_seen;
    u8 header_done;
    u8 body_truncated;
    u8 status_valid;
    u32 remote_ip;
    u16 local_port;
    u16 remote_port;
    u32 seq_local;
    u32 seq_remote;
    u32 connect_sent_ticks;
    u32 last_activity_ticks;
    u32 body_written;
    u32 body_total;
    u16 status_code;
    char* body_out;
    u32 body_out_size;
    u32 body_out_pos;
    char header_buf[1024];
    u32 header_len;
} tcp_http_session;

static tcp_http_session tcp_http;

static void dbg_print(const char* s) {
#if NET_DEBUG
    display_print(s);
#else
    (void)s;
#endif
}

static void dbg_print_u32(u32 v) {
#if NET_DEBUG
    display_print_num(v, 10);
#else
    (void)v;
#endif
}

static void dbg_print_ip(u32 ip) {
#if NET_DEBUG
    char buf[16];
    net_format_ipv4(ip, buf, sizeof(buf));
    display_print(buf);
#else
    (void)ip;
#endif
}

static u16 bswap16(u16 x) {
    return (u16)((x >> 8) | (x << 8));
}

static u32 bswap32(u32 x) {
    return (x >> 24) |
           ((x >> 8) & 0x0000FF00u) |
           ((x << 8) & 0x00FF0000u) |
           (x << 24);
}

static u16 htons(u16 x) { return bswap16(x); }
static u16 ntohs(u16 x) { return bswap16(x); }
static u32 htonl(u32 x) { return bswap32(x); }
static u32 ntohl(u32 x) { return bswap32(x); }

static void mem_copy(void* dst, const void* src, u32 len) {
    u8* d = (u8*)dst;
    const u8* s = (const u8*)src;
    u32 i;
    for (i = 0; i < len; i++) {
        d[i] = s[i];
    }
}

static void mem_set(void* dst, u8 value, u32 len) {
    u8* d = (u8*)dst;
    u32 i;
    for (i = 0; i < len; i++) {
        d[i] = value;
    }
}

static u8 mem_eq(const u8* a, const u8* b, u32 len) {
    u32 i;
    for (i = 0; i < len; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

static u16 checksum16(const void* data, u32 len) {
    const u8* p = (const u8*)data;
    u32 sum = 0;
    u32 i;

    for (i = 0; i + 1 < len; i += 2) {
        sum += (u32)((p[i] << 8) | p[i + 1]);
    }

    if (len & 1u) {
        sum += (u32)(p[len - 1] << 8);
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFFu) + (sum >> 16);
    }

    return (u16)(~sum);
}

static u32 checksum32_add(u32 sum, const u8* data, u32 len) {
    u32 i;

    for (i = 0; i + 1u < len; i += 2u) {
        sum += (u32)((data[i] << 8) | data[i + 1u]);
    }
    if (len & 1u) {
        sum += (u32)(data[len - 1u] << 8);
    }
    return sum;
}

static u16 tcp_checksum(u32 src_ip, u32 dst_ip, const u8* tcp_seg, u32 tcp_len) {
    u8 pseudo[12];
    u32 sum = 0;

    pseudo[0] = (u8)((src_ip >> 24) & 0xFFu);
    pseudo[1] = (u8)((src_ip >> 16) & 0xFFu);
    pseudo[2] = (u8)((src_ip >> 8) & 0xFFu);
    pseudo[3] = (u8)(src_ip & 0xFFu);
    pseudo[4] = (u8)((dst_ip >> 24) & 0xFFu);
    pseudo[5] = (u8)((dst_ip >> 16) & 0xFFu);
    pseudo[6] = (u8)((dst_ip >> 8) & 0xFFu);
    pseudo[7] = (u8)(dst_ip & 0xFFu);
    pseudo[8] = 0u;
    pseudo[9] = IP_PROTO_TCP;
    pseudo[10] = (u8)((tcp_len >> 8) & 0xFFu);
    pseudo[11] = (u8)(tcp_len & 0xFFu);

    sum = checksum32_add(sum, pseudo, sizeof(pseudo));
    sum = checksum32_add(sum, tcp_seg, tcp_len);

    while (sum >> 16) {
        sum = (sum & 0xFFFFu) + (sum >> 16);
    }
    return (u16)(~sum);
}

static u8 parse_u32(const char* s, u32* value, u32* consumed) {
    u32 v = 0;
    u32 i = 0;
    if (!s || s[0] < '0' || s[0] > '9') {
        return 0;
    }
    while (s[i] >= '0' && s[i] <= '9') {
        v = v * 10u + (u32)(s[i] - '0');
        i++;
        if (v > 255u) {
            return 0;
        }
    }
    *value = v;
    *consumed = i;
    return 1;
}

static u8 parse_u16_any(const char* s, u16* out_value, u32* consumed) {
    u32 v = 0;
    u32 i = 0;

    if (!s || s[0] < '0' || s[0] > '9') {
        return 0;
    }

    while (s[i] >= '0' && s[i] <= '9') {
        v = v * 10u + (u32)(s[i] - '0');
        if (v > 65535u) {
            return 0;
        }
        i++;
    }

    *out_value = (u16)v;
    if (consumed) {
        *consumed = i;
    }
    return 1;
}

static char ascii_lower(char c) {
    if (c >= 'A' && c <= 'Z') {
        return (char)(c + ('a' - 'A'));
    }
    return c;
}

static u8 starts_with_ci(const char* s, const char* prefix) {
    u32 i = 0;
    if (!s || !prefix) {
        return 0;
    }
    while (prefix[i]) {
        if (ascii_lower(s[i]) != ascii_lower(prefix[i])) {
            return 0;
        }
        i++;
    }
    return 1;
}

static net_http_result parse_http_url(const char* url,
                                      char* host_out,
                                      u32 host_out_size,
                                      u16* out_port,
                                      const char** out_path) {
    const char* p;
    const char* host_start;
    const char* scheme_sep = 0;
    u32 host_len = 0;
    u32 i;

    if (!url || !host_out || host_out_size == 0u || !out_port || !out_path) {
        return NET_HTTP_ERR_INVALID_ARG;
    }

    for (i = 0u; url[i]; i++) {
        if (url[i] == ':' && url[i + 1u] == '/' && url[i + 2u] == '/') {
            scheme_sep = &url[i];
            break;
        }
    }

    if (scheme_sep) {
        if (starts_with_ci(url, "https://")) {
            return NET_HTTP_ERR_UNSUPPORTED_SCHEME;
        }
        if (!starts_with_ci(url, "http://")) {
            return NET_HTTP_ERR_UNSUPPORTED_SCHEME;
        }
        p = url + 7;
    } else {
        /* Bare-host format is accepted: host[:port][/path]. */
        p = url;
    }

    host_start = p;
    while (*p && *p != ':' && *p != '/') {
        host_len++;
        p++;
    }

    if (host_len == 0u || host_len + 1u > host_out_size) {
        return NET_HTTP_ERR_BAD_URL;
    }

    for (i = 0u; i < host_len; i++) {
        host_out[i] = host_start[i];
    }
    host_out[host_len] = '\0';

    *out_port = 80u;
    if (*p == ':') {
        u16 port;
        u32 consumed = 0u;
        p++;
        if (!parse_u16_any(p, &port, &consumed) || consumed == 0u) {
            return NET_HTTP_ERR_BAD_URL;
        }
        *out_port = port;
        p += consumed;
    }

    if (*p == '\0') {
        *out_path = "/";
        return NET_HTTP_OK;
    }

    if (*p != '/') {
        return NET_HTTP_ERR_BAD_URL;
    }

    *out_path = p;
    return NET_HTTP_OK;
}

u8 net_parse_ipv4(const char* text, u32* out_ip) {
    u32 o0;
    u32 o1;
    u32 o2;
    u32 o3;
    u32 n;

    if (!parse_u32(text, &o0, &n) || text[n] != '.') {
        return 0;
    }
    text += n + 1;
    if (!parse_u32(text, &o1, &n) || text[n] != '.') {
        return 0;
    }
    text += n + 1;
    if (!parse_u32(text, &o2, &n) || text[n] != '.') {
        return 0;
    }
    text += n + 1;
    if (!parse_u32(text, &o3, &n) || text[n] != '\0') {
        return 0;
    }

    *out_ip = (o0 << 24) | (o1 << 16) | (o2 << 8) | o3;
    return 1;
}

static void u32_to_dec(char* out, u32 value) {
    char tmp[16];
    u32 i = 0;
    u32 j;
    if (value == 0) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }
    while (value > 0) {
        tmp[i++] = (char)('0' + (value % 10u));
        value /= 10u;
    }
    for (j = 0; j < i; j++) {
        out[j] = tmp[i - 1u - j];
    }
    out[i] = '\0';
}

void net_format_ipv4(u32 ip, char* out, u32 out_size) {
    char a[4];
    char b[4];
    char c[4];
    char d[4];
    u32 p = 0;
    u32 i;

    if (!out || out_size < 16) {
        return;
    }

    u32_to_dec(a, (ip >> 24) & 0xFFu);
    u32_to_dec(b, (ip >> 16) & 0xFFu);
    u32_to_dec(c, (ip >> 8) & 0xFFu);
    u32_to_dec(d, ip & 0xFFu);

    for (i = 0; a[i] && p + 1 < out_size; i++) out[p++] = a[i];
    out[p++] = '.';
    for (i = 0; b[i] && p + 1 < out_size; i++) out[p++] = b[i];
    out[p++] = '.';
    for (i = 0; c[i] && p + 1 < out_size; i++) out[p++] = c[i];
    out[p++] = '.';
    for (i = 0; d[i] && p + 1 < out_size; i++) out[p++] = d[i];
    out[p] = '\0';
}

static u8 send_frame(const u8* dst, u16 ethertype, const void* payload, u32 payload_len) {
    eth_header* eth = (eth_header*)eth_tx_frame_buf;
    u32 frame_len;

    if (payload_len > 1500u) {
        return 0;
    }

    mem_copy(eth->dst, dst, 6);
    mem_copy(eth->src, cfg.mac, 6);
    eth->ethertype = htons(ethertype);
    mem_copy(eth_tx_frame_buf + sizeof(eth_header), payload, payload_len);

    frame_len = sizeof(eth_header) + payload_len;
    if (frame_len < 60u) {
        mem_set(eth_tx_frame_buf + frame_len, 0, 60u - frame_len);
        frame_len = 60u;
    }

    return rtl8139_send(eth_tx_frame_buf, frame_len);
}

static void send_arp_request(u32 target_ip) {
    arp_packet p;
    u8 broadcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

    p.hw_type = htons(1);
    p.proto_type = htons(ETH_TYPE_IPV4);
    p.hw_len = 6;
    p.proto_len = 4;
    p.op = htons(ARP_OP_REQUEST);
    mem_copy(p.sender_mac, cfg.mac, 6);
    p.sender_ip = htonl(cfg.ip);
    mem_set(p.target_mac, 0, 6);
    p.target_ip = htonl(target_ip);

    dbg_print("net: arp request for ");
    dbg_print_ip(target_ip);
    dbg_print("\n");

    send_frame(broadcast, ETH_TYPE_ARP, &p, sizeof(p));
}

static void send_arp_reply(const u8* dst_mac, u32 dst_ip) {
    arp_packet p;

    p.hw_type = htons(1);
    p.proto_type = htons(ETH_TYPE_IPV4);
    p.hw_len = 6;
    p.proto_len = 4;
    p.op = htons(ARP_OP_REPLY);
    mem_copy(p.sender_mac, cfg.mac, 6);
    p.sender_ip = htonl(cfg.ip);
    mem_copy(p.target_mac, dst_mac, 6);
    p.target_ip = htonl(dst_ip);

    dbg_print("net: arp reply to ");
    dbg_print_ip(dst_ip);
    dbg_print("\n");

    send_frame(dst_mac, ETH_TYPE_ARP, &p, sizeof(p));
}

static u8 resolve_target_mac(u32 dst_ip, u8* out_mac) {
    u32 route_ip;
    u32 start;

    if (cfg.subnet != 0 && ((dst_ip & cfg.subnet) != (cfg.ip & cfg.subnet))) {
        route_ip = cfg.gateway;
    } else {
        route_ip = dst_ip;
    }

    if (arp_cache.valid && arp_cache.ip == route_ip) {
        mem_copy(out_mac, arp_cache.mac, 6);
        return 1;
    }

    send_arp_request(route_ip);

    start = pit_get_ticks();
    while ((pit_get_ticks() - start) < pit_get_frequency()) {
        net_poll();
        if (arp_cache.valid && arp_cache.ip == route_ip) {
            mem_copy(out_mac, arp_cache.mac, 6);
            return 1;
        }
    }

    return 0;
}

static u8 send_ipv4_packet(u32 dst_ip, u8 proto, const void* payload, u16 payload_len, u16 ident) {
    ipv4_header* ip = (ipv4_header*)ipv4_tx_buf;
    u8 dst_mac[6];

    if (!resolve_target_mac(dst_ip, dst_mac)) {
        return 0;
    }

    ip->version_ihl = 0x45;
    ip->dscp_ecn = 0;
    ip->total_len = htons((u16)(sizeof(ipv4_header) + payload_len));
    ip->ident = htons(ident);
    ip->flags_frag = htons(0x4000);
    ip->ttl = 64;
    ip->proto = proto;
    ip->checksum = 0;
    ip->src_ip = htonl(cfg.ip);
    ip->dst_ip = htonl(dst_ip);
    ip->checksum = htons(checksum16(ip, sizeof(ipv4_header)));

    mem_copy(ipv4_tx_buf + sizeof(ipv4_header), payload, payload_len);
    return send_frame(dst_mac, ETH_TYPE_IPV4, ipv4_tx_buf, sizeof(ipv4_header) + payload_len);
}

static u8 send_udp_packet(u32 dst_ip, u16 src_port, u16 dst_port, const void* payload, u16 payload_len, u16 ident) {
    udp_header* udp = (udp_header*)udp_tx_buf;

    if (sizeof(udp_header) + payload_len > sizeof(udp_tx_buf)) {
        return 0;
    }

    udp->src_port = htons(src_port);
    udp->dst_port = htons(dst_port);
    udp->len = htons((u16)(sizeof(udp_header) + payload_len));
    udp->checksum = 0;

    mem_copy(udp_tx_buf + sizeof(udp_header), payload, payload_len);
    return send_ipv4_packet(dst_ip, IP_PROTO_UDP, udp_tx_buf, (u16)(sizeof(udp_header) + payload_len), ident);
}

static u8 send_tcp_packet(u32 dst_ip,
                          u16 src_port,
                          u16 dst_port,
                          u32 seq,
                          u32 ack,
                          u8 flags,
                          const void* payload,
                          u16 payload_len,
                          u16 ident) {
    tcp_header* tcp = (tcp_header*)tcp_tx_buf;
    u32 seg_len;

    if (sizeof(tcp_header) + payload_len > sizeof(tcp_tx_buf)) {
        return 0;
    }

    tcp->src_port = htons(src_port);
    tcp->dst_port = htons(dst_port);
    tcp->seq_num = htonl(seq);
    tcp->ack_num = htonl(ack);
    tcp->data_offset_reserved = (u8)(5u << 4);
    tcp->flags = flags;
    tcp->window = htons(4096u);
    tcp->checksum = 0u;
    tcp->urgent_ptr = 0u;

    if (payload_len > 0u) {
        mem_copy(tcp_tx_buf + sizeof(tcp_header), payload, payload_len);
    }

    seg_len = sizeof(tcp_header) + payload_len;
    tcp->checksum = htons(tcp_checksum(cfg.ip, dst_ip, tcp_tx_buf, seg_len));

    return send_ipv4_packet(dst_ip, IP_PROTO_TCP, tcp_tx_buf, (u16)seg_len, ident);
}

static void tcp_http_reset(void) {
    mem_set(&tcp_http, 0, sizeof(tcp_http));
}

static void tcp_http_parse_status_line(void) {
    u32 i = 0u;

    while (i < tcp_http.header_len && tcp_http.header_buf[i] != ' ') {
        i++;
    }
    while (i < tcp_http.header_len && tcp_http.header_buf[i] == ' ') {
        i++;
    }

    if (i + 2u < tcp_http.header_len &&
        tcp_http.header_buf[i] >= '0' && tcp_http.header_buf[i] <= '9' &&
        tcp_http.header_buf[i + 1u] >= '0' && tcp_http.header_buf[i + 1u] <= '9' &&
        tcp_http.header_buf[i + 2u] >= '0' && tcp_http.header_buf[i + 2u] <= '9') {
        tcp_http.status_code = (u16)((tcp_http.header_buf[i] - '0') * 100u +
                                      (tcp_http.header_buf[i + 1u] - '0') * 10u +
                                      (tcp_http.header_buf[i + 2u] - '0'));
        tcp_http.status_valid = 1u;
    }
}

static void tcp_http_append_body(const u8* data, u32 len) {
    u32 i;

    tcp_http.body_total += len;
    for (i = 0u; i < len; i++) {
        if (tcp_http.body_out && tcp_http.body_out_size > 0u &&
            tcp_http.body_out_pos + 1u < tcp_http.body_out_size) {
            char c = (char)data[i];
            if ((u8)c < 32u && c != '\n' && c != '\r' && c != '\t') {
                c = '.';
            }
            tcp_http.body_out[tcp_http.body_out_pos++] = c;
        } else {
            tcp_http.body_truncated = 1u;
        }
    }

    if (tcp_http.body_out && tcp_http.body_out_size > 0u) {
        tcp_http.body_out[tcp_http.body_out_pos] = '\0';
    }
}

static void tcp_http_feed_data(const u8* data, u32 len) {
    u32 i;

    if (len == 0u) {
        return;
    }

    if (!tcp_http.header_done) {
        for (i = 0u; i < len; i++) {
            if (tcp_http.header_len + 1u < sizeof(tcp_http.header_buf)) {
                tcp_http.header_buf[tcp_http.header_len++] = (char)data[i];
                tcp_http.header_buf[tcp_http.header_len] = '\0';
            } else {
                tcp_http.failed = 1u;
                return;
            }

            if (tcp_http.header_len >= 4u) {
                u32 n = tcp_http.header_len;
                if (tcp_http.header_buf[n - 4u] == '\r' &&
                    tcp_http.header_buf[n - 3u] == '\n' &&
                    tcp_http.header_buf[n - 2u] == '\r' &&
                    tcp_http.header_buf[n - 1u] == '\n') {
                    tcp_http.header_done = 1u;
                    tcp_http_parse_status_line();
                    i++;
                    break;
                }
            }
        }

        if (!tcp_http.header_done) {
            return;
        }

        if (i < len) {
            tcp_http_append_body(data + i, len - i);
        }
        return;
    }

    tcp_http_append_body(data, len);
}

static void handle_tcp(const ipv4_header* ip, const u8* payload, u32 len) {
    const tcp_header* tcp;
    u16 src_port;
    u16 dst_port;
    u32 seq;
    u8 flags;
    u32 data_offset;
    u32 data_len;
    const u8* data;

    if (len < sizeof(tcp_header)) {
        return;
    }

    tcp = (const tcp_header*)payload;
    data_offset = (u32)((tcp->data_offset_reserved >> 4) & 0x0Fu) * 4u;
    if (data_offset < sizeof(tcp_header) || data_offset > len) {
        return;
    }

    src_port = ntohs(tcp->src_port);
    dst_port = ntohs(tcp->dst_port);
    seq = ntohl(tcp->seq_num);
    flags = tcp->flags;
    data = payload + data_offset;
    data_len = len - data_offset;

    if (!tcp_http.active) {
        return;
    }
    if (ntohl(ip->src_ip) != tcp_http.remote_ip) {
        return;
    }
    if (src_port != tcp_http.remote_port || dst_port != tcp_http.local_port) {
        return;
    }

    tcp_http.last_activity_ticks = pit_get_ticks();

    if (flags & TCP_FLAG_RST) {
        tcp_http.failed = 1u;
        return;
    }

    if (tcp_http.connecting && (flags & TCP_FLAG_SYN) && (flags & TCP_FLAG_ACK)) {
        tcp_http.seq_remote = seq + 1u;
        tcp_http.connecting = 0u;
        tcp_http.established = 1u;
        send_tcp_packet(tcp_http.remote_ip,
                        tcp_http.local_port,
                        tcp_http.remote_port,
                        tcp_http.seq_local,
                        tcp_http.seq_remote,
                        TCP_FLAG_ACK,
                        0,
                        0u,
                        (u16)(tcp_http.seq_local & 0xFFFFu));
        return;
    }

    if (!tcp_http.established) {
        return;
    }

    if (data_len > 0u) {
        if (seq == tcp_http.seq_remote) {
            tcp_http.seq_remote += data_len;
            tcp_http_feed_data(data, data_len);
            send_tcp_packet(tcp_http.remote_ip,
                            tcp_http.local_port,
                            tcp_http.remote_port,
                            tcp_http.seq_local,
                            tcp_http.seq_remote,
                            TCP_FLAG_ACK,
                            0,
                            0u,
                            (u16)(tcp_http.seq_local & 0xFFFFu));
        }
    }

    if (flags & TCP_FLAG_FIN) {
        if (seq + data_len == tcp_http.seq_remote) {
            tcp_http.seq_remote++;
        }
        tcp_http.fin_seen = 1u;
        send_tcp_packet(tcp_http.remote_ip,
                        tcp_http.local_port,
                        tcp_http.remote_port,
                        tcp_http.seq_local,
                        tcp_http.seq_remote,
                        TCP_FLAG_ACK,
                        0,
                        0u,
                        (u16)(tcp_http.seq_local & 0xFFFFu));
    }
}

static u8 dns_encode_name(const char* host, u8* out, u32 out_size, u32* out_len) {
    u32 i = 0;
    u32 w = 0;
    u32 label_start = 0;
    u32 label_len = 0;
    u8 had_label = 0;

    while (host[i]) {
        if (host[i] == '.') {
            if (label_len == 0 || label_len > 63 || w + 1 + label_len >= out_size) {
                return 0;
            }
            out[w++] = (u8)label_len;
            while (label_start < i) {
                out[w++] = (u8)host[label_start++];
            }
            had_label = 1;
            i++;
            label_start = i;
            label_len = 0;
            continue;
        }

        if (host[i] < 33 || host[i] > 126) {
            return 0;
        }

        label_len++;
        i++;
    }

    if (label_len == 0 && !had_label) {
        return 0;
    }
    if (label_len == 0 && had_label) {
        return 0;
    }
    if (label_len > 63 || w + 1 + label_len + 1 > out_size) {
        return 0;
    }

    out[w++] = (u8)label_len;
    while (label_start < i) {
        out[w++] = (u8)host[label_start++];
    }
    out[w++] = 0;

    *out_len = w;
    return 1;
}

static u8 dns_skip_name(const u8* msg, u32 msg_len, u32 offset, u32* out_offset) {
    u32 pos = offset;
    u32 jumps = 0;
    u8 jumped = 0;

    while (pos < msg_len) {
        u8 c = msg[pos];

        if (c == 0) {
            if (!jumped) {
                *out_offset = pos + 1;
            }
            return 1;
        }

        if ((c & 0xC0u) == 0xC0u) {
            if (pos + 1 >= msg_len) {
                return 0;
            }
            if (!jumped) {
                *out_offset = pos + 2;
            }

            pos = ((u32)(c & 0x3Fu) << 8) | msg[pos + 1];
            jumped = 1;
            jumps++;
            if (jumps > 8 || pos >= msg_len) {
                return 0;
            }
            continue;
        }

        if (c > 63 || pos + 1 + c > msg_len) {
            return 0;
        }

        pos += 1 + c;
    }

    return 0;
}

static void handle_dns(const u8* payload, u32 len) {
    const dns_header* hdr;
    u16 flags;
    u16 qdcount;
    u16 ancount;
    u32 off;
    u16 i;

    if (!dns_state.waiting || len < sizeof(dns_header)) {
        return;
    }

    hdr = (const dns_header*)payload;
    if (ntohs(hdr->id) != dns_state.xid) {
        return;
    }

    flags = ntohs(hdr->flags);
    qdcount = ntohs(hdr->qdcount);
    ancount = ntohs(hdr->ancount);

    if ((flags & 0x8000u) == 0) {
        return;
    }
    if ((flags & 0x000Fu) != 0) {
        dns_state.waiting = 0;
        return;
    }

    off = sizeof(dns_header);

    for (i = 0; i < qdcount; i++) {
        if (!dns_skip_name(payload, len, off, &off)) {
            dns_state.waiting = 0;
            return;
        }
        if (off + 4 > len) {
            dns_state.waiting = 0;
            return;
        }
        off += 4;
    }

    for (i = 0; i < ancount; i++) {
        u16 rr_type;
        u16 rr_class;
        u16 rdlen;

        if (!dns_skip_name(payload, len, off, &off)) {
            dns_state.waiting = 0;
            return;
        }
        if (off + 10 > len) {
            dns_state.waiting = 0;
            return;
        }

        rr_type = (u16)((payload[off] << 8) | payload[off + 1]);
        rr_class = (u16)((payload[off + 2] << 8) | payload[off + 3]);
        rdlen = (u16)((payload[off + 8] << 8) | payload[off + 9]);
        off += 10;

        if (off + rdlen > len) {
            dns_state.waiting = 0;
            return;
        }

        if (rr_type == 1 && rr_class == 1 && rdlen == 4) {
            dns_state.result_ip = ((u32)payload[off] << 24) |
                                  ((u32)payload[off + 1] << 16) |
                                  ((u32)payload[off + 2] << 8) |
                                  (u32)payload[off + 3];
            dns_state.got_reply = 1;
            dns_state.waiting = 0;
            return;
        }

        off += rdlen;
    }

    dns_state.waiting = 0;
}

static void handle_dhcp(const u8* payload, u32 len) {
    const dhcp_packet* d;
    u8 msg_type = 0;
    u32 i;

    if (len < sizeof(dhcp_packet) - sizeof(((dhcp_packet*)0)->options)) {
        dbg_print("dhcp: packet too small\n");
        return;
    }

    d = (const dhcp_packet*)payload;
    if (ntohl(d->xid) != dhcp_state.xid) {
        dbg_print("dhcp: xid mismatch rx=");
        dbg_print_u32(ntohl(d->xid));
        dbg_print(" expected=");
        dbg_print_u32(dhcp_state.xid);
        dbg_print("\n");
        return;
    }
    if (!mem_eq(d->chaddr, cfg.mac, 6)) {
        dbg_print("dhcp: chaddr mismatch\n");
        return;
    }
    if (ntohl(d->cookie) != 0x63825363u) {
        dbg_print("dhcp: bad magic cookie\n");
        return;
    }

    for (i = 0; i + 1 < sizeof(d->options); ) {
        u8 code = d->options[i++];
        if (code == 0xFF) {
            break;
        }
        if (code == 0) {
            continue;
        }
        if (i >= sizeof(d->options)) {
            break;
        }
        {
            u8 opt_len = d->options[i++];
            if (i + opt_len > sizeof(d->options)) {
                break;
            }
            if (code == 53 && opt_len == 1) {
                msg_type = d->options[i];
            } else if (code == 1 && opt_len == 4) {
                dhcp_state.subnet = (d->options[i] << 24) | (d->options[i + 1] << 16) |
                                    (d->options[i + 2] << 8) | d->options[i + 3];
            } else if (code == 3 && opt_len >= 4) {
                dhcp_state.router = (d->options[i] << 24) | (d->options[i + 1] << 16) |
                                    (d->options[i + 2] << 8) | d->options[i + 3];
            } else if (code == 6 && opt_len >= 4) {
                dhcp_state.dns = (d->options[i] << 24) | (d->options[i + 1] << 16) |
                                 (d->options[i + 2] << 8) | d->options[i + 3];
            } else if (code == 54 && opt_len == 4) {
                dhcp_state.server_ip = (d->options[i] << 24) | (d->options[i + 1] << 16) |
                                       (d->options[i + 2] << 8) | d->options[i + 3];
            }
            i += opt_len;
        }
    }

    dbg_print("dhcp: message type ");
    dbg_print_u32(msg_type);
    dbg_print(" yiaddr=");
    dbg_print_ip(ntohl(d->yiaddr));
    dbg_print(" server=");
    dbg_print_ip(dhcp_state.server_ip);
    dbg_print("\n");

    if (msg_type == 2 && dhcp_state.waiting_offer) {
        dhcp_state.offered_ip = ntohl(d->yiaddr);
        dhcp_state.waiting_offer = 0;
        dhcp_state.waiting_ack = 1;
        dbg_print("dhcp: got offer for ");
        dbg_print_ip(dhcp_state.offered_ip);
        dbg_print("\n");
    } else if (msg_type == 5 && dhcp_state.waiting_ack) {
        cfg.ip = ntohl(d->yiaddr);
        if (dhcp_state.subnet) {
            cfg.subnet = dhcp_state.subnet;
        }
        if (dhcp_state.router) {
            cfg.gateway = dhcp_state.router;
        }
        if (dhcp_state.dns) {
            cfg.dns = dhcp_state.dns;
        }
        cfg.dhcp_configured = 1;
        dhcp_state.waiting_ack = 0;
        dbg_print("dhcp: got ack ip=");
        dbg_print_ip(cfg.ip);
        dbg_print("\n");
    }
}

static void handle_udp(const ipv4_header* ip, const u8* payload, u32 len) {
    const udp_header* udp;
    u16 src_port;
    u16 dst_port;
    u16 udp_len;

    if (len < sizeof(udp_header)) {
        return;
    }

    udp = (const udp_header*)payload;
    src_port = ntohs(udp->src_port);
    dst_port = ntohs(udp->dst_port);
    udp_len = ntohs(udp->len);

    if (udp_len < sizeof(udp_header) || udp_len > len) {
        return;
    }

    if (src_port == DHCP_SERVER_PORT && dst_port == DHCP_CLIENT_PORT) {
        dbg_print("dhcp: udp packet received\n");
        handle_dhcp(payload + sizeof(udp_header), udp_len - sizeof(udp_header));
    } else if (dst_port == DNS_CLIENT_PORT && src_port == DNS_SERVER_PORT) {
        handle_dns(payload + sizeof(udp_header), udp_len - sizeof(udp_header));
    }

    (void)ip;
}

static void handle_icmp(const ipv4_header* ip, const u8* payload, u32 len) {
    const icmp_echo* icmp;

    if (len < sizeof(icmp_echo)) {
        return;
    }

    icmp = (const icmp_echo*)payload;

    if (icmp->type == ICMP_ECHO_REPLY && ping_state.waiting) {
        if (ntohs(icmp->ident) == ping_state.ident && ntohs(icmp->seq) == ping_state.seq) {
            ping_state.waiting = 0;
            ping_state.got_reply = 1;
            ping_state.rtt_ms = ((pit_get_ticks() - ping_state.sent_ticks) * 1000u) / pit_get_frequency();
        }
    } else if (icmp->type == ICMP_ECHO_REQUEST && cfg.ip != 0 && ntohl(ip->dst_ip) == cfg.ip) {
        u8 reply[128];
        icmp_echo* out = (icmp_echo*)reply;
        u32 copy_len = len;

        if (copy_len > sizeof(reply)) {
            copy_len = sizeof(reply);
        }

        mem_copy(reply, payload, copy_len);
        out->type = ICMP_ECHO_REPLY;
        out->checksum = 0;
        out->checksum = htons(checksum16(reply, copy_len));
        send_ipv4_packet(ntohl(ip->src_ip), IP_PROTO_ICMP, reply, (u16)copy_len, 0x1234);
    }
}

static void handle_ipv4(const u8* payload, u32 len) {
    const ipv4_header* ip;
    u32 ihl;
    u16 total_len;

    if (len < sizeof(ipv4_header)) {
        return;
    }

    ip = (const ipv4_header*)payload;
    if ((ip->version_ihl >> 4) != 4) {
        return;
    }

    ihl = (u32)(ip->version_ihl & 0x0Fu) * 4u;
    if (ihl < sizeof(ipv4_header) || len < ihl) {
        return;
    }

    total_len = ntohs(ip->total_len);
    if (total_len < ihl || total_len > len) {
        return;
    }

    if (cfg.ip != 0 && ntohl(ip->dst_ip) != cfg.ip && ntohl(ip->dst_ip) != 0xFFFFFFFFu) {
        return;
    }

    if (ip->proto == IP_PROTO_ICMP) {
        handle_icmp(ip, payload + ihl, total_len - ihl);
    } else if (ip->proto == IP_PROTO_UDP) {
        handle_udp(ip, payload + ihl, total_len - ihl);
    } else if (ip->proto == IP_PROTO_TCP) {
        handle_tcp(ip, payload + ihl, total_len - ihl);
    }
}

static void handle_arp(const u8* payload, u32 len) {
    const arp_packet* arp;
    u16 op;
    u32 sender_ip;
    u32 target_ip;

    if (len < sizeof(arp_packet)) {
        return;
    }

    arp = (const arp_packet*)payload;
    if (ntohs(arp->hw_type) != 1 || ntohs(arp->proto_type) != ETH_TYPE_IPV4 ||
        arp->hw_len != 6 || arp->proto_len != 4) {
        return;
    }

    op = ntohs(arp->op);
    sender_ip = ntohl(arp->sender_ip);
    target_ip = ntohl(arp->target_ip);

    arp_cache.valid = 1;
    arp_cache.ip = sender_ip;
    mem_copy(arp_cache.mac, arp->sender_mac, 6);

    dbg_print("net: arp learned ");
    dbg_print_ip(sender_ip);
    dbg_print("\n");

    if (op == ARP_OP_REQUEST && cfg.ip != 0 && target_ip == cfg.ip) {
        send_arp_reply(arp->sender_mac, sender_ip);
    }
}

static void net_on_frame(const u8* frame, u32 len) {
    const eth_header* eth;
    u16 type;

    if (len < sizeof(eth_header)) {
        return;
    }

    eth = (const eth_header*)frame;
    type = ntohs(eth->ethertype);

    if (type == ETH_TYPE_ARP) {
        handle_arp(frame + sizeof(eth_header), len - sizeof(eth_header));
    } else if (type == ETH_TYPE_IPV4) {
        handle_ipv4(frame + sizeof(eth_header), len - sizeof(eth_header));
    }
}

u8 net_is_ready(void) {
    return cfg.link_up;
}

const net_config* net_get_config(void) {
    return &cfg;
}

void net_init(void) {
    const u8* m;

    mem_set(&cfg, 0, sizeof(cfg));
    mem_set(&arp_cache, 0, sizeof(arp_cache));
    mem_set(&ping_state, 0, sizeof(ping_state));
    mem_set(&dhcp_state, 0, sizeof(dhcp_state));
    mem_set(&dns_state, 0, sizeof(dns_state));
    tcp_http_reset();

    if (!rtl8139_init(net_on_frame)) {
        cfg.link_up = 0;
        dbg_print("net: rtl8139 init failed\n");
        return;
    }

    m = rtl8139_mac();
    mem_copy(cfg.mac, m, 6);
    cfg.link_up = 1;
    dbg_print("net: rtl8139 ready\n");
}

void net_poll(void) {
    if (!cfg.link_up) {
        return;
    }
    rtl8139_poll();
}

static void print_mac(const u8* mac_addr) {
    static const char* hex = "0123456789ABCDEF";
    u32 i;
    for (i = 0; i < 6; i++) {
        display_put_char(hex[(mac_addr[i] >> 4) & 0xFu]);
        display_put_char(hex[mac_addr[i] & 0xFu]);
        if (i != 5) {
            display_put_char(':');
        }
    }
}

void net_print_config(void) {
    char ip_buf[16];

    if (!cfg.link_up) {
        display_print("net: link down (RTL8139 not found)\n");
        return;
    }

    display_print("net: link up\nmac: ");
    print_mac(cfg.mac);
    display_put_char('\n');

    net_format_ipv4(cfg.ip, ip_buf, sizeof(ip_buf));
    display_print("ip: ");
    display_print(ip_buf);
    display_put_char('\n');

    net_format_ipv4(cfg.subnet, ip_buf, sizeof(ip_buf));
    display_print("mask: ");
    display_print(ip_buf);
    display_put_char('\n');

    net_format_ipv4(cfg.gateway, ip_buf, sizeof(ip_buf));
    display_print("gw: ");
    display_print(ip_buf);
    display_put_char('\n');

    net_format_ipv4(cfg.dns, ip_buf, sizeof(ip_buf));
    display_print("dns: ");
    display_print(ip_buf);
    display_put_char('\n');

    display_print("dhcp: ");
    display_print(cfg.dhcp_configured ? "yes\n" : "no\n");
}

static u8 send_dhcp_message(u8 msg_type, u32 req_ip, u32 server_id) {
    dhcp_packet* d = (dhcp_packet*)dhcp_body_buf;
    udp_header* udp = (udp_header*)dhcp_udp_buf;
    u32 dhcp_len;
    u32 offset;
    u8 broadcast_mac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    ipv4_header* ip;

    mem_set(dhcp_body_buf, 0, sizeof(dhcp_body_buf));

    d->op = 1;
    d->htype = 1;
    d->hlen = 6;
    d->hops = 0;
    d->xid = htonl(dhcp_state.xid);
    d->secs = htons(0);
    d->flags = htons(0x8000);
    d->ciaddr = 0;
    d->yiaddr = 0;
    d->siaddr = 0;
    d->giaddr = 0;
    mem_copy(d->chaddr, cfg.mac, 6);
    d->cookie = htonl(0x63825363u);

    offset = 0;
    d->options[offset++] = 53;
    d->options[offset++] = 1;
    d->options[offset++] = msg_type;

    d->options[offset++] = 55;
    d->options[offset++] = 3;
    d->options[offset++] = 1;
    d->options[offset++] = 3;
    d->options[offset++] = 6;

    if (msg_type == 3 && req_ip != 0 && server_id != 0) {
        d->options[offset++] = 50;
        d->options[offset++] = 4;
        d->options[offset++] = (u8)((req_ip >> 24) & 0xFFu);
        d->options[offset++] = (u8)((req_ip >> 16) & 0xFFu);
        d->options[offset++] = (u8)((req_ip >> 8) & 0xFFu);
        d->options[offset++] = (u8)(req_ip & 0xFFu);

        d->options[offset++] = 54;
        d->options[offset++] = 4;
        d->options[offset++] = (u8)((server_id >> 24) & 0xFFu);
        d->options[offset++] = (u8)((server_id >> 16) & 0xFFu);
        d->options[offset++] = (u8)((server_id >> 8) & 0xFFu);
        d->options[offset++] = (u8)(server_id & 0xFFu);
    }

    d->options[offset++] = 255;
    dhcp_len = (u32)((u8*)d->options + offset - (u8*)d);

    /* RFC 2131: DHCP messages should be at least 300 bytes. */
    if (dhcp_len < 300u) {
        dhcp_len = 300u;
    }

    dbg_print("dhcp: tx ");
    dbg_print(msg_type == 1 ? "discover" : (msg_type == 3 ? "request" : "message"));
    dbg_print(" xid=");
    dbg_print_u32(dhcp_state.xid);
    if (msg_type == 3) {
        dbg_print(" req_ip=");
        dbg_print_ip(req_ip);
        dbg_print(" server=");
        dbg_print_ip(server_id);
    }
    dbg_print(" len=");
    dbg_print_u32(dhcp_len);
    dbg_print("\n");

    mem_copy(dhcp_udp_buf + sizeof(udp_header), dhcp_body_buf, dhcp_len);

    udp->src_port = htons(DHCP_CLIENT_PORT);
    udp->dst_port = htons(DHCP_SERVER_PORT);
    udp->len = htons((u16)(sizeof(udp_header) + dhcp_len));
    udp->checksum = 0;

    ip = (ipv4_header*)dhcp_ip_buf;
    ip->version_ihl = 0x45;
    ip->dscp_ecn = 0;
    ip->total_len = htons((u16)(sizeof(ipv4_header) + ntohs(udp->len)));
    ip->ident = htons((u16)(dhcp_state.xid & 0xFFFFu));
    ip->flags_frag = htons(0x0000);
    ip->ttl = 64;
    ip->proto = IP_PROTO_UDP;
    ip->checksum = 0;
    ip->src_ip = htonl(0u);
    ip->dst_ip = htonl(0xFFFFFFFFu);
    ip->checksum = htons(checksum16(ip, sizeof(ipv4_header)));

    mem_copy(dhcp_ip_buf + sizeof(ipv4_header), dhcp_udp_buf, ntohs(udp->len));

    return send_frame(broadcast_mac, ETH_TYPE_IPV4, dhcp_ip_buf, sizeof(ipv4_header) + ntohs(udp->len));
}

u8 net_dhcp_request(void) {
    u32 start;
    u32 timeout_ticks;

    if (!cfg.link_up) {
        dbg_print("dhcp: link down\n");
        return 0;
    }

    dhcp_state.xid = pit_get_ticks() ^ 0xA5A51234u;
    dhcp_state.waiting_offer = 1;
    dhcp_state.waiting_ack = 0;
    dhcp_state.offered_ip = 0;
    dhcp_state.server_ip = 0;
    dhcp_state.subnet = 0;
    dhcp_state.router = 0;
    dhcp_state.dns = 0;

    dbg_print("dhcp: start xid=");
    dbg_print_u32(dhcp_state.xid);
    dbg_print("\n");

    if (!send_dhcp_message(1, 0, 0)) {
        dbg_print("dhcp: failed to send discover\n");
        return 0;
    }

    start = pit_get_ticks();
    timeout_ticks = pit_get_frequency() * 5u;
    while ((pit_get_ticks() - start) < timeout_ticks) {
        net_poll();
        if (!dhcp_state.waiting_offer) {
            break;
        }
    }

    if (dhcp_state.waiting_offer || dhcp_state.offered_ip == 0 || dhcp_state.server_ip == 0) {
        dbg_print("dhcp: offer timeout or invalid offer\n");
        return 0;
    }

    if (!send_dhcp_message(3, dhcp_state.offered_ip, dhcp_state.server_ip)) {
        dbg_print("dhcp: failed to send request\n");
        return 0;
    }

    start = pit_get_ticks();
    while ((pit_get_ticks() - start) < timeout_ticks) {
        net_poll();
        if (!dhcp_state.waiting_ack) {
            break;
        }
    }

    if (!cfg.dhcp_configured) {
        dbg_print("dhcp: ack timeout\n");
    }

    return cfg.dhcp_configured;
}

u8 net_ping(u32 target_ip, u32 timeout_ms, u32* out_rtt_ms) {
    u8 payload[64];
    icmp_echo* icmp = (icmp_echo*)payload;
    u32 i;
    u32 timeout_ticks;
    u32 start;

    if (!cfg.link_up || cfg.ip == 0) {
        return 0;
    }

    ping_state.waiting = 1;
    ping_state.got_reply = 0;
    ping_state.ident = (u16)(pit_get_ticks() & 0xFFFFu);
    ping_state.seq++;
    ping_state.target = target_ip;

    icmp->type = ICMP_ECHO_REQUEST;
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->ident = htons(ping_state.ident);
    icmp->seq = htons(ping_state.seq);

    for (i = sizeof(icmp_echo); i < sizeof(payload); i++) {
        payload[i] = (u8)i;
    }

    icmp->checksum = htons(checksum16(payload, sizeof(payload)));

    ping_state.sent_ticks = pit_get_ticks();
    if (!send_ipv4_packet(target_ip, IP_PROTO_ICMP, payload, sizeof(payload), ping_state.ident)) {
        ping_state.waiting = 0;
        return 0;
    }

    timeout_ticks = (timeout_ms * pit_get_frequency()) / 1000u;
    if (timeout_ticks == 0) {
        timeout_ticks = 1;
    }

    start = pit_get_ticks();
    while ((pit_get_ticks() - start) < timeout_ticks) {
        net_poll();
        if (ping_state.got_reply) {
            if (out_rtt_ms) {
                *out_rtt_ms = ping_state.rtt_ms;
            }
            return 1;
        }
    }

    ping_state.waiting = 0;
    return 0;
}

u8 net_resolve_ipv4(const char* host, u32 timeout_ms, u32* out_ip) {
    dns_header* hdr;
    u32 name_len;
    u32 msg_len;
    u32 start;
    u32 timeout_ticks;

    if (!cfg.link_up || cfg.ip == 0 || cfg.dns == 0 || !host || !host[0] || !out_ip) {
        return 0;
    }

    if (net_parse_ipv4(host, out_ip)) {
        return 1;
    }

    mem_set(dns_query_buf, 0, sizeof(dns_query_buf));
    hdr = (dns_header*)dns_query_buf;

    dns_state.waiting = 1;
    dns_state.got_reply = 0;
    dns_state.result_ip = 0;
    dns_state.xid = (u16)((pit_get_ticks() ^ 0xBEEF1234u) & 0xFFFFu);

    hdr->id = htons(dns_state.xid);
    hdr->flags = htons(0x0100u);
    hdr->qdcount = htons(1u);
    hdr->ancount = 0;
    hdr->nscount = 0;
    hdr->arcount = 0;

    if (!dns_encode_name(host, dns_query_buf + sizeof(dns_header), sizeof(dns_query_buf) - sizeof(dns_header), &name_len)) {
        dns_state.waiting = 0;
        return 0;
    }

    msg_len = sizeof(dns_header) + name_len;
    if (msg_len + 4 > sizeof(dns_query_buf)) {
        dns_state.waiting = 0;
        return 0;
    }

    dns_query_buf[msg_len++] = 0;
    dns_query_buf[msg_len++] = 1;
    dns_query_buf[msg_len++] = 0;
    dns_query_buf[msg_len++] = 1;

    if (!send_udp_packet(cfg.dns, DNS_CLIENT_PORT, DNS_SERVER_PORT, dns_query_buf, (u16)msg_len, dns_state.xid)) {
        dns_state.waiting = 0;
        return 0;
    }

    timeout_ticks = (timeout_ms * pit_get_frequency()) / 1000u;
    if (timeout_ticks == 0) {
        timeout_ticks = 1;
    }

    start = pit_get_ticks();
    while ((pit_get_ticks() - start) < timeout_ticks) {
        net_poll();
        if (!dns_state.waiting) {
            break;
        }
    }

    if (!dns_state.got_reply || dns_state.result_ip == 0) {
        return 0;
    }

    *out_ip = dns_state.result_ip;
    return 1;
}

net_http_result net_http_get(const char* url,
                             u32 timeout_ms,
                             char* body_out,
                             u32 body_out_size,
                             net_http_response* out_response) {
    char host[128];
    const char* path;
    u16 port = 80u;
    net_http_result parsed;
    u32 ip = 0u;
    u32 timeout_ticks;
    u32 start;
    u32 now;
    u32 retries = 0u;
    u16 local_port;
    char request[384];
    u32 req_len = 0u;
    const char* host_hdr;
    const char* p;

    if (!out_response || !url || timeout_ms == 0u) {
        return NET_HTTP_ERR_INVALID_ARG;
    }

    mem_set(out_response, 0, sizeof(*out_response));

    parsed = parse_http_url(url, host, sizeof(host), &port, &path);
    if (parsed != NET_HTTP_OK) {
        out_response->result = parsed;
        return parsed;
    }

    if (!cfg.link_up) {
        out_response->result = NET_HTTP_ERR_LINK_DOWN;
        return out_response->result;
    }

    if (cfg.ip == 0u) {
        out_response->result = NET_HTTP_ERR_NO_IP;
        return out_response->result;
    }

    if (!net_resolve_ipv4(host, timeout_ms, &ip)) {
        out_response->result = NET_HTTP_ERR_DNS_FAILED;
        return out_response->result;
    }

    out_response->resolved_ip = ip;
    out_response->port = port;

    if (body_out && body_out_size > 0u) {
        body_out[0] = '\0';
    }

    timeout_ticks = (timeout_ms * pit_get_frequency()) / 1000u;
    if (timeout_ticks == 0u) {
        timeout_ticks = 1u;
    }

    tcp_http_reset();
    local_port = (u16)(49152u + (pit_get_ticks() & 0x3FFFu));

    tcp_http.active = 1u;
    tcp_http.connecting = 1u;
    tcp_http.remote_ip = ip;
    tcp_http.local_port = local_port;
    tcp_http.remote_port = port;
    tcp_http.seq_local = 0x13570000u ^ pit_get_ticks();
    tcp_http.body_out = body_out;
    tcp_http.body_out_size = body_out_size;
    tcp_http.last_activity_ticks = pit_get_ticks();

    if (!send_tcp_packet(ip,
                         local_port,
                         port,
                         tcp_http.seq_local,
                         0u,
                         TCP_FLAG_SYN,
                         0,
                         0u,
                         (u16)(tcp_http.seq_local & 0xFFFFu))) {
        out_response->result = NET_HTTP_ERR_TIMEOUT;
        tcp_http_reset();
        return out_response->result;
    }

    tcp_http.seq_local++;
    tcp_http.connect_sent_ticks = pit_get_ticks();
    start = pit_get_ticks();
    while ((pit_get_ticks() - start) < timeout_ticks) {
        net_poll();
        if (tcp_http.established) {
            break;
        }
        if (tcp_http.failed) {
            out_response->result = NET_HTTP_ERR_PROTOCOL;
            tcp_http_reset();
            return out_response->result;
        }
        now = pit_get_ticks();
        if (!tcp_http.established && retries < 3u &&
            (now - tcp_http.connect_sent_ticks) > (pit_get_frequency() / 2u)) {
            if (send_tcp_packet(ip,
                                local_port,
                                port,
                                tcp_http.seq_local - 1u,
                                0u,
                                TCP_FLAG_SYN,
                                0,
                                0u,
                                (u16)((tcp_http.seq_local - 1u) & 0xFFFFu))) {
                tcp_http.connect_sent_ticks = now;
                retries++;
            }
        }
    }

    if (!tcp_http.established) {
        out_response->result = NET_HTTP_ERR_TIMEOUT;
        tcp_http_reset();
        return out_response->result;
    }

    host_hdr = host;
    p = "GET ";
    while (*p && req_len + 1u < sizeof(request)) request[req_len++] = *p++;
    p = path;
    while (*p && req_len + 1u < sizeof(request)) request[req_len++] = *p++;
    p = " HTTP/1.1\r\nHost: ";
    while (*p && req_len + 1u < sizeof(request)) request[req_len++] = *p++;
    p = host_hdr;
    while (*p && req_len + 1u < sizeof(request)) request[req_len++] = *p++;
    if (port != 80u) {
        char port_buf[8];
        u32 n = 0u;
        u32 v = port;
        char rev[8];
        if (v == 0u) {
            port_buf[n++] = '0';
        } else {
            while (v > 0u && n < sizeof(rev)) {
                rev[n++] = (char)('0' + (v % 10u));
                v /= 10u;
            }
            {
                u32 j;
                for (j = 0u; j < n; j++) {
                    port_buf[j] = rev[n - 1u - j];
                }
            }
        }
        p = ":";
        while (*p && req_len + 1u < sizeof(request)) request[req_len++] = *p++;
        {
            u32 j;
            for (j = 0u; j < n && req_len + 1u < sizeof(request); j++) {
                request[req_len++] = port_buf[j];
            }
        }
    }
    p = "\r\nConnection: close\r\nUser-Agent: VertexOS/0.1\r\n\r\n";
    while (*p && req_len + 1u < sizeof(request)) request[req_len++] = *p++;

    if (req_len == 0u || req_len + 1u >= sizeof(request)) {
        out_response->result = NET_HTTP_ERR_PROTOCOL;
        tcp_http_reset();
        return out_response->result;
    }
    request[req_len] = '\0';

    if (!send_tcp_packet(ip,
                         local_port,
                         port,
                         tcp_http.seq_local,
                         tcp_http.seq_remote,
                         TCP_FLAG_ACK | TCP_FLAG_PSH,
                         request,
                         (u16)req_len,
                         (u16)(tcp_http.seq_local & 0xFFFFu))) {
        out_response->result = NET_HTTP_ERR_TIMEOUT;
        tcp_http_reset();
        return out_response->result;
    }
    tcp_http.seq_local += req_len;

    start = pit_get_ticks();
    while ((pit_get_ticks() - start) < timeout_ticks) {
        net_poll();
        if (tcp_http.failed) {
            out_response->result = NET_HTTP_ERR_PROTOCOL;
            tcp_http_reset();
            return out_response->result;
        }
        if (tcp_http.fin_seen) {
            break;
        }
    }

    if (!tcp_http.header_done) {
        out_response->result = NET_HTTP_ERR_TIMEOUT;
        tcp_http_reset();
        return out_response->result;
    }
    if (!tcp_http.status_valid) {
        out_response->result = NET_HTTP_ERR_PROTOCOL;
        tcp_http_reset();
        return out_response->result;
    }

    out_response->status_code = tcp_http.status_code;
    out_response->body_len = tcp_http.body_total;
    out_response->body_truncated = tcp_http.body_truncated;
    out_response->result = NET_HTTP_OK;

    if (tcp_http.established) {
        send_tcp_packet(ip,
                        local_port,
                        port,
                        tcp_http.seq_local,
                        tcp_http.seq_remote,
                        TCP_FLAG_FIN | TCP_FLAG_ACK,
                        0,
                        0u,
                        (u16)(tcp_http.seq_local & 0xFFFFu));
        tcp_http.seq_local++;
    }

    tcp_http_reset();
    return out_response->result;
}
