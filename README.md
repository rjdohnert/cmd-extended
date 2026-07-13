cmd-extended
------------
This is a collection of command line tools for the Windows default shell cmd.exe.

Commands
--------

- **reboot** - Reboots the system
- **file-manager** - Command line file manager with many functions. When you open file manager type "help" for a full list of options
- **security-events** - Similar to dmesg in that it outputs the contents of the Windows security event log. Admin privileges are needed
- **app-events** - Similar to dmesg in that it outputs the contents of the Windows application event log. Admin privileges are needed
- **system-events** - Similar to dmesg in that it outputs the contents of the Windows system event log. Admin privileges are needed
- **calculator** - Command line calculator that allows you to do simple or complex calculations
- **calendar** - List the current date and a calendar where you can chose a specific date
- **driveinfo** - Lists all available drives, filesystem type, used and free space
- **list-bluetooth** - Lists all available Bluetooth devices
- **cpuinfo** - Lists the processor installed on your system
- **list-pci** - Lists all PCI devices installed on your system
- **list-usb** - Lists all available USB devices on your system
- **meminfo** - Lists all memory statistics on your system including virtual memory
- **sudo** - Launches an admin console. sudo -cmd launches an admin cmd session. --help shows all options
- **version** - Lists system information including architecture type and Windows NT version. --help list all options. Similar to UNAME in UNIX
- **system-monitor** - Lists all running processes, memory usage and how many processes running
- **accountinfo** - Shows all user accounts on the system
- **userinfo** - Shows all logged in users on the system and the amount of time they have been logged in
- **clock** - Lists the current date and time
- **logout** - Ends current user session
- **wget** - Retrieve files over the network via the command-line

History
-------
I developed these tools to extend the capabilities of cmd.exe to bring it more in line with UNIX in 2026.
I was working late one night and fired up cmd.exe and started working on the command line and said:
"This sucks" and went to work correcting the issues.  These tools are not perfect by any means.  They 
were made by ME for ME, I just decided to share.  If you did download and used these "Thanks!!" if you
found them useful too; GREAT.

Systems Tested
--------------
Windows 10/11 Home and Pro.  Windows 10/11 Enterprise.  Windows Server 2022 and Windows Server 2025; Core and with 
desktop

License
-------
These tools are licensed under the BSD 4 Clause license

Installer
---------
MSI location:
- Release/cmd-extended-installer.msi

Installation (GUI):
1. Right-click Release/cmd-extended-installer.msi.
2. Select Run as administrator.
3. Accept the UAC prompt.
4. Complete the installer wizard.

Installation (Command Line):
1. Open an elevated Command Prompt (Run as administrator).
2. Run:
	msiexec /i "Release\cmd-extended-installer.msi"

Silent install with log:
- msiexec /i "Release\cmd-extended-installer.msi" /qn /L*V "Release\install.log"

Notes:
- The installer requires administrator privileges.
- Executables are installed into C:\Windows\System32.
