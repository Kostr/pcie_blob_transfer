# Description

This program performs a transfer of PCIe device information from Host to BMC over IPMI BLOB protocol.

# Usage

Install the latest meson:
```
$ pip3 install --user meson
$ source ~/.profile
```
Build [ipmi-blob-tool](https://github.com/openbmc/ipmi-blob-tool) library:
```
$ cd ~
$ git clone https://github.com/openbmc/ipmi-blob-tool.git
$ cd ipmi-blob-tool
$ meson setup builddir
$ cd builddir
$ meson compile
```
Build and run `pcie_blob_transfer` (change `user` to your name):
```
$ cd ~
$ git clone https://github.com/Kostr/pcie_blob_transfer.git
$ cd pcie_blob_transfer/OS
$ g++ pcie_blob_transfer.cpp -L/home/user/ipmi-blob-tool/builddir/src/ -lipmiblob -lpci -I/home/user/ipmi-blob-tool/src/ -o pcie_blob_transfer
$ sudo LD_LIBRARY_PATH=~/ipmi-blob-tool/builddir/src/ ./pcie_blob_transfer
```

Here is an example of the program output:
```
BLOB 0: /pcie
SessionID = 49899
Send 21:0.0
Send 01:0.1
Send 01:0.3
Send 21:0.1
Send 01:0.0
Send 01:0.2
Commit
BLOB 1: /smbios
```

After the program execution this file should be created on the BMC:
```
/var/lib/pcie/pcie
```

If you don't want to pass all the parameters for dynamic library linking, you can install `ipmi-blob-tool` to your system.

# Systemd unit

Probably you'll want to execute `pcie_blob_transfer` program on every OS boot. Here is an example of a systemd unit that you can use:
```
[Unit]
Description=Transfer PCIe device information to OpenBMC
After=network.target

[Service]
Type=oneshot
Environment=LD_LIBRARY_PATH=/home/user/ipmi-blob-tool/builddir/src
ExecStart=/home/user/pcie_blob_transfer/OS/pcie_blob_transfer

[Install]
WantedBy=multi-user.target
```

Create service file with such content (replacing `user` with your actual username in paths) in the `/etc/systemd/system/` folder:
```
sudo vi /etc/systemd/system/openbmc-pcie.service
```

After that reload the systemd service files to include the new service:
```
systemctl daemon-reload
```

Start new service:
```
systemctl start openbmc-pcie.service
```

If everything works correctly enable service to be launched on every boot:
```
systemctl enable openbmc-pcie.service
```
