@echo off
sc create "rpip" binPath= "\"D:\projects\C\rpip\src\rpip.exe\" Notepad.exe \"D:\projects\C\heart\src\build\bruh.exe\" bruh.exe" start= auto
sc start rpip
pause
