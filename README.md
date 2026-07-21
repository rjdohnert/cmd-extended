CrossShell BSD
------------
This is a collection of command line tools for the Windows command line based on the BSD coreutils.

Commands
--------

- **ls** lists all the files in the directory
- **cp** makes a copy of a file in the system
- **mv** moves a file to another directory
- **compress** Compresses a file or folder into a compressed archive supports .Z, .zip and .tar.gz
- **grep** used to find lines matching a given pattern in files or input streams. It supports regular expressions
- **awk** text-processing tool used to scan files line by line, match patterns, and perform actions such as printing or calculations.
- **sed** non-interactive text editor that processes input line-by-line, applying transformations based on given commands. It’s widely   used for searching, replacing, inserting, deleting, and transforming text in files or streams.
- **pkg** UNIX style frontend for winget use the --help option for commands
- **tee**  reads from standard input and writes to both standard output and one or more files simultaneously, allowing you to capture command output while still displaying it.
- **wall** allows you to transmit a message to all users logged into a system
- **iostat** reports CPU utilization and input/output (I/O) statistics for devices and partitions, helping administrators monitor system performance and detect bottlenecks
- **pwd** Prints working directory
- **rm** removes a file or directory
- **sleep** pauses the execution of a script or command for a specified duration, allowing precise control over timing in shell scripts and automated tasks.
- **hostname** View your system hostname or make changes
- **tail** displays the last part of a file, commonly used to monitor logs or view recent updates.
- **cat** used to view, create, and concatenate file contents directly from the terminal.
- **touch** used to create empty files or update the access and modification timestamps of existing files
- **uniq** sed to filter out adjacent duplicate lines from text input, whether from a file or standard input. It’s often paired with sort because uniq only detects duplicates that are next to each other.
- **whoami** lists the current user logged into an account
- **uptime** lists the ammount of time the system has been currently powered on
- **attach** mounting or attaching Virtual Hard Disks (.vhd/.vhdx) or optical disc images (.iso) to the filesystem as read-only
- **reboot** - Reboots the system
- **file-manager** - Command line file manager with many functions. When you open file manager type "help" for a full list of options
- **security-events** - Similar to dmesg in that it outputs the contents of the Windows security event log. Admin privileges are needed
- **app-events** - Similar to dmesg in that it outputs the contents of the Windows application event log. Admin privileges are needed
- **system-events** - Similar to dmesg in that it outputs the contents of the Windows system event log. Admin privileges are needed
- **bc** - Command line calculator that allows you to do simple or complex calculations
- **cal** - List the current date and a calendar where you can chose a specific date
- **lsdrv** - Lists all available drives, filesystem type, used and free space
- **lsbt** - Lists all available Bluetooth devices
- **lscpu** - Lists the processor installed on your system
- **lspci** - Lists all PCI devices installed on your system
- **lsusb** - Lists all available USB devices on your system
- **mem** - Lists all memory statistics on your system including virtual memory
- **sudo** - Launches an admin console. sudo -cmd launches an admin cmd session. --help shows all options
- **uname** - Lists system information including architecture type and Windows NT version. --help list all options. Similar to UNAME in UNIX
- **system-monitor** - Lists all running processes, memory usage and how many processes running
- **listusers** - Shows all user accounts on the system
- **lsuser** - Shows all logged in users on the system and the amount of time they have been logged in
- **clock** - Lists the current date and time
- **logout** - Ends current user session
- **wget** - Retrieve files over the network via the command-line
- **htop** - Clone of the original UNIX utility.  This is a fork of htop4win developed by Mike Fara at Faratech

History
-------
I developed these tools to extend the capabilities of cmd.exe to bring some of the tools I was used to
using in BSD/UNX in 2026. I was working late one night and fired up cmd.exe and started working on the 
command line and said: "This sucks" and went to work correcting the issues.  These tools are not perfect by 
any means.  They were made by ME for ME, I just decided to share.  If you did download and used these "Thanks!!" 
if you found them useful; AWESOME. Either or thank you for stopping by and having a look!!!

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
