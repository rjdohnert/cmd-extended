<img width="300" height="170" alt="freebsd-alongside-windows" src="https://github.com/user-attachments/assets/c9d1a7c4-bc15-4f44-bb96-02db22b38157" />

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
- **csplit** splitting a file into two or more smaller files determined by context lines.
- **cut** extracts sections from each line of input text — usually from a file
- **dd**  used for reading, writing and converting file data.
- **df** reports the amount of available and consumed storage space on a file system.
- **du** reports file system storage allocated to files and directory trees
- **diff** compares two files for similarities or differences
- **dirname**  extracts the directory path portion of a path, without the last name. The command is specified in the Single UNIX Specification and is primarily used in shell scripts.
- **head**  used to display the beginning of a text file or piped data.
- **m4** is a general-purpose macro processor included in most Unix-like operating systems, and is a component of the POSIX standard. On Windows this is a work-a-like as Windows lacks POSIX compliance without WSL, SFU, Cygwin or MSYS
- **md5sum** calculates and verifies 128-bit MD5 hashes, as described in RFC 1321. The MD5 hash functions as a compact digital fingerprint of a file. 
- **sha256sum**  used to compute and check SHA256 (256-bit) checksums. This is a work alike and not a release of the version in the GNU CoreUtils
- **paste**joins files horizontally (parallel merging) by writing to standard output lines consisting of the sequentially corresponding lines of each input file, separated by tabs.
- **printenv** used to display environment variables. It can print all variables or only specific ones if their names are provided.
- **rsync**  is a versatile command-line utility for synchronizing files and directories between two locations. It is commonly used for mirroring data, incremental backups, and copying files between systems.
- **seq** utility for generating a sequence of numbers.
- **split** utility on Unix, Plan 9, and Unix-like operating systems most commonly used to split a computer file into two or more smaller files.
- **wc** is a command in Unix, Plan 9, Inferno, and operating systems that are Unix-like. The program reads either standard input or a list of computer files and generates one or more of the following statistics: newline count, word count, and byte count. If a list of files is provided, both individual file and total statistics follow.
- **wipe** Securely deletes a file and makes recovery almost impossible
- **xargs** is a command on Unix and most Unix-like operating systems used to build and execute commands from standard input. It converts input from standard input into arguments to a command.
- **tail** displays the last part of a file, commonly used to monitor logs or view recent updates.
- **cat** used to view, create, and concatenate file contents directly from the terminal.
- **touch** used to create empty files or update the access and modification timestamps of existing files
- **uniq** sed to filter out adjacent duplicate lines from text input, whether from a file or standard input. It’s often paired with sort because uniq only detects duplicates that are next to each other.
- **whoami** lists the current user logged into an account
- **uptime** lists the ammount of time the system has been currently powered on
- **attach** mounting or attaching Virtual Hard Disks (.vhd/.vhdx) or optical disc images (.iso) to the filesystem as read-only
- **comm** used to compare two sorted files line by line. By default, it produces three columns:
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
- **tsort** utility that performs topological sorting. It reads pairs of items (strings) from input, interprets them as directed edges in a graph, and outputs a linear ordering that respects the dependencies.
- **zip** compresses a file into the .zip format
- **unzip** decompresses files in the .zip format
- **dircolors** command that outputs commands to set the LS_COLORS environment variable for the ls command
- **system-monitor** - Lists all running processes, memory usage and how many processes running
- **listusers** - Shows all user accounts on the system
- **lsuser** - Shows all logged in users on the system and the amount of time they have been logged in
- **clock** - Lists the current date and time
- **logout** - Ends current user session
- **wget** - Retrieve files over the network via the command-line
- **top** - Clone of the original UNIX utility.

History
-------
I developed these tools to extend the capabilities of cmd.exe to bring some of the tools I was used to
using in BSD/UNX in 2026. I found UnxUtils but that project is dead and hasnt been updated in over 10 years.
I was working late one night and fired up cmd.exe and started working on the command line and said: "This sucks" 
and went to work correcting the issues.  I started producing these tools in May of 2025 and they have been a labor.  
These tools are not perfect by any means.  They were made by ME for ME, I just decided to share.  If you did download 
and used these "Thanks!!" if you found them useful; AWESOME.  Either or thank you for stopping by and having a look!!!

Disclaimer
-----------
Many of these tools share a similar name with their GNU or BSD counterparts but are functionally and structurely different to
reduce licensing issues with the GPL and in areas where POSIX complaince was near impossible.  Whenever source code was observed for actual BSD utilities BSD source code was observed no Linux/GNU source code was observed during the production of these utilities.
GNU and the GPL are properties are registered trademarks of The Free Software Foundation.  FreeBSD is a registered Trademark of
the FreeBSD foundation.  Linux is a registered trademark of Linus Torvals.  Windows/Windows Server are registered trademarks of
Microsoft Corporation

Systems Tested
--------------
Windows 10/11 Home and Pro.  Windows 10/11 Enterprise.  Windows Server 2022 and Windows Server 2025; Core and with 
desktop

License
-------
These tools are licensed under the BSD 3 Clause license

Installer
---------
MSI location:
- Release/ folder or under the Releases section

Installation (GUI):
1. Right-click Release/CrossShell-BSD-3.0-x64.msi.
2. Select Run as administrator.
3. Accept the UAC prompt.
4. Complete the installer wizard.

Installation (Command Line):
1. Open an elevated Command Prompt (Run as administrator).
2. Run:msi
	msiexec /i "Release\CrossShell-BSD-3.0-x64.msi"

Silent install with log:
- msiexec /i "Release\CrossShell-BSD-3.0-x64.msi" /qn /L*V "Release\install.log"

Notes:
- The installer requires administrator privileges.
- Executables are installed into C:\Windows\System32.
