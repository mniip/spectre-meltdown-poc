Spectre and Meltdown Vuln Scanner
=====================================

This scanner first enables the /proc/kallsyms file to find your syscall table address.
Once the address is found, it attempts to exploit speculative execution (mis)training
to find syscalls near the syscall table. If it is able to do so it will report you
as vulnerable. If not, it will tell you that you are not vulnerable.

Usage:

```
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