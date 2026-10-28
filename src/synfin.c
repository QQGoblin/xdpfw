// SPDX-License-Identifier: GPL-2.0
#include <linux/bpf.h>
#include <linux/if_ether.h>

/* 将程序和许可证放入加载器识别的 ELF 段；内联解析函数供 BPF 编译。 */
#define SEC(name) __attribute__((section(name), used))
#define INLINE static __attribute__((always_inline)) inline

INLINE __u16 be16(const __u8 *p)
{
    // 按网络字节序拼接 16 位值
    // 调用者负责保证两个字节均可访问
    return ((__u16)p[0] << 8) | p[1];
}

INLINE int tcp_synfin(__u8 *p, __u8 *end, __u32 remaining)
{
    // 校验 TCP Header 至少 20 字节
    if (remaining < 20 || p + 20 > end)
        return 0;

    // 获取 TCP Header 长度
    __u32 length = (p[12] >> 4) * 4;
    if (length < 20 || length > remaining || p + length > end)
        return 0;

    // 检查 SYN 和 FIN 标志位是否同时置位
    return (p[13] & 0x03) == 0x03;
}

INLINE int synfin(__u8 *data, __u8 *end)
{
    // 先确认 14 字节以太网头完整，截断报文直接放行
    if (data + ETH_HLEN > end)
        return 0;

    // 普通以太网帧格式：| 目的 MAC 6B | 源 MAC 6B | EtherType 2B | 数据 ... |
    __u16 protocol = be16(data + 12);  // 获取以太网帧的 EtherType 字段
    __u8 *p = data + ETH_HLEN;         // 跳过以太网头（PS：长度为 ETH_HLEN），获取 IP 报文的起始位置

#pragma unroll
    // 允许最多两层 VLAN 标签，避免循环解析成本过高
    for (int i = 0; i < 2; i++)
    {
        // ETH_P_8021Q 和 ETH_P_8021AD 分别对应 0x8100 和 0x88A8，表示 VLAN 标签协议类型。
        // VLAN 帧格式如下，其中 TCI 称为 Tag Control Information，包含 VLAN ID、优先级等信息。
        //
        //     | 目的 MAC 6B | 源 MAC 6B | TCI 2B | EtherType 2B | EtherType 2B | 数据 ... |
        //
        if (protocol != ETH_P_8021Q && protocol != ETH_P_8021AD)
            break;
        if (p + 4 > end)
            return 0;
        protocol = be16(p + 2);  // 内层 EtherType
        p += 4;
    }

    // 解析 IPv4 报文
    if (protocol == ETH_P_IP)
    {
        // 验证 IPv4 固定头大于 20 字节
        if (p + 20 > end)
            return 0;

        // 验证 IP 版本为 4
        if (p[0] >> 4 != 4)
            return 0;

        // 验证载荷 TCP 报文
        if (p[9] != 6)
            return 0;
        
        // 读取 IP Header 长度
        __u32 ihl = (p[0] & 15) * 4;

        // 读取 IP 总长度
        __u32 total = be16(p + 2);

        // 长度验证
        if (ihl < 20 || total < ihl || p + ihl > end)
            return 0;

        // 考虑 IP 分片的情况，只有 Fragment Offset 为 0 的报文才包含完整的 TCP Header
        if (be16(p + 6) & 0x1fff)
            return 0;

        return tcp_synfin(p + ihl, end, total - ihl);
    }

    if (protocol != ETH_P_IPV6 || p + 40 > end || p[0] >> 4 != 6)
        return 0;

    // IPv6 处理逻辑

    __u32 remaining = be16(p + 4);
    __u8 next = p[6];
    p += 40;

#pragma unroll
    for (int i = 0; i < 8; i++)
    {
        if (next == 6)
            return tcp_synfin(p, end, remaining);
        if (remaining < 8 || p + 8 > end)
            return 0;

        __u32 length;
        if (next == 44) {
            if (be16(p + 2) & 0xfff8)
                return 0;
            length = 8;
        } else if (next == 0 || next == 43 || next == 60) {
            length = ((__u32)p[1] + 1) * 8;
        } else if (next == 51) {
            length = ((__u32)p[1] + 2) * 4;
        } else{
            return 0;
        }
        if (length > remaining || p + length > end)
            return 0;
        next = p[0];
        p += length;
        remaining -= length;
    }
    return next == 6 ? tcp_synfin(p, end, remaining) : 0;
}

SEC("xdp")
int synfin_filter(struct xdp_md *ctx)
{
    __u8 *data = (__u8 *)(long)ctx->data;
    __u8 *end = (__u8 *)(long)ctx->data_end;
    return synfin(data, end) ? XDP_DROP : XDP_PASS;
}

char _license[] SEC("license") = "GPL";
