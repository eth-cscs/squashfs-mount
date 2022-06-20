# squashfs-mount

`squashfs-mount [src.squashfs] [dst] command...` is a small setuid binary that
effectively runs `mount -n -o loop,nosuid,nodev,ro -t squashfs [src.squashfs] [dst]` in
a mount namespace and then executes the given command as the normal user.


## Motivation

squashfs blobs can be convenient to provide a full software stack: immutable,
single file, no need to extract. The downside is they can only be mounted
"rootless" (well, using setuid fusermount) with `squashfuse`, which has two
issues:

1. Bad performance compared to the Linux kernel. The following benchmark tests
   compilation performance of a hello world C program for a few compilers in a
   squashfs file (700MB compressed with zlib).

   Using `squashfuse 0.1.103` and `fuse 3.9.0`:
   
   ```
   Benchmark 1: ./squashfuse/gcc-10.3.0-ammihitysch7br7kke3pntiplfblqpdu/bin/gcc -c main.c
     Time (mean ± σ):      61.2 ms ±   3.1 ms    [User: 13.1 ms, System: 6.2 ms]
     Range (min … max):    53.5 ms …  70.3 ms    50 runs
   
   Benchmark 2: ./squashfuse/gcc-11.3.0-cm7ueuil6ppl3yp56esfcan6lmlis63b/bin/gcc -c main.c
     Time (mean ± σ):      58.8 ms ±   2.9 ms    [User: 13.0 ms, System: 7.0 ms]
     Range (min … max):    53.3 ms …  66.7 ms    52 runs
   
   Benchmark 3: ./squashfuse/gcc-12.1.0-zc3agmabcw3w22q3zlxopeangnn6dipp/bin/gcc -c main.c
     Time (mean ± σ):      54.4 ms ±   3.3 ms    [User: 13.6 ms, System: 6.7 ms]
     Range (min … max):    48.0 ms …  66.1 ms    49 runs
   
   Benchmark 4: ./squashfuse/gcc-9.5.0-dwbag4fgidanx5rgm2apidnajtpneza6/bin/gcc -c main.c
     Time (mean ± σ):      78.5 ms ±   3.9 ms    [User: 13.3 ms, System: 5.5 ms]
     Range (min … max):    74.0 ms …  90.0 ms    39 runs
   
   Benchmark 5: /usr/bin/gcc -c main.c
     Time (mean ± σ):      16.6 ms ±   1.7 ms    [User: 12.1 ms, System: 4.5 ms]
     Range (min … max):    11.8 ms …  20.4 ms    178 runs
   
   Summary
     '/usr/bin/gcc -c main.c' ran
       3.27 ± 0.39 times faster than './squashfuse/gcc-12.1.0-zc3agmabcw3w22q3zlxopeangnn6dipp/bin/gcc -c main.c'
       3.54 ± 0.40 times faster than './squashfuse/gcc-11.3.0-cm7ueuil6ppl3yp56esfcan6lmlis63b/bin/gcc -c main.c'
       3.68 ± 0.42 times faster than './squashfuse/gcc-10.3.0-ammihitysch7br7kke3pntiplfblqpdu/bin/gcc -c main.c'
       4.72 ± 0.54 times faster than './squashfuse/gcc-9.5.0-dwbag4fgidanx5rgm2apidnajtpneza6/bin/gcc -c main.c'
   ```

   Using `squashfs-mount` (or well, the `mount` convenience wrapper of this repo...):

   ```
   Benchmark 1: ./native/gcc-10.3.0-ammihitysch7br7kke3pntiplfblqpdu/bin/gcc -c main.c
     Time (mean ± σ):      15.7 ms ±   1.6 ms    [User: 11.0 ms, System: 4.6 ms]
     Range (min … max):    10.3 ms …  19.4 ms    236 runs
    
   Benchmark 2: ./native/gcc-11.3.0-cm7ueuil6ppl3yp56esfcan6lmlis63b/bin/gcc -c main.c
     Time (mean ± σ):      16.0 ms ±   1.8 ms    [User: 11.4 ms, System: 4.5 ms]
     Range (min … max):    10.6 ms …  19.6 ms    236 runs
    
   Benchmark 3: ./native/gcc-12.1.0-zc3agmabcw3w22q3zlxopeangnn6dipp/bin/gcc -c main.c
     Time (mean ± σ):      16.8 ms ±   1.6 ms    [User: 11.6 ms, System: 5.2 ms]
     Range (min … max):    11.3 ms …  20.3 ms    192 runs
    
   Benchmark 4: ./native/gcc-9.5.0-dwbag4fgidanx5rgm2apidnajtpneza6/bin/gcc -c main.c
     Time (mean ± σ):      15.3 ms ±   1.5 ms    [User: 11.1 ms, System: 4.1 ms]
     Range (min … max):    10.2 ms …  18.4 ms    195 runs
    
   Benchmark 5: /usr/bin/gcc -c main.c
     Time (mean ± σ):      16.5 ms ±   1.9 ms    [User: 11.9 ms, System: 4.6 ms]
     Range (min … max):    10.3 ms …  20.0 ms    187 runs
    
   Summary
     './native/gcc-9.5.0-dwbag4fgidanx5rgm2apidnajtpneza6/bin/gcc -c main.c' ran
       1.02 ± 0.15 times faster than './native/gcc-10.3.0-ammihitysch7br7kke3pntiplfblqpdu/bin/gcc -c main.c'
       1.04 ± 0.16 times faster than './native/gcc-11.3.0-cm7ueuil6ppl3yp56esfcan6lmlis63b/bin/gcc -c main.c'
       1.08 ± 0.17 times faster than '/usr/bin/gcc -c main.c'
       1.10 ± 0.15 times faster than './native/gcc-12.1.0-zc3agmabcw3w22q3zlxopeangnn6dipp/bin/gcc -c main.c'
   ```
   
2. Inconvenient when scripting, as illustrated by [NVIDIA
   enroot](https://github.com/NVIDIA/enroot):
   
   ```bash
   # Mount the image as the lower layer.
   squashfuse -f -o "uid=${euid},gid=${egid}" "${image}" "${rootfs}/lower" &
   pid=$!; i=0
   while ! mountpoint -q "${rootfs}/lower"; do
       ! kill -0 "${pid}" 2> /dev/null || ((i++ == timeout)) && exit 1
       sleep .001
   done
   ```

## Dependencies

- `util-linux` (libmount)

## Install instructions

```console
make
sudo chown root:root squashfs-mount
sudo chmod +s squashfs-mount
make install
```
