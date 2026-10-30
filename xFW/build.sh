#!/bin/bash

# 构建 xFW 运行时镜像，并导出 rootfs
IMAGE="registry.lqingcloud.cn/library/xfw:ubuntu24.04"
ROOTFS="rootfs"
ROOTFS_ARCHIVE="rootfs.tar.gz"
CONTAINER_ID=""

docker build --network=host  -t ${IMAGE} -f Dockerfile .
rm -rf "$ROOTFS" "$ROOTFS_ARCHIVE"
mkdir -p "$ROOTFS"
CONTAINER_ID=$(docker create "$IMAGE")
docker export "$CONTAINER_ID" | tar -xpf - -C "$ROOTFS"
docker rm "$CONTAINER_ID"
tar -czpf "$ROOTFS_ARCHIVE" "$ROOTFS"

rm  -rf "$ROOTFS"
echo "success export rootfs to $ROOTFS_ARCHIVE"