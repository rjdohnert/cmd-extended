cmd-extended
------------
This is a collection of command line tools for the Windows default shell cmd.exe.  You can find a full list
of commands and what they do in the commands list file in the Release folder.

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
