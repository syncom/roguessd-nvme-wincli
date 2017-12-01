# roguessd-nvme-wincli
Windows 10 userspace application for Sending Rogue SSD needed NVME commands

This application sends a vendor-specific command to the device, using CDW15 to hold authentication data.

# References
1. [Working with NVMe drives](https://msdn.microsoft.com/en-us/library/windows/desktop/mt718131(v=vs.85).aspx)
2. [Calling DeviceIoControl](https://msdn.microsoft.com/en-us/library/windows/desktop/aa363147(v=vs.85).aspx)
3. [Where is nvme.h include file on Windows 10 Kit?](https://social.msdn.microsoft.com/Forums/en-US/24f6bf24-7545-4863-858b-3c8876109b53/where-is-nvmeh-include-file-on-windows-10-kit?forum=windowsgeneraldevelopmentissues)
