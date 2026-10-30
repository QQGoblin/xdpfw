%global app_dir /opt/tempesta
%global unit_dir %{_sysconfdir}/systemd/system
%global debug_package %{nil}

Name:           xFW
Version:        1.1.0
Release:        1%{?dist}
Summary:        XDP firewall
License:        GPL-2.0-or-later
ExclusiveArch:  x86_64

Source0:        rootfs.tar.gz
Source1:        xfw.json
Source2:        xfwctl
Source3:        xfw.service
Source4:        xFW.sh
Source5:        rules.tar.gz

BuildRequires:  coreutils
BuildRequires:  tar
BuildRequires:  gzip
Requires:       python3 >= 3.6
Requires:       util-linux
Requires:       systemd
Requires(pre):  systemd
Requires(post): coreutils, tar, gzip
Requires(preun): systemd
Requires(postun): coreutils, systemd
Requires(posttrans): systemd

%description
xFW root filesystem, host command-line wrapper, configuration,
and systemd service.

%prep
%setup -q -c -T
tar -xzf "%{SOURCE0}"
tar -xzf "%{SOURCE5}"
install -m 0755 "%{SOURCE4}" rootfs/opt/tempesta/xFW.sh

%install
install -d -m 0755 "%{buildroot}%{app_dir}"
tar -czf "%{buildroot}%{app_dir}/rootfs.tar.gz" rootfs
install -D -m 0644 "%{SOURCE1}" "%{buildroot}%{_sysconfdir}/tempesta/xfw.json"
install -d -m 0755 "%{buildroot}%{_sysconfdir}/tempesta/rules"
cp -a rules/. "%{buildroot}%{_sysconfdir}/tempesta/rules/"
install -D -m 0755 "%{SOURCE2}" "%{buildroot}/usr/local/bin/xfwctl"
ln -s xfwctl "%{buildroot}/usr/local/bin/tfw"
install -D -m 0644 "%{SOURCE3}" "%{buildroot}%{unit_dir}/xfw.service"

%pre
set -e
if [ -d /run/systemd/system ] && systemctl cat xfw.service >/dev/null 2>&1; then
    systemctl stop xfw.service
fi

%post
set -e
rm -rf -- "%{app_dir}/rootfs"
tar --numeric-owner -xzpf "%{app_dir}/rootfs.tar.gz" -C "%{app_dir}"

%posttrans
set -e
if [ -d /run/systemd/system ]; then
    systemctl daemon-reload
    systemctl restart xfw.service
fi

%preun
set -e
if [ "$1" -eq 0 ] && [ -d /run/systemd/system ]; then
    systemctl stop xfw.service
    systemctl disable xfw.service >/dev/null 2>&1 || :
fi

%postun
set -e
if [ "$1" -eq 0 ]; then
    rm -rf -- "%{app_dir}/rootfs"
    rmdir -- "%{app_dir}" 2>/dev/null || :
    if [ -d /run/systemd/system ]; then
        systemctl daemon-reload
    fi
fi

%files
%defattr(-,root,root,-)
%dir %{app_dir}
%{app_dir}/rootfs.tar.gz
%dir %{_sysconfdir}/tempesta
%{_sysconfdir}/tempesta/xfw.json
%{_sysconfdir}/tempesta/rules
/usr/local/bin/xfwctl
/usr/local/bin/tfw
%{unit_dir}/xfw.service
