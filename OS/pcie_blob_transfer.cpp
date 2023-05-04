#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <ipmiblob/blob_handler.hpp>
#include <ipmiblob/blob_interface.hpp>
#include <ipmiblob/ipmi_handler.hpp>

extern "C" {
#include <pci/pci.h>
}

constexpr int configSpaceSize = 256;

typedef struct {
  uint32_t header;
  uint8_t bus;
  uint8_t device;
  uint8_t function;
  uint8_t reserved;
} PciHeaderMDRV;


bool InternalPCIDevice(uint16_t VendorId, uint16_t DeviceId)
{
  if (VendorId == 0x1A03) {
    if ((DeviceId == 0x1150) || (DeviceId == 0x2000))
      return true;
  }

  if (VendorId != 0x1022)
    return false;

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
      return true;
  }

  return false;
}

std::uint32_t writePCIeHeaderToBlob(ipmiblob::BlobHandler& blob,
                                    std::uint16_t session,
                                    std::uint32_t blob_offset,
                                    std::uint8_t bus,
                                    std::uint8_t dev,
                                    std::uint8_t func)
{
  PciHeaderMDRV PciHeader;
  PciHeader.header = 0x45494350; // "PCIE"
  PciHeader.bus = bus;
  PciHeader.device = dev;
  PciHeader.function = func;
  auto ptr = reinterpret_cast<uint8_t*>(&PciHeader);
  std::vector<uint8_t> buffer(ptr, ptr + sizeof(PciHeader));
  blob.writeBytes(session, blob_offset, buffer);
  return blob_offset + sizeof(PciHeader);
}

std::uint32_t writePCIeConfigToBlob(ipmiblob::BlobHandler& blob,
                                    std::uint16_t session,
                                    std::uint32_t blob_offset,
                                    struct pci_dev *dev
                                    )
{
  uint8_t config[configSpaceSize];
  for (int i=0; i<configSpaceSize; i++) {
    config[i] = pci_read_byte(dev, i);
  }
  std::vector<uint8_t> bytes(config, config + configSpaceSize);

  std::uint32_t buffer_offset = 0;
  while (buffer_offset < bytes.size()) {
    std::uint32_t size = ((bytes.size() - buffer_offset) > 128) ? 128 : (bytes.size() - buffer_offset);
    auto first = bytes.cbegin() + buffer_offset;
    auto last = bytes.cbegin() + buffer_offset + size;
    std::vector<std::uint8_t> buf(first, last);
    blob.writeBytes(session, blob_offset + buffer_offset, buf);
    buffer_offset += size;
  }
  return blob_offset + configSpaceSize;
}

int main()
{
  if (getuid()) {
    std::cout << "Error! Please run the program with root priviledges" << std::endl;
    return EXIT_FAILURE;
  }
  auto ipmi = ipmiblob::IpmiHandler::CreateIpmiHandler();
  ipmiblob::BlobHandler blob(std::move(ipmi));
  int count = blob.getBlobCount();
  for (int i = 0; i < count; i++) {
    std::string blobId = blob.enumerateBlob(i);
    std::cout << "BLOB " << static_cast<int>(i) << ": " << blobId << std::endl;
    if (blobId == "/pcie") {
      std::uint16_t session = blob.openBlob(blobId, ipmiblob::StateFlags::open_write);
      std::cout << "SessionID = " << static_cast<int>(session) << std::endl;
      std::uint32_t blob_offset = 0;
      struct pci_access *pacc;
      struct pci_dev *dev;
      pacc = pci_alloc();
      pci_init(pacc);
      pci_scan_bus(pacc);
      for (dev=pacc->devices; dev; dev=dev->next) {
        if (InternalPCIDevice(pci_read_word(dev, PCI_VENDOR_ID), pci_read_word(dev, PCI_DEVICE_ID)))
          continue;
        std::cout << "Send " << std::hex << std::setfill('0') << std::setw(2) << int(dev->bus) << ":"
                  << int(dev->dev) << "." << std::setw(1) << int(dev->func) << std::endl;
        blob_offset = writePCIeHeaderToBlob(blob, session, blob_offset, dev->bus, dev->dev, dev->func);
        blob_offset = writePCIeConfigToBlob(blob, session, blob_offset, dev);
      }
      pci_cleanup(pacc);
      std::cout << "Commit" << std::endl;
      blob.commit(session);
      blob.closeBlob(session);
    }
  }
  return EXIT_SUCCESS;
}
