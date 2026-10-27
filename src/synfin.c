// SPDX-License-Identifier: GPL-2.0
#include <linux/bpf.h>
#include <linux/if_ether.h>

/* 将程序和许可证放入加载器识别的 ELF 段；内联解析函数供 BPF 编译。 */
#define SEC(name) __attribute__((section(name), used))
#define INLINE static __attribute__((always_inline)) inline

INLINE __u16 be16(const __u8 *p)
{
    /* 按网络字节序拼接 16 位值；调用者负责保证两个字节均可访问。 */
    return ((__u16)p[0] << 8) | p[1];
}

INLINE int tcp_synfin(__u8 *p, __u8 *end, __u32 remaining)
{
    /* 同时检查 IP 声明的剩余长度与实际报文边界，保证最小 TCP 头可读。 */
    if (remaining < 20 || p + 20 > end)
        return 0;
    /* Data Offset 以 4 字节为单位，包含固定头和 TCP 选项。 */
    __u32 length = (p[12] >> 4) * 4;
    if (length < 20 || length > remaining || p + length > end)
        return 0;
    /* FIN 为 bit 0、SYN 为 bit 1；其他标志位不影响匹配结果。 */
    return (p[13] & 0x03) == 0x03;
}

/* 返回 1 表示确认匹配 SYN+FIN；截断、格式异常或不支持的协议均返回 0，放行。 */
INLINE int synfin(__u8 *data, __u8 *end)
{
    /* 先确认 14 字节以太网头完整，截断报文直接放行。 */
    if (data + ETH_HLEN > end)
        return 0;
    /* EtherType 位于以太网头偏移 12。 */
    __u16 protocol = be16(data + 12);
    __u8 *p = data + ETH_HLEN;

    /* 最多剥离两层 802.1Q/802.1ad 标签；每层跳过 TCI 并读取内层 EtherType。
     * 更深的 VLAN 封装不会继续解析，最终按未识别协议放行。 */
#pragma unroll
    for (int i = 0; i < 2; i++) {
        if (protocol != ETH_P_8021Q && protocol != ETH_P_8021AD)
            break;
        if (p + 4 > end)
            return 0;
        protocol = be16(p + 2);
        p += 4;
    }

    if (protocol == ETH_P_IP) {
        /* 验证 IPv4 固定头、版本及 TCP 协议号，再读取可变头长和 IP 总长度。 */
        if (p + 20 > end || p[0] >> 4 != 4 || p[9] != 6)
            return 0;
        __u32 ihl = (p[0] & 15) * 4;
        __u32 total = be16(p + 2);
        if (ihl < 20 || total < ihl || p + ihl > end)
            return 0;
        /* 分片偏移非零时不解析 TCP；偏移为零的首片即使 MF 置位也继续检查。 */
        if (be16(p + 6) & 0x1fff)
            return 0;
        /* 跳过 IP 选项；这里只要求 TCP 头完整，不要求整个 IP 数据报已到齐。 */
        return tcp_synfin(p + ihl, end, total - ihl);
    }

    if (protocol != ETH_P_IPV6 || p + 40 > end || p[0] >> 4 != 6)
        return 0;
    /* IPv6 Payload Length 不包含 40 字节固定头；Next Header 指向后续头部。 */
    __u32 remaining = be16(p + 4);
    __u8 next = p[6];
    p += 40;

    /* 最多跳过 8 个扩展头，展开循环以限制解析成本。
     * 不解析 Jumbo Payload 选项，Payload Length 为 0 的超大报文会放行；
     * 超过 8 个扩展头仍未到达 TCP 时也放行。 */
#pragma unroll
    for (int i = 0; i < 8; i++) {
        if (next == 6)
            return tcp_synfin(p, end, remaining);
        if (remaining < 8 || p + 8 > end)
            return 0;

        __u32 length;
        if (next == 44) {
            /* Fragment 头固定为 8 字节；仅解析偏移为零的首片或原子分片。 */
            if (be16(p + 2) & 0xfff8)
                return 0;
            length = 8;
        } else if (next == 0 || next == 43 || next == 60) {
            /* Hop-by-Hop、Routing、Destination Options：长度为 (Hdr Ext Len + 1) * 8。 */
            length = ((__u32)p[1] + 1) * 8;
        } else if (next == 51) {
            /* AH 的长度单位为 4 字节，字段值不计最前面的两个 4 字节单元。 */
            length = ((__u32)p[1] + 2) * 4;
        } else {
            /* 其他协议（含 ESP、No Next Header）无法继续定位 TCP，直接放行。 */
            return 0;
        }
        if (length > remaining || p + length > end)
            return 0;
        /* 消费扩展头并缩减 IP 声明的剩余长度，每次前进前均检查实际边界。 */
        next = p[0];
        p += length;
        remaining -= length;
    }
    /* 恰好跳过第 8 个扩展头后，仍允许检查紧随其后的 TCP。 */
    return next == 6 ? tcp_synfin(p, end, remaining) : 0;
}

SEC("xdp")
int synfin_filter(struct xdp_md *ctx)
{
    /* XDP 提供当前可直接访问的报文区间 [data, data_end)。 */
    __u8 *data = (__u8 *)(long)ctx->data;
    __u8 *end = (__u8 *)(long)ctx->data_end;
    /* 只有明确识别到 SYN+FIN 才丢弃，其余交给后续网络栈处理。 */
    return synfin(data, end) ? XDP_DROP : XDP_PASS;
}

char _license[] SEC("license") = "GPL";
