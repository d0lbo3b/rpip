gcc service_install.c -o service_install.exe -O3
gcc main.c -o rpip.exe -lkernel32 -lshell32 -lwtsapi32 -O3