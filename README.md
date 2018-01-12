Spectre and Meltdown Vuln Scanner
=====================================

This scanner first enables the /proc/kallsyms file to find your syscall table address.
Once the address is found, it attempts to exploit speculative execution (mis)training
to find syscalls near the syscall table. If it is able to do so it will report you
as vulnerable. If not, it will tell you that you are not vulnerable.

Usage:

```
punk@dtsp-server ~/dev/spectre-meltdown-poc $ ls
btb.c  Makefile  README.md  scan.c  scanner.py

punk@dtsp-server ~/dev/spectre-meltdown-poc $ make
cc -o scan scan.c -O0 -std=gnu99 -g -lm 
scan.c: In function ‘read_byte’:
scan.c:266:10: warning: format ‘%g’ expects argument of type ‘double’, but argument 2 has type ‘int’ [-Wformat=]
   printf("%20.13g %02x", 0, 0);
          ^
scan.c: In function ‘main’:
scan.c:360:18: warning: assignment makes integer from pointer without a cast [-Wint-conversion]
    first_addr[9] = NULL;
                  ^
scan.c:361:19: warning: assignment makes integer from pointer without a cast [-Wint-conversion]
    second_addr[9] = NULL;
                   ^

punk@dtsp-server ~/dev/spectre-meltdown-poc $ ls
btb.c  Makefile  README.md  scan  scan.c  scanner.py
punk@dtsp-server ~/dev/spectre-meltdown-poc $ sudo python3 scanner.py
Found your syscall table at ffffffff88c00200! Attempting to abuse spec exec to find syscalls...
ffffffff8842f820 T SyS_close
ffffffff8842f820 T sys_close
ffffffff884317e0 T SyS_open
ffffffff884317e0 T sys_open
ffffffff884341e0 T SyS_read
ffffffff884341e0 T sys_read
ffffffff884342a0 T SyS_write
ffffffff884342a0 T sys_write
ffffffff88438950 T SyS_newstat
ffffffff88438950 T sys_newstat
ffffffff88438960 T SyS_newlstat
ffffffff88438960 T sys_newlstat
ffffffff88438980 T SyS_newfstat
ffffffff88438980 T sys_newfstat
ffffffff8844a1b0 T SyS_poll
ffffffff8844a1b0 T sys_poll
We were able to find the above addresses of your syscall table using speculative execution (mis)training. You are likely vulnerable to SPECTRE and/or Meltdown.
```
