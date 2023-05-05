# Description

This folder contains fully independent `DXE_DRIVER` implementation that can be compiled with the modern [EDK2](https://github.com/tianocore/edk2). Instead of using some IPMI protocol from the BIOS vendor, this implementation has a fully working IPMI KCS transfer code. Besides the `DXE_DRIVER` this folder also contains an `UEFI_APPLICATION` to test the code in the UEFI Shell.

# Building with EDK2

Check out this guide for the process of [building a package with the modern EDK2](https://github.com/Kostr/smbios_blob_transfer/blob/main/UEFI/standalone/README.md#building-with-edk2).

The final images can be found in the `Build\OpenBmcPkg\RELEASE_GCC5\X64` folder:
- `PCIeBlob_App.efi`
- `PCIeBlob_Drv.efi`

# `PCIeBlob_App.efi`

First it is suggested to test `PCIeBlob_App.efi` implementation in the UEFI shell. If everything on the BMC side is configured correctly, the application should have an output similar to this:
```
FS0:\BLOB\> PCIeBlob_App.efi
BlobCount = 2
BlobEnumerate: id=0: /pcie
PCIe blob is found
BlobOpen
Session = 0xC2F5
BlobSendPCIeDevices
Send 01:00.0, offset=0
Send 01:00.1, offset=264
Send 01:00.2, offset=528
Send 01:00.3, offset=792
Send 21:00.0, offset=1056
Send 21:00.1, offset=1320
BlobCommit
BlobClose
PCIe device information was successfully transmitted to the BMC
BlobEnumerate: id=1: /smbios
```

If it is not, you can turn on debug for IPMI messages:
```cpp
#define DEBUG_IPMI_MESSAGES
```

If nothing works check that your KCS port is `0xCA2`, or modify the value passed to the `ExecuteIpmiCmd` function.

If just a commit operation fails, you might try to increase timeout argument:
```cpp
EFI_STATUS Status = ExecuteIpmiCmd(
                        500000,			// timeout
                        0xCA2,			// kcs port
                        (VOID*)SendBuf,
                        SendLength,
                        (VOID*)ReceiveBuf,
                        &ReceiveLength
                    );
```

# `PCIeBlob_Drv.efi`

If `PCIeBlob_App.efi` works, you can check that `PCIeBlob_Drv.efi` also transfer PCIe devices information.

For that execute this command in UEFI Shell:
```
FS0:\> load PCIeBlob_Drv.efi
```

This wouldn't produce any output, but you can check BMC logs to check that information was transfered once again.
```
~# journalctl -u "pcie-mdrv2.service" -f
```

# Implementation details

The implementation of the IPMI KCS transfer is mainly based on the one from the `edk2-platfroms` repo:
- [KcsBmc.c](https://github.com/tianocore/edk2-platforms/blob/ab9805e0020b413232e1abd8d6e6624c98f63816/Features/Intel/OutOfBandManagement/IpmiFeaturePkg/GenericIpmi/Common/KcsBmc.c)
- [KcsBmc.h](https://github.com/tianocore/edk2-platforms/blob/ab9805e0020b413232e1abd8d6e6624c98f63816/Features/Intel/OutOfBandManagement/IpmiFeaturePkg/GenericIpmi/Common/KcsBmc.h)
- [IpmiTransportProtocol.h](https://github.com/tianocore/edk2-platforms/blob/ab9805e0020b413232e1abd8d6e6624c98f63816/Features/Intel/OutOfBandManagement/IpmiFeaturePkg/Include/Protocol/IpmiTransportProtocol.h)
- [ServerManagement.h](https://github.com/tianocore/edk2-platforms/blob/ab9805e0020b413232e1abd8d6e6624c98f63816/Features/Intel/OutOfBandManagement/IpmiFeaturePkg/Include/ServerManagement.h)

The main change is that `MicroSecondDelay` function here doesn't come from the `TimerLib` but is implemented via the standard boot services `Stall` call. This makes the code more portable.

This change is performed in the `KcsBmc.h` and looks like this:
```
- #include <Library/TimerLib.h>
+ #include <Library/UefiBootServicesTableLib.h>

+ #define MicroSecondDelay(X) (gBS->Stall(X))
```

# Modifying closed-sourced BIOS with `UEFITool`

If you want to embed your module to the closed-source BIOS check out [this guide](https://github.com/Kostr/smbios_blob_transfer/blob/main/UEFI/standalone/README.md#modifying-closed-sourced-bios-with-uefitool).

We need to insert not just the `PCIeBlob_Drv.efi` (PE32 image), but an FFS file containing PE32 image as a section. In our case it is produced here:
```
Build/OpenBmcPkg/RELEASE_GCC5/FV/Ffs/7be7246e-8ea1-413a-99fd-a8bd013266d2PCIeBlob_Drv/7be7246e-8ea1-413a-99fd-a8bd013266d2.ffs
```
