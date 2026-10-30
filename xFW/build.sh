#!/bin/bash
set -euo pipefail

# 构建 xFW 运行时镜像，并导出 rootfs
IMAGE="registry.lqingcloud.cn/library/xfw:ubuntu24.04"
ROOTFS="rootfs"
ROOTFS_ARCHIVE="rootfs.tar.gz"
CONTAINER_ID="xfw-exporter"

docker build --network=host  -t ${IMAGE} -f Dockerfile .
rm -rf "$ROOTFS" "$ROOTFS_ARCHIVE"
mkdir -p "$ROOTFS"
CONTAINER_ID=docker create --name "$IMAGE" 
docker export "$CONTAINER_ID" | tar -xpf - -C "$ROOTFS"
docker rm "$CONTAINER_ID"
tar -czpf "$ROOTFS_ARCHIVE" "$ROOTFS"

rm  -rf "$ROOTFS"
echo "success export rootfs to $ROOTFS_ARCHIVE"


# 构建 xFW RPM 包
RPM_OUTPUT_DIR=dist
RPMBUILD_DIR="rpmbuild"

rm -rf -- "$RPMBUILD_DIR" "$RPM_OUTPUT_DIR"
mkdir -p "$RPMBUILD_DIR"/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS} "$RPM_OUTPUT_DIR"
install -m 0644 "$ROOTFS_ARCHIVE" "$RPMBUILD_DIR/SOURCES/rootfs.tar.gz"
install -m 0644 hack/xfw.json "$RPMBUILD_DIR/SOURCES/xfw.json"
install -m 0644 hack/xfwctl "$RPMBUILD_DIR/SOURCES/xfwctl"
install -m 0644 hack/xfw.service "$RPMBUILD_DIR/SOURCES/xfw.service"
install -m 0644 docker/xFW.sh "$RPMBUILD_DIR/SOURCES/xFW.sh"
install -m 0644 rpm.spec "$RPMBUILD_DIR/SPECS/rpm.spec"

# 构建二进制 RPM，rpmbuild 会在输出目录下创建架构子目录
rpmbuild -bb \
    --define "_topdir $RPMBUILD_DIR" \
    --define "_rpmdir $RPM_OUTPUT_DIR" \
    "$RPMBUILD_DIR/SPECS/rpm.spec"

echo "success build xFW rpm package, output dir: $RPM_OUTPUT_DIR"
