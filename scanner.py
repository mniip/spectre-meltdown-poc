import subprocess
import os
import sys
import time

def scan():
    
    if not os.geteuid() == 0:
        print("You must run this script as root")
        sys.exit(1)

    #enable kallsyms, display warning

    f = open("/proc/sys/kernel/kptr_restrict", "w")
    f.write("0")
    f.close()
    
    #avoid race condition/give time for kallsyms to be populated or whatever
    time.sleep(1)

    sys_call_table_addr = None
    for kallsym_line in open("/proc/kallsyms"):
        kallsym_line = kallsym_line.split(" ")
        kallsym_line_name = kallsym_line[2]
        if (kallsym_line_name.lower().strip() == "sys_call_table"):
            sys_call_table_addr = kallsym_line[0]            

    if sys_call_table_addr:
        print("Found your syscall table at %s! Attempting to abuse spec exec to find syscalls..." % sys_call_table_addr)
    else:
        sys.stderr.write("Couldn't find your syscall table, something has gone wrong. Please open a ticket on github and complain to me about this.\n")
        sys.exit(1)
            

    exit_code, output = subprocess.getstatusoutput("./scan " + sys_call_table_addr)

    if (exit_code != 0):
        print("Something went wrong, exiting with this error: ")
        print(output)
        sys.exit(exit_code)

    address_lines = output.split("\n")
    addresses_embedded = [x.split(" ") for x in address_lines]
    addresses = [item for sublist in addresses_embedded for item in sublist]    

    #read shit, do some string stuff, then see if we get hits in kallsyms, if yes then vulnerable
    kallsym_hits = 0
    for kallsym_line in open("/proc/kallsyms", "r"):
        kallsym_line_addr = kallsym_line.split(" ")[0]
        if kallsym_line_addr in addresses:
            print(kallsym_line.strip())
            kallsym_hits += 1

    if kallsym_hits > 0:
        print("We were able to find the above addresses of your syscall table using speculative execution (mis)training. You are likely vulnerable to SPECTRE and/or Meltdown.")
    else:
        print("We were not able to find any addresses from your syscall table using speculative execution (mis)training. You are likely not vulnerable to SPECTRE and/or Meltdown.")
    
    #disable kallsyms
    f = open("/proc/sys/kernel/kptr_restrict", "w")
    f.write("1")
    f.close()
    
if __name__ == "__main__":

    scan()
