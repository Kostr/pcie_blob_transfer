# Description

This repo contains utilities to transfer PCIe device information from Host to BMC over IPMI BLOB protocol.

Two implementations are available:
- [transfer PCIe device information from the UEFI firmware (`DXE_DRIVER`)](UEFI)
- [transfer PCIe device information from the OS](OS)

# BMC requirements

BMC firmware should be based on the [openbmc](https://github.com/openbmc/openbmc) distribution.

You need to add [pcie-mdr](https://github.com/Kostr/pcie-mdr) and [phosphor-ipmi-blobs](https://github.com/openbmc/phosphor-ipmi-blobs) packages to the image.

`pcie-mdr` is not a part of the openbmc software stack, so you have to add the recipe manually.

Create the file `meta-phosphor/recipes-phosphor/pcie/pcie-mdr_git.bb` with the following content:
```
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://LICENSE;md5=e3fc50a88d0a364313df4b21ef20c29e"

DEPENDS += " \
    boost \
    systemd \
    sdbusplus \
    phosphor-logging \
    phosphor-ipmi-blobs \
    "

SRC_URI = "git://github.com/Kostr/pcie-mdr.git;protocol=https;branch=master"

PV = "1.0+git${SRCPV}"
SRCREV = "e291a6e688c77aee443555a00f8a39216d7295ee"

S = "${WORKDIR}/git"

SYSTEMD_SERVICE:${PN} += "pcie-mdrv2.service"

inherit cmake pkgconfig systemd
inherit obmc-phosphor-ipmiprovider-symlink

FILES:${PN}:append = " ${libdir}/ipmid-providers/lib*${SOLIBS}"
FILES:${PN}:append = " ${libdir}/blob-ipmid/lib*${SOLIBS}"
FILES:${PN}-dev:append = " ${libdir}/ipmid-providers/lib*${SOLIBSDEV}"

BLOBIPMI_PROVIDER_LIBRARY += "libpciestore.so"
```

Change `SRCREV` to point to the latest commit in the `pcie-mdr` repository.

# Results

## D-Bus

After the successful transfer of PCIe device information you can check out the objects created by the `xyz.openbmc_project.PCIe.MDRV2` service.

For example:
```
root@ethanolx:~# busctl tree xyz.openbmc_project.PCIe.MDRV2
`-/xyz
  `-/xyz/openbmc_project
    |-/xyz/openbmc_project/PCIe_MDRV2
    `-/xyz/openbmc_project/inventory
      `-/xyz/openbmc_project/inventory/pcie
        |-/xyz/openbmc_project/inventory/pcie/Bus_01_Device_00
        `-/xyz/openbmc_project/inventory/pcie/Bus_21_Device_00
```

Here are you can see 2 PCIe devices:
- Intel 4 port Ethernet adapter (on the host it is represented as 4 functions: `01:00.0`/`01:00.1`/`01:00.2`/`01:00.3`),
- NVIDIA graphic card (on the host it is represented as 2 functions: `21:00.0`/`21:00.1`).

You can introspect the devices:
```
root@ethanolx:~# busctl introspect xyz.openbmc_project.PCIe.MDRV2 /xyz/openbmc_project/inventory/pcie/Bus_01_Device_00
NAME                                          TYPE      SIGNATURE RESULT/VALUE                             FLAGS
org.freedesktop.DBus.Introspectable           interface -         -                                        -
.Introspect                                   method    -         s                                        -
org.freedesktop.DBus.Peer                     interface -         -                                        -
.GetMachineId                                 method    -         s                                        -
.Ping                                         method    -         -                                        -
org.freedesktop.DBus.Properties               interface -         -                                        -
.Get                                          method    ss        v                                        -
.GetAll                                       method    s         a{sv}                                    -
.Set                                          method    ssv       -                                        -
.PropertiesChanged                            signal    sa{sv}as  -                                        -
xyz.openbmc_project.Inventory.Item.PCIeDevice interface -         -                                        -
.DeviceType                                   property  s         "MultiFunction"                          emits-change writable
.Function0ClassCode                           property  s         "0x020000"                               emits-change writable
.Function0DeviceClass                         property  s         "NetworkController"                      emits-change writable
.Function0DeviceId                            property  s         "0x1521"                                 emits-change writable
.Function0FunctionType                        property  s         "Physical"                               emits-change writable
.Function0RevisionId                          property  s         "0x01"                                   emits-change writable
.Function0SubsystemId                         property  s         "0x0001"                                 emits-change writable
.Function0SubsystemVendorId                   property  s         "0x8086"                                 emits-change writable
.Function0VendorId                            property  s         "0x8086"                                 emits-change writable
.Function1ClassCode                           property  s         "0x020000"                               emits-change writable
.Function1DeviceClass                         property  s         "NetworkController"                      emits-change writable
.Function1DeviceId                            property  s         "0x1521"                                 emits-change writable
.Function1FunctionType                        property  s         "Physical"                               emits-change writable
.Function1RevisionId                          property  s         "0x01"                                   emits-change writable
.Function1SubsystemId                         property  s         "0x0001"                                 emits-change writable
.Function1SubsystemVendorId                   property  s         "0x8086"                                 emits-change writable
.Function1VendorId                            property  s         "0x8086"                                 emits-change writable
.Function2ClassCode                           property  s         "0x020000"                               emits-change writable
.Function2DeviceClass                         property  s         "NetworkController"                      emits-change writable
.Function2DeviceId                            property  s         "0x1521"                                 emits-change writable
.Function2FunctionType                        property  s         "Physical"                               emits-change writable
.Function2RevisionId                          property  s         "0x01"                                   emits-change writable
.Function2SubsystemId                         property  s         "0x0001"                                 emits-change writable
.Function2SubsystemVendorId                   property  s         "0x8086"                                 emits-change writable
.Function2VendorId                            property  s         "0x8086"                                 emits-change writable
.Function3ClassCode                           property  s         "0x020000"                               emits-change writable
.Function3DeviceClass                         property  s         "NetworkController"                      emits-change writable
.Function3DeviceId                            property  s         "0x1521"                                 emits-change writable
.Function3FunctionType                        property  s         "Physical"                               emits-change writable
.Function3RevisionId                          property  s         "0x01"                                   emits-change writable
.Function3SubsystemId                         property  s         "0x0001"                                 emits-change writable
.Function3SubsystemVendorId                   property  s         "0x8086"                                 emits-change writable
.Function3VendorId                            property  s         "0x8086"                                 emits-change writable
.Function4ClassCode                           property  s         ""                                       emits-change writable
.Function4DeviceClass                         property  s         ""                                       emits-change writable
.Function4DeviceId                            property  s         ""                                       emits-change writable
.Function4FunctionType                        property  s         ""                                       emits-change writable
.Function4RevisionId                          property  s         ""                                       emits-change writable
.Function4SubsystemId                         property  s         ""                                       emits-change writable
.Function4SubsystemVendorId                   property  s         ""                                       emits-change writable
.Function4VendorId                            property  s         ""                                       emits-change writable
.Function5ClassCode                           property  s         ""                                       emits-change writable
.Function5DeviceClass                         property  s         ""                                       emits-change writable
.Function5DeviceId                            property  s         ""                                       emits-change writable
.Function5FunctionType                        property  s         ""                                       emits-change writable
.Function5RevisionId                          property  s         ""                                       emits-change writable
.Function5SubsystemId                         property  s         ""                                       emits-change writable
.Function5SubsystemVendorId                   property  s         ""                                       emits-change writable
.Function5VendorId                            property  s         ""                                       emits-change writable
.Function6ClassCode                           property  s         ""                                       emits-change writable
.Function6DeviceClass                         property  s         ""                                       emits-change writable
.Function6DeviceId                            property  s         ""                                       emits-change writable
.Function6FunctionType                        property  s         ""                                       emits-change writable
.Function6RevisionId                          property  s         ""                                       emits-change writable
.Function6SubsystemId                         property  s         ""                                       emits-change writable
.Function6SubsystemVendorId                   property  s         ""                                       emits-change writable
.Function6VendorId                            property  s         ""                                       emits-change writable
.Function7ClassCode                           property  s         ""                                       emits-change writable
.Function7DeviceClass                         property  s         ""                                       emits-change writable
.Function7DeviceId                            property  s         ""                                       emits-change writable
.Function7FunctionType                        property  s         ""                                       emits-change writable
.Function7RevisionId                          property  s         ""                                       emits-change writable
.Function7SubsystemId                         property  s         ""                                       emits-change writable
.Function7SubsystemVendorId                   property  s         ""                                       emits-change writable
.Function7VendorId                            property  s         ""                                       emits-change writable
.GenerationInUse                              property  s         "xyz.openbmc_project.Inventory.Item.P... emits-change writable
.GenerationSupported                          property  s         "xyz.openbmc_project.Inventory.Item.P... emits-change writable
.LanesInUse                                   property  u         0                                        emits-change writable
.Manufacturer                                 property  s         "Intel Corporation"                      emits-change writable
.MaxLanes                                     property  u         0                                        emits-change writable

root@ethanolx:~# busctl introspect xyz.openbmc_project.PCIe.MDRV2 /xyz/openbmc_project/inventory/pcie/Bus_21_Device_00
NAME                                          TYPE      SIGNATURE RESULT/VALUE                             FLAGS
org.freedesktop.DBus.Introspectable           interface -         -                                        -
.Introspect                                   method    -         s                                        -
org.freedesktop.DBus.Peer                     interface -         -                                        -
.GetMachineId                                 method    -         s                                        -
.Ping                                         method    -         -                                        -
org.freedesktop.DBus.Properties               interface -         -                                        -
.Get                                          method    ss        v                                        -
.GetAll                                       method    s         a{sv}                                    -
.Set                                          method    ssv       -                                        -
.PropertiesChanged                            signal    sa{sv}as  -                                        -
xyz.openbmc_project.Inventory.Item.PCIeDevice interface -         -                                        -
.DeviceType                                   property  s         "MultiFunction"                          emits-change writable
.Function0ClassCode                           property  s         "0x030000"                               emits-change writable
.Function0DeviceClass                         property  s         "DisplayController"                      emits-change writable
.Function0DeviceId                            property  s         "0x13bb"                                 emits-change writable
.Function0FunctionType                        property  s         "Physical"                               emits-change writable
.Function0RevisionId                          property  s         "0xa2"                                   emits-change writable
.Function0SubsystemId                         property  s         "0x1098"                                 emits-change writable
.Function0SubsystemVendorId                   property  s         "0x10de"                                 emits-change writable
.Function0VendorId                            property  s         "0x10de"                                 emits-change writable
.Function1ClassCode                           property  s         "0x040300"                               emits-change writable
.Function1DeviceClass                         property  s         "MultimediaController"                   emits-change writable
.Function1DeviceId                            property  s         "0x0fbc"                                 emits-change writable
.Function1FunctionType                        property  s         "Physical"                               emits-change writable
.Function1RevisionId                          property  s         "0xa1"                                   emits-change writable
.Function1SubsystemId                         property  s         "0x1098"                                 emits-change writable
.Function1SubsystemVendorId                   property  s         "0x10de"                                 emits-change writable
.Function1VendorId                            property  s         "0x10de"                                 emits-change writable
.Function2ClassCode                           property  s         ""                                       emits-change writable
.Function2DeviceClass                         property  s         ""                                       emits-change writable
.Function2DeviceId                            property  s         ""                                       emits-change writable
.Function2FunctionType                        property  s         ""                                       emits-change writable
.Function2RevisionId                          property  s         ""                                       emits-change writable
.Function2SubsystemId                         property  s         ""                                       emits-change writable
.Function2SubsystemVendorId                   property  s         ""                                       emits-change writable
.Function2VendorId                            property  s         ""                                       emits-change writable
.Function3ClassCode                           property  s         ""                                       emits-change writable
.Function3DeviceClass                         property  s         ""                                       emits-change writable
.Function3DeviceId                            property  s         ""                                       emits-change writable
.Function3FunctionType                        property  s         ""                                       emits-change writable
.Function3RevisionId                          property  s         ""                                       emits-change writable
.Function3SubsystemId                         property  s         ""                                       emits-change writable
.Function3SubsystemVendorId                   property  s         ""                                       emits-change writable
.Function3VendorId                            property  s         ""                                       emits-change writable
.Function4ClassCode                           property  s         ""                                       emits-change writable
.Function4DeviceClass                         property  s         ""                                       emits-change writable
.Function4DeviceId                            property  s         ""                                       emits-change writable
.Function4FunctionType                        property  s         ""                                       emits-change writable
.Function4RevisionId                          property  s         ""                                       emits-change writable
.Function4SubsystemId                         property  s         ""                                       emits-change writable
.Function4SubsystemVendorId                   property  s         ""                                       emits-change writable
.Function4VendorId                            property  s         ""                                       emits-change writable
.Function5ClassCode                           property  s         ""                                       emits-change writable
.Function5DeviceClass                         property  s         ""                                       emits-change writable
.Function5DeviceId                            property  s         ""                                       emits-change writable
.Function5FunctionType                        property  s         ""                                       emits-change writable
.Function5RevisionId                          property  s         ""                                       emits-change writable
.Function5SubsystemId                         property  s         ""                                       emits-change writable
.Function5SubsystemVendorId                   property  s         ""                                       emits-change writable
.Function5VendorId                            property  s         ""                                       emits-change writable
.Function6ClassCode                           property  s         ""                                       emits-change writable
.Function6DeviceClass                         property  s         ""                                       emits-change writable
.Function6DeviceId                            property  s         ""                                       emits-change writable
.Function6FunctionType                        property  s         ""                                       emits-change writable
.Function6RevisionId                          property  s         ""                                       emits-change writable
.Function6SubsystemId                         property  s         ""                                       emits-change writable
.Function6SubsystemVendorId                   property  s         ""                                       emits-change writable
.Function6VendorId                            property  s         ""                                       emits-change writable
.Function7ClassCode                           property  s         ""                                       emits-change writable
.Function7DeviceClass                         property  s         ""                                       emits-change writable
.Function7DeviceId                            property  s         ""                                       emits-change writable
.Function7FunctionType                        property  s         ""                                       emits-change writable
.Function7RevisionId                          property  s         ""                                       emits-change writable
.Function7SubsystemId                         property  s         ""                                       emits-change writable
.Function7SubsystemVendorId                   property  s         ""                                       emits-change writable
.Function7VendorId                            property  s         ""                                       emits-change writable
.GenerationInUse                              property  s         "xyz.openbmc_project.Inventory.Item.P... emits-change writable
.GenerationSupported                          property  s         "xyz.openbmc_project.Inventory.Item.P... emits-change writable
.LanesInUse                                   property  u         0                                        emits-change writable
.Manufacturer                                 property  s         "NVidia Corporation"                     emits-change writable
.MaxLanes                                     property  u         0                                        emits-change writable
```

## Redfish

Here is an example how the devices are represented in the Redfish.

`https://<IP>/redfish/v1/Chassis/<chassis>`
```
{
  ...
  "PCIeDevices": {
    "@odata.id": "/redfish/v1/Systems/system/PCIeDevices"
  },
  ...
}
```

`https://<IP>/redfish/v1/Systems/system/PCIeDevices`
```
{
  "@odata.id": "/redfish/v1/Systems/system/PCIeDevices",
  "@odata.type": "#PCIeDeviceCollection.PCIeDeviceCollection",
  "Description": "Collection of PCIe Devices",
  "Members": [
    {
      "@odata.id": "/redfish/v1/Systems/system/PCIeDevices/Bus_01_Device_00"
    },
    {
      "@odata.id": "/redfish/v1/Systems/system/PCIeDevices/Bus_21_Device_00"
    }
  ],
  "Members@odata.count": 2,
  "Name": "PCIe Device Collection"
}
```

`https://<IP>/redfish/v1/Systems/system/PCIeDevices/Bus_21_Device_00`
```
{
  "@odata.id": "/redfish/v1/Systems/system/PCIeDevices/Bus_21_Device_00",
  "@odata.type": "#PCIeDevice.v1_4_0.PCIeDevice",
  "DeviceType": "MultiFunction",
  "Id": "Bus_21_Device_00",
  "Manufacturer": "NVidia Corporation",
  "Name": "PCIe Device",
  "PCIeFunctions": {
    "@odata.id": "/redfish/v1/Systems/system/PCIeDevices/Bus_21_Device_00/PCIeFunctions"
  },
  "PCIeInterface": {
    "PCIeType": "Gen1"
  }
}
```

`https://<IP>/redfish/v1/Systems/system/PCIeDevices/Bus_21_Device_00/PCIeFunctions`
```
{
  "@odata.id": "/redfish/v1/Systems/system/PCIeDevices/Bus_21_Device_00/PCIeFunctions",
  "@odata.type": "#PCIeFunctionCollection.PCIeFunctionCollection",
  "Description": "Collection of PCIe Functions for PCIe Device Bus_21_Device_00",
  "Members": [
    {
      "@odata.id": "/redfish/v1/Systems/system/PCIeDevices/Bus_21_Device_00/PCIeFunctions/0"
    },
    {
      "@odata.id": "/redfish/v1/Systems/system/PCIeDevices/Bus_21_Device_00/PCIeFunctions/1"
    }
  ],
  "Members@odata.count": 2,
  "Name": "PCIe Function Collection"
}
```

`https://<IP>/redfish/v1/Systems/system/PCIeDevices/Bus_21_Device_00/PCIeFunctions/0`
```
{
  "@odata.id": "/redfish/v1/Systems/system/PCIeDevices/Bus_21_Device_00/PCIeFunctions/0",
  "@odata.type": "#PCIeFunction.v1_2_0.PCIeFunction",
  "ClassCode": "0x030000",
  "DeviceClass": "DisplayController",
  "DeviceId": "0x13bb",
  "FunctionId": 0,
  "FunctionType": "Physical",
  "Id": "0",
  "Links": {
    "PCIeDevice": {
      "@odata.id": "/redfish/v1/Systems/system/PCIeDevices/Bus_21_Device_00"
    }
  },
  "Name": "PCIe Function",
  "RevisionId": "0xa2",
  "SubsystemId": "0x1098",
  "SubsystemVendorId": "0x10de",
  "VendorId": "0x10de"
}
```

The representation can change due to [bmcweb](https://github.com/openbmc/bmcweb) changes.

## webui-vue

Currently [webui-vue](https://github.com/openbmc/webui-vue) doesn't display PCIe device information on the inventory page.

There is some WIP patch, but currently it is far from ideal:

[60526: Add PCIe devices to 'Inventory and LEDs' section](https://gerrit.openbmc.org/c/openbmc/webui-vue/+/60526).
