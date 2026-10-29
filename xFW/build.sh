#!/bin/bash

IMAGE="registry.lqingcloud.cn/library/xfw:ubuntu24.04"
ROOTFS="rootfs"
CONTAINER_NAME="xfw-exporter"

# 构建 xFW 运行时镜像
docker build --network=host  -t ${IMAGE} -f Dockerfile .

# 导出 rootfs
rm -rf "$ROOTFS"
mkdir -p "$ROOTFS"
docker create --name "$CONTAINER_NAME" "$IMAGE" 
docker export "$CONTAINER_NAME" | tar -xpf - -C "$ROOTFS"
docker rm "$CONTAINER_NAME"

rm -rf rootfs.tar.gz
tar -czpf rootfs.tar.gz rootfs
rm  -rf rootfs
echo "success"
