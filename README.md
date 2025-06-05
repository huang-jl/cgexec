# Cgexec

A simple tool that used to spawn processes under a specific cgroup.

## Compile and Install

```bash
# compile the cgexec and install it into /usr/local/bin
make build && make install
```

## Examples

```bash
# start a python script, will be located in /sys/fs/cgroup/some_cgroup_path
cgexec some_cgroup_path python hello_world.py

# start a python script with arguments, will be localted in /sys/fs/cgroup/A/B
cgexec A/B python hello_world.py --args a --args b
```

## Features

- Use the new `CLONE_INTO_CGROUP` flag in `clone()` syscall.
- Auto create the cgroup path if not exists.

## Notes

- Only support on Linux >= 5.7 and cgroup v2.

