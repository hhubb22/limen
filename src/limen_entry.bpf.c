#include "vmlinux.h"
#include <bpf/bpf_endian.h>
#include <bpf/bpf_helpers.h>

#define ETH_P_8021Q 0x8100  /* 802.1Q VLAN Extended Header  */
#define ETH_P_8021AD 0x88A8 /* 802.1ad Service VLAN		*/
#define ETH_P_IP	0x0800		/* Internet Protocol packet	*/
#define ETH_P_IPV6	0x86DD		/* IPv6 over bluebook		*/

SEC(".rodata") const volatile __u32 toeplitz_tbl[32][256];

static __always_inline int parse_eth(void **cursor, void *data_end,
                                     __u16 *proto) {
  struct ethhdr *eth = (struct ethhdr *)(*cursor);
  if ((void *)(eth + 1) > data_end)
    return -1;

  *proto = eth->h_proto;
  *cursor = eth + 1;

#pragma clang loop unroll(full)
  for (int i = 0; i < 2; i++) {
    __u16 p = bpf_ntohs(*proto);
    if (p == ETH_P_8021Q || p == ETH_P_8021AD) {
      struct vlan_hdr *vh = (struct vlan_hdr *)(*cursor);
      if ((void *)(vh + 1) > data_end)
        return -1;
      *proto = vh->h_vlan_encapsulated_proto;
      *cursor = vh + 1;
    } else {
      break;
    }
  }
  return 0;
}

static __always_inline int parse_ipv4_src(void **cursor, void *data_end, __u32 *src)
{
    struct iphdr *iph = (struct iphdr *)(*cursor);
    if ((void *)(iph + 1) > data_end) return -1;

    // 校验头长
    __u32 ihl_len = iph->ihl * 4;
    if (ihl_len < sizeof(*iph)) return -1;
    if ((void *)iph + ihl_len > data_end) return -1;

    *src = iph->saddr;
    *cursor = (void *)iph + ihl_len;
    return 0;
}

static __always_inline __u32 rss_toeplitz_bytes(const __u8 *p, int len, __u32 pos_mod32)
{
  __u32 acc = 0;
#pragma clang loop unroll(full)
  for (int i = 0; i < len; i++) {
    acc ^= toeplitz_tbl[pos_mod32][p[i]];
    pos_mod32 = (pos_mod32 + 8) & 31;
  }
  return acc;
}

static __always_inline void rss_feed(const __u8 *p, int len, __u32 *pos, __u32 *acc)
{
  *acc ^= rss_toeplitz_bytes(p, len, *pos);
  *pos = (*pos + len*8) & 31;
}


SEC("xdp")
int xdp_prog(struct xdp_md *ctx) {
  void *data = (void *)(long)ctx->data;
  void *data_end = (void *)(long)ctx->data_end;
  void *cur = data;

  __u16 proto;
  if (parse_eth(&cur, data_end, &proto) < 0)
    return XDP_PASS;
  __u16 p = bpf_ntohs(proto);
  if (p == ETH_P_IP) {
    __u32 src;
    if (parse_ipv4_src(&cur, data_end, &src) == 0) {
        __u32 pos = 0;
        __u32 acc = 0;
        rss_feed((const __u8 *)&src, sizeof(src), &pos, &acc);
        bpf_printk("src=%x rss=%x\n", bpf_ntohl(src), bpf_ntohl(acc));
    }
  } else if (p == ETH_P_IPV6) {
    return XDP_DROP;
  }

  return XDP_PASS;
}

char LICENSE[] SEC("license") = "Dual BSD/GPL";
