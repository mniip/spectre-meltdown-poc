import subprocess
import os


if __name__ == "__main__":

    if not os.geteuid() == 0:
        print("You must run this script with root privileges")
        sys.exit(1)

    #enable kallsyms, display warning
        
    _proc = subprocess.popen("./scan", shell=True)

    _proc.communicate()

    #disable kallsyms
