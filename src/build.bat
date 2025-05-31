gcc service_install.c -o service_install.exe
gcc main.c -o rpip.exe -lkernel32 -lshell32 -lwtsapi32