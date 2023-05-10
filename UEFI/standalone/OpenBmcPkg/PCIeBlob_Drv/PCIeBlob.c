#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Protocol/PciRootBridgeIo.h>
#include <IndustryStandard/Pci.h>
#include <KcsBmc.h>
#include <IpmiTransportProtocol.h>

//#define DEBUG_IPMI_MESSAGES

#define MAX_TEMP_DATA     160

//
// Structure of IPMI Command buffer
//
#define EFI_IPMI_COMMAND_HEADER_SIZE  2

typedef struct {
  UINT8 Lun : 2;
  UINT8 NetFunction : 6;
  UINT8 Command;
  UINT8 CommandData[MAX_TEMP_DATA - EFI_IPMI_COMMAND_HEADER_SIZE];
} EFI_IPMI_COMMAND;

//
// Structure of IPMI Command response buffer
//
#define EFI_IPMI_RESPONSE_HEADER_SIZE 2

typedef struct {
  UINT8 Lun : 2;
  UINT8 NetFunction : 6;
  UINT8 Command;
  UINT8 ResponseData[MAX_TEMP_DATA - EFI_IPMI_RESPONSE_HEADER_SIZE];
} EFI_IPMI_RESPONSE;



#define IPMI_GOOGLE_NETFN              46
#define IPMI_GOOGLE_BLOB_CMD           128

#define BMC_BLOB_CMD_CODE_GET_COUNT    0
#define BMC_BLOB_CMD_CODE_ENUMERATE    1
#define BMC_BLOB_CMD_CODE_OPEN         2
#define BMC_BLOB_CMD_CODE_READ         3
#define BMC_BLOB_CMD_CODE_WRITE        4
#define BMC_BLOB_CMD_CODE_COMMIT       5
#define BMC_BLOB_CMD_CODE_CLOSE        6
#define BMC_BLOB_CMD_CODE_DELETE       7
#define BMC_BLOB_CMD_CODE_STAT         8
#define BMC_BLOB_CMD_CODE_SESSION_STAT 9

#define BLOB_STATE_FLAG_OPEN_READ      (1 << 0)
#define BLOB_STATE_FLAG_OPEN_WRITE     (1 << 1)
#define BLOB_STATE_FLAG_COMMITING      (1 << 2)
#define BLOB_STATE_FLAG_COMMITED       (1 << 3)
#define BLOB_STATE_FLAG_COMMIT_ERROR   (1 << 4)

#pragma pack (1)
typedef struct {
  UINT8 OEN[3];
  UINT8 BlobCmd;
} BLOB_REQUEST;

typedef struct {
  UINT8 OEN[3];
  UINT8 BlobCmd;
  UINT16 Crc;
  UINT8 Data[1];
} BLOB_REQUEST_WITH_DATA;

typedef struct {
  UINT8 Completion;
  UINT8 OEN[3];
} BLOB_RESPONSE;

typedef struct {
  UINT8 Completion;
  UINT8 OEN[3];
  UINT16 Crc;
  UINT8 Data[1];
} BLOB_RESPONSE_WITH_DATA;
#pragma pack ()

#pragma pack (1)
typedef struct {
  UINT32 header;
  UINT8 bus;
  UINT8 device;
  UINT8 function;
  UINT8 reserved;
} PciHeaderMDRV;

typedef struct {
  PciHeaderMDRV mdrvHeader;
  UINT8 data[256];
} PciMDRV;
#pragma pack ()

EFI_STATUS ExecuteIpmiCmd(UINT64 KcsTimeout, UINT16 KcsPort, VOID* SendBuf, UINT8 SendLength, VOID* ReceiveBuf, UINT8* ReceiveLength)
{
  EFI_IPMI_COMMAND            IpmiCommand;
  EFI_IPMI_RESPONSE           IpmiResponse;

  IpmiCommand.Lun = 0;
  IpmiCommand.NetFunction = IPMI_GOOGLE_NETFN;
  IpmiCommand.Command = IPMI_GOOGLE_BLOB_CMD;

  for (UINT8 i = 0; i < SendLength; i++) {
    IpmiCommand.CommandData[i] = ((UINT8*)SendBuf)[i];
  }

  EFI_STATUS Status = SendDataToBmcPort (
                          KcsTimeout,
                          KcsPort,
                          NULL,
                          (UINT8 *) &IpmiCommand,
                          (SendLength + EFI_IPMI_COMMAND_HEADER_SIZE)
                      );
  if (EFI_ERROR(Status)) {
    DEBUG((EFI_D_ERROR, "SendDataToBmcPort Error: %r\n", Status));
    return Status;
  }

  UINT8 DataSize;
  DataSize = MAX_TEMP_DATA;
  Status = ReceiveBmcDataFromPort (
               KcsTimeout,
               KcsPort,
               NULL,
               (UINT8 *)&IpmiResponse,
               &DataSize
           );

  if (EFI_ERROR(Status)) {
    DEBUG((EFI_D_ERROR, "ReceiveBmcDataFromPort Error: %r\n", Status));
    return Status;
  }

  *ReceiveLength = DataSize - 2;
  for (UINT8 i = 0; i < (*ReceiveLength); i++) {
    ((UINT8*)ReceiveBuf)[i] = IpmiResponse.ResponseData[i];
  }

  return Status;
}

UINT8 OpenBMC_OEN[3] = {0xcf, 0xc2, 0x00};

VOID PrintBuffer(UINT8* Buffer, UINTN Size)
{
  UINTN i = 0;
  while (i < Size) {
    DEBUG((EFI_D_VERBOSE, "%02x ", Buffer[i]));
    i++;
    if (!(i%16)) {
      DEBUG((EFI_D_VERBOSE, " | "));
      for (UINTN j=16; j>0; j--)
        if ((Buffer[i-j] >= 0x20) && (Buffer[i-j] < 0x7E))
          DEBUG((EFI_D_VERBOSE, "%c", Buffer[i-j]));
        else
          DEBUG((EFI_D_VERBOSE, "."));
      DEBUG((EFI_D_VERBOSE, "\n"));
    }
  }

  if (i%16) {
    for (UINTN j=0; j<=15; j++) {
      if ((i+j)%16)
        DEBUG((EFI_D_VERBOSE, "   "));
      else
        break;
    }
    DEBUG((EFI_D_VERBOSE, " | "));

    for (UINTN j=(i%16); j>0; j--) {
      if ((Buffer[i-j] >= 0x20) && (Buffer[i-j] < 0x7E))
        DEBUG((EFI_D_VERBOSE, "%c", Buffer[i-j]));
      else
        DEBUG((EFI_D_VERBOSE, "."));
    }
    DEBUG((EFI_D_VERBOSE, "\n"));
  }
}

UINT16 generateCrc(UINT8* data, UINTN size)
{
  UINT16 kPoly = 0x1021;
  UINT16 kLeftBit = 0x8000;
  UINT16 crc = 0xFFFF;
  INTN kExtraRounds = 2;
  for (UINTN i = 0; i < size + kExtraRounds; ++i) {
    for (UINTN j = 0; j < 8; ++j) {
      BOOLEAN xor_flag = (crc & kLeftBit) ? 1 : 0;
      crc <<= 1;
      if (i < size && (data[i] & (1 << (7 - j))))
      {
        crc++;
      }
      if (xor_flag)
      {
        crc ^= kPoly;
      }
    }
  }
  return crc;
}

EFI_STATUS sendBlobCommand(UINT8 BlobCmd, UINT8* Request, UINT8 RequestLength, UINT8* Response, UINT8* ResponseLength)
{
  UINT8 SendLength;
  if (RequestLength) {
    SendLength = sizeof(BLOB_REQUEST_WITH_DATA) - 1 + RequestLength;
  } else {
    SendLength = sizeof(BLOB_REQUEST);
  }
  UINT8* SendBuf = (UINT8*)AllocateZeroPool(SendLength);
  ((BLOB_REQUEST*)SendBuf)->OEN[0] = OpenBMC_OEN[0];
  ((BLOB_REQUEST*)SendBuf)->OEN[1] = OpenBMC_OEN[1];
  ((BLOB_REQUEST*)SendBuf)->OEN[2] = OpenBMC_OEN[2];
  ((BLOB_REQUEST*)SendBuf)->BlobCmd = BlobCmd;
  if (RequestLength) {
    ((BLOB_REQUEST_WITH_DATA*)SendBuf)->Crc = generateCrc(Request, RequestLength);
    CopyMem(&(((BLOB_REQUEST_WITH_DATA*)SendBuf)->Data), Request, RequestLength);
  }

#ifdef DEBUG_IPMI_MESSAGES
  DEBUG((EFI_D_VERBOSE, "SEND BUF:\n"));
  PrintBuffer(SendBuf, SendLength);
#endif

  UINT8 ReceiveLength;
  if (*ResponseLength) {
    ReceiveLength = sizeof(BLOB_RESPONSE_WITH_DATA) - 1 + (*ResponseLength);
  } else {
    ReceiveLength = sizeof(BLOB_RESPONSE);
  }
  UINT8* ReceiveBuf = (UINT8*)AllocateZeroPool(ReceiveLength);

  EFI_STATUS Status = ExecuteIpmiCmd(
                          500000,
                          0xCA2,
                          (VOID*)SendBuf,
                          SendLength,
                          (VOID*)ReceiveBuf,
                          &ReceiveLength
                      );
  FreePool(SendBuf);
  if (EFI_ERROR(Status)) {
    DEBUG((EFI_D_ERROR, "Error! ExecuteIpmiCmd returned %r\n", Status));
    FreePool(ReceiveBuf);
    return Status;
  }
#ifdef DEBUG_IPMI_MESSAGES
  DEBUG((EFI_D_VERBOSE, "RECEIVE BUF:\n"));
  PrintBuffer(ReceiveBuf, ReceiveLength);
#endif

  if (  (ReceiveLength < sizeof(BLOB_RESPONSE)) ||
       ((ReceiveLength > sizeof(BLOB_RESPONSE)) && (ReceiveLength < sizeof(BLOB_RESPONSE_WITH_DATA)))  ) {
    DEBUG((EFI_D_ERROR, "Error! Response error: Wrong response size %d\n", ReceiveLength));
    FreePool(ReceiveBuf);
    return EFI_DEVICE_ERROR;
  }
  if (((BLOB_RESPONSE*)ReceiveBuf)->Completion != 0x00) {
    DEBUG((EFI_D_ERROR, "Error! Response error: Completion byte error\n", ReceiveBuf[0]));
    FreePool(ReceiveBuf);
    return EFI_DEVICE_ERROR;
  }
  if ((((BLOB_RESPONSE*)ReceiveBuf)->OEN[0] != OpenBMC_OEN[0]) ||
      (((BLOB_RESPONSE*)ReceiveBuf)->OEN[1] != OpenBMC_OEN[1]) ||
      (((BLOB_RESPONSE*)ReceiveBuf)->OEN[2] != OpenBMC_OEN[2])) {
    DEBUG((EFI_D_ERROR, "Error! Response error: OEN mismatch\n"));
    FreePool(ReceiveBuf);
    return EFI_DEVICE_ERROR;
  }
  if (ReceiveLength >= sizeof(BLOB_RESPONSE_WITH_DATA)) {
    *ResponseLength = ReceiveLength - (sizeof(BLOB_RESPONSE_WITH_DATA) - 1);
    UINT16 Crc = generateCrc(((BLOB_RESPONSE_WITH_DATA*)ReceiveBuf)->Data, *ResponseLength);
    if (Crc != ((BLOB_RESPONSE_WITH_DATA*)ReceiveBuf)->Crc) {
      DEBUG((EFI_D_ERROR, "Error! Response error: CRC mismatch\n"));
      FreePool(ReceiveBuf);
      return EFI_DEVICE_ERROR;
    }
    CopyMem(Response, &(((BLOB_RESPONSE_WITH_DATA*)ReceiveBuf)->Data), *ResponseLength);
  } else {
    *ResponseLength = 0;
  }
  FreePool(ReceiveBuf);

#ifdef DEBUG_IPMI_MESSAGES
  DEBUG((EFI_D_VERBOSE, "Response:\n"));
  PrintBuffer(Response, *ResponseLength);
#endif
  return Status;
}

EFI_STATUS BlobGetCount(UINT32* BlobCount)
{
  UINT8 ResponseLength = sizeof(*BlobCount);
  EFI_STATUS Status = sendBlobCommand(BMC_BLOB_CMD_CODE_GET_COUNT,
                                      NULL,
                                      0,
                                      (UINT8*)BlobCount,
                                      &ResponseLength);
  if (ResponseLength != sizeof(*BlobCount))
    return EFI_DEVICE_ERROR;
  return Status;
}

EFI_STATUS BlobEnumerate(UINT32 BlobIndex, CHAR8* BlobId)
{
  UINT8 ResponseLength = 127;
  EFI_STATUS Status = sendBlobCommand(BMC_BLOB_CMD_CODE_ENUMERATE,
                                     (UINT8*)&BlobIndex,
                                     (UINT8)sizeof(BlobIndex),
                                     (UINT8*)BlobId,
                                     &ResponseLength);
  return Status;
}

EFI_STATUS BlobOpen(UINT16 flags, CHAR8* BlobId, UINT16* Session)
{
  UINT8 ResponseLength = (UINT8)sizeof(*Session);
  UINT8 RequestLength = (UINT8)sizeof(flags) + (UINT8)AsciiStrLen(BlobId) + 1;
  UINT8* Request = (UINT8*)AllocateZeroPool(RequestLength);
  Request[0] = flags & 0xFF;
  Request[1] = (flags >> 8) & 0xFF;
  CopyMem(&Request[2], BlobId, AsciiStrLen((CHAR8*)BlobId));
  EFI_STATUS Status = sendBlobCommand(BMC_BLOB_CMD_CODE_OPEN,
                                     Request,
                                     RequestLength,
                                     (UINT8*)Session,
                                     &ResponseLength);
  FreePool(Request);
  if (ResponseLength != ((UINT8)sizeof(*Session)))
    return EFI_DEVICE_ERROR;
  return Status;
}

EFI_STATUS BlobClose(UINT16 Session)
{
  UINT8 Response;
  UINT8 ResponseLength = 0;
  EFI_STATUS Status = sendBlobCommand(BMC_BLOB_CMD_CODE_CLOSE,
                                     (UINT8*)&Session,
                                     (UINT8)sizeof(Session),
                                     &Response,
                                     &ResponseLength);
  if (ResponseLength != 0)
    return EFI_DEVICE_ERROR;
  return Status;
}

EFI_STATUS BlobWrite(UINT16 Session, UINT32 Offset, UINT8* Data, UINT8 DataSize)
{
  UINT8 Response;
  UINT8 ResponseLength = 0;
  UINT8 RequestLength = (UINT8)sizeof(Session) + (UINT8)sizeof(Offset) + DataSize;
  UINT8* Request = (UINT8*)AllocateZeroPool(RequestLength);
  Request[0] = Session & 0xFF;
  Request[1] = (Session >> 8) & 0xFF;
  Request[2] = Offset & 0xFF;
  Request[3] = (Offset >> 8) & 0xFF;
  Request[4] = (Offset >> 16) & 0xFF;
  Request[5] = (Offset >> 24) & 0xFF;
  CopyMem(&Request[6], Data, DataSize);
  EFI_STATUS Status = sendBlobCommand(BMC_BLOB_CMD_CODE_WRITE,
                                     Request,
                                     RequestLength,
                                     &Response,
                                     &ResponseLength);
  FreePool(Request);
  if (ResponseLength != 0)
    return EFI_DEVICE_ERROR;
  return Status;
}

EFI_STATUS BlobLongWrite(UINT16 Session, UINT32 Offset, UINT8* Data, UINT32 DataSize)
{
  EFI_STATUS Status = EFI_SUCCESS;
  UINT32 Count = 0;
  while (Count < DataSize) {
    UINT32 Size = ((DataSize - Count) > 128) ? 128 : (DataSize - Count);
    Status = BlobWrite(Session, Offset + Count, &Data[Count], (UINT8)Size);
    if (EFI_ERROR(Status)) {
      DEBUG((EFI_D_ERROR, "Error! BlobWrite failed: %r\n", Status));
      return Status;
    }
    Count += Size;
  }
  return Status;
}

EFI_STATUS BlobCommit(UINT16 Session)
{
  UINT8 Response;
  UINT8 ResponseLength = 0;
  UINT8 RequestLength = (UINT8)sizeof(Session) + 1;
  UINT8* Request = (UINT8*)AllocateZeroPool(RequestLength);
  Request[0] = Session & 0xFF;
  Request[1] = (Session >> 8) & 0xFF;
  Request[2] = 0;
  EFI_STATUS Status = sendBlobCommand(BMC_BLOB_CMD_CODE_COMMIT,
                                     Request,
                                     RequestLength,
                                     &Response,
                                     &ResponseLength);
  FreePool(Request);
  if (ResponseLength != 0)
    return EFI_DEVICE_ERROR;
  return Status;
}

BOOLEAN InternalPCIDevice(UINT16 VendorId, UINT16 DeviceId)
{
  if (VendorId == 0x1A03) {
    if ((DeviceId == 0x1150) || (DeviceId == 0x2000))
      return TRUE;
  }

  if (VendorId != 0x1022)
    return FALSE;

  switch (DeviceId) {
    // NAPLES
    case 0x1450:
    case 0x1452:
    case 0x1453:
    case 0x1454:
    case 0x1455:
    case 0x1456:
    case 0x145A:
    case 0x145F:
    case 0x1460:
    case 0x1461:
    case 0x1462:
    case 0x1463:
    case 0x1464:
    case 0x1465:
    case 0x1466:
    case 0x1467:
    case 0x1468:
    case 0x7901:
    // ROME
    case 0x1480:
    case 0x1481:
    case 0x1482:
    case 0x1483:
    case 0x1484:
    case 0x1485:
    case 0x1486:
    case 0x1487:
    case 0x148A:
    case 0x148C:
    case 0x1490:
    case 0x1491:
    case 0x1492:
    case 0x1493:
    case 0x1494:
    case 0x1495:
    case 0x1496:
    case 0x1497:
    case 0x1498:
    case 0x149A:
    case 0x7904:
    case 0x790B:
    case 0x790E:
    // MILAN
    case 0x164F:
    case 0x1650:
    case 0x1651:
    case 0x1652:
    case 0x1653:
    case 0x1654:
    case 0x1655:
    case 0x1656:
    case 0x1657:
      return TRUE;
  }

  return FALSE;
}

UINT64 PciConfigurationAddress(UINT8 Bus,
                               UINT8 Device,
                               UINT8 Function,
                               UINT32 Register)
{
  UINT64 Address = (((UINT64)Bus) << 24) + (((UINT64)Device) << 16) + (((UINT64)Function) << 8);
  if (Register & 0xFFFFFF00) {
    Address += (((UINT64)Register) << 32);
  } else {
    Address += (((UINT64)Register) << 0);
  }
  return Address;
}

EFI_STATUS BlobSendPCIeRootBridgeDevices(EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL* PciRootBridgeIo, UINT16 Session, UINT32* Offset)
{
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR* AddressDescriptor;
  EFI_STATUS Status = PciRootBridgeIo->Configuration(
                                         PciRootBridgeIo,
                                         (VOID**)&AddressDescriptor
                                       );
  if (EFI_ERROR(Status)) {
    DEBUG((EFI_D_ERROR, "Error! PciRootBridgeIo->Configuration returned: %r\n", Status));
    return Status;
  }

  while (AddressDescriptor->Desc != ACPI_END_TAG_DESCRIPTOR) {
    if (AddressDescriptor->ResType == ACPI_ADDRESS_SPACE_TYPE_BUS) {
      for (UINT8 Bus = (UINT8)(AddressDescriptor->AddrRangeMin); Bus <= (UINT8)(AddressDescriptor->AddrRangeMax); Bus++) {
        for (UINT8 Device = 0; Device <= PCI_MAX_DEVICE; Device++) {
          for (UINT8 Func = 0; Func <= PCI_MAX_FUNC; Func++) {
            UINT64 Address = PciConfigurationAddress(Bus, Device, Func, 0);
            PciMDRV pciDevMDRV;
            Status = PciRootBridgeIo->Pci.Read(
              PciRootBridgeIo,
              EfiPciWidthUint8,
              Address,
              256,
              pciDevMDRV.data
            );
            if (EFI_ERROR(Status)) {
              DEBUG((EFI_D_ERROR, "Error! Failed to use PciRootBridgeIo->Pci.Read: %r\n", Status));
              return Status;
            }
            UINT16 VendorId = pciDevMDRV.data[0] | (pciDevMDRV.data[1] << 8);
            UINT16 DeviceId = pciDevMDRV.data[2] | (pciDevMDRV.data[3] << 8);
            if ((VendorId == 0xffff) && (DeviceId == 0xffff)) {
              continue;
            }
            if (InternalPCIDevice(VendorId, DeviceId)) {
              continue;
            }
            pciDevMDRV.mdrvHeader.header = 0x45494350; // "PCIE"
            pciDevMDRV.mdrvHeader.bus = Bus;
            pciDevMDRV.mdrvHeader.device = Device;
            pciDevMDRV.mdrvHeader.function = Func;
            DEBUG((EFI_D_INFO, "Send %02x:%02x.%x, offset=%d\n", Bus, Device, Func, *Offset));
            Status = BlobLongWrite(Session, *Offset, (UINT8*)&pciDevMDRV, sizeof(PciMDRV));
            if (EFI_ERROR(Status)) {
              DEBUG((EFI_D_ERROR, "Error! BlobLongWrite failed: %r\n", Status));
              return Status;
            }
            *Offset += sizeof(PciMDRV);
          }
        }
      }
    }
    AddressDescriptor++;
  }
  return Status;
}

EFI_STATUS BlobSendPCIeDevices(EFI_HANDLE ImageHandle, UINT16 Session)
{
  EFI_STATUS             Status;
  UINTN                  HandleCount;
  EFI_HANDLE             *HandleBuffer;
  Status = gBS->LocateHandleBuffer(
                  ByProtocol,
                  &gEfiPciRootBridgeIoProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                );
  if (EFI_ERROR(Status)) {
    DEBUG((EFI_D_ERROR, "Error! Can't locate gEfiPciRootBridgeIoProtocolGuid: %r\n", Status));
    return Status;
  }

  UINT32 Offset = 0;
  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL* PciRootBridgeIo;
  for (UINTN Index = 0; Index < HandleCount; Index++) {
    Status = gBS->OpenProtocol (
                    HandleBuffer[Index],
                    &gEfiPciRootBridgeIoProtocolGuid,
                    (VOID **)&PciRootBridgeIo,
                    ImageHandle,
                    HandleBuffer[Index],
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
    if (EFI_ERROR(Status)) {
      DEBUG((EFI_D_ERROR, "Error! Can't open gEfiPciRootBridgeIoProtocolGuid: %r\n", Status));
      return Status;
    }

    Status = BlobSendPCIeRootBridgeDevices(PciRootBridgeIo, Session, &Offset);
    if (EFI_ERROR(Status)) {
      DEBUG((EFI_D_ERROR, "Error! BlobSendPCIeRootBridgeDevices failed with error: %r\n", Status));
      return Status;
    }

    Status = gBS->CloseProtocol(
               HandleBuffer[Index],
               &gEfiPciRootBridgeIoProtocolGuid,
               ImageHandle,
               HandleBuffer[Index]
             );
  }
  FreePool(HandleBuffer);

  return Status;

}

EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS Status;
  UINT32 BlobCount = 0;
  Status = BlobGetCount(&BlobCount);
  if (EFI_ERROR(Status)) {
    DEBUG((EFI_D_ERROR, "Error! BlobGetCount failed: %r\n", Status));
    return Status;
  }
  DEBUG((EFI_D_INFO, "BlobCount = %d\n", BlobCount));

  for (UINT32 id = 0; id < BlobCount; id++) {
    CHAR8* BlobId = (CHAR8*)AllocateZeroPool(255);
    Status = BlobEnumerate(id, BlobId);
    if (EFI_ERROR(Status)) {
      DEBUG((EFI_D_ERROR, "Error! BlobEnumerate failed: %r\n", Status));
      FreePool(BlobId);
      return Status;
    }
    DEBUG((EFI_D_INFO, "BlobEnumerate: id=%d: %a\n", id, BlobId));
    if (AsciiStrCmp(BlobId, "/pcie")) {
      FreePool(BlobId);
      continue;
    }
    DEBUG((EFI_D_INFO, "PCIe blob is found\n"));
    UINT16 Session;
    DEBUG((EFI_D_INFO, "BlobOpen\n"));
    Status = BlobOpen(BLOB_STATE_FLAG_OPEN_WRITE, BlobId, &Session);
    FreePool(BlobId);
    if (EFI_ERROR(Status)) {
      DEBUG((EFI_D_ERROR, "Error! BlobOpen failed: %r\n", Status));
      return Status;
    }
    DEBUG((EFI_D_INFO, "Session = 0x%04x\n", Session));

    DEBUG((EFI_D_INFO, "BlobSendPCIeDevices\n"));
    Status = BlobSendPCIeDevices(ImageHandle, Session);
    if (EFI_ERROR(Status)) {
      DEBUG((EFI_D_ERROR, "Error! BlobSendPCIeDevices failed: %r\n", Status));
      BlobClose(Session);
      return Status;
    }

    DEBUG((EFI_D_INFO, "BlobCommit\n"));
    Status = BlobCommit(Session);
    if (EFI_ERROR(Status)) {
      DEBUG((EFI_D_ERROR, "Error! BlobCommit failed: %r\n", Status));
      BlobClose(Session);
      return Status;
    }
    DEBUG((EFI_D_INFO, "BlobClose\n"));
    Status = BlobClose(Session);
    if (EFI_ERROR(Status)) {
      DEBUG((EFI_D_ERROR, "Error! BlobClose failed: %r\n", Status));
      return Status;
    }
    DEBUG((EFI_D_INFO, "PCIe device information was successfully transmitted to the BMC\n"));
  }

  return EFI_SUCCESS;
}
