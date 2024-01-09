## vmstated

`vmstated` is a very simple bhyve virtual machine manager for FreeBSD.

### How to compile

Eventually, `vmstated` will be released via the regular ports channel; until then,
one can run the following commands:

```
git clone https://github.com/christian-moerz/vmstated
cd vmstated/port
make package
pkg install work/pkg/vmstated-0.01.pkg
```

To uninstall vmstated, use the usual pkg command

```
pkg deinstall vmstated
```
