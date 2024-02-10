## vmstated

`vmstated` is a very simple bhyve virtual machine manager for FreeBSD.

### How to install

`vmstated` can be installed via regular ports ([sysutils/vmstated](https://www.freshports.org/sysutils/vmstated/));
however, the version on Github is usually a bit ahead feature and bug-fixing wise.

To install from Github

```
git clone https://github.com/christian-moerz/vmstated
cd vmstated/port
make package
pkg install work/pkg/vmstated-0.06.pkg
```

To install from ports, open a terminal and run

```
pkg install vmstated
```

To uninstall vmstated, use the usual pkg command

```
pkg deinstall vmstated
```

### How to get started

Please refer to the man pages for `vmstated(8)` or `vmstatedctl(1)` for further
details on how to use `vmstated`.

To start `vmstated` run

```
sysrc vmstated_enable=YES
service vmstated start
```

- Configuration files go under `/usr/local/etc/vmstated`.
- Log files are under `/var/log/vmstated`
- PID file and socket are under `/var/run/`

