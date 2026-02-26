/**
 * Evidence Collection Form – Device Type Data Model
 *
 * Mirrors the Rust `evidence_form` module.  These types are used by the
 * SolidJS component and can also be hydrated at runtime via the Tauri
 * command `get_device_type_configs`.
 */

// ---------------------------------------------------------------------------
// Shared types
// ---------------------------------------------------------------------------

export interface FormOption {
  /** Machine-readable identifier used as the <option value="…"> */
  value: string;
  /** Human-readable label shown in the dropdown */
  label: string;
  /** When true a free-text "specify" input is rendered below the dropdown */
  is_other: boolean;
}

export interface DeviceTypeConfig {
  value: string;
  label: string;
  /** Common physical / logical acquisition interfaces for this device type */
  interfaces: FormOption[];
  /** Common acquisition protocols for this device type */
  protocols: FormOption[];
}

// ---------------------------------------------------------------------------
// Static data (matches evidence_form.rs)
// ---------------------------------------------------------------------------

const other: FormOption = {
  value: "other",
  label: "Other (specify below)",
  is_other: true,
};

export const DEVICE_TYPE_CONFIGS: DeviceTypeConfig[] = [
  {
    value: "mobile",
    label: "Mobile Device (Phone / Tablet)",
    interfaces: [
      { value: "usb", label: "USB", is_other: false },
      { value: "wifi", label: "Wi-Fi", is_other: false },
      { value: "bluetooth", label: "Bluetooth", is_other: false },
      { value: "sim", label: "SIM Card Slot", is_other: false },
      { value: "microsd", label: "MicroSD / Memory Card", is_other: false },
      other,
    ],
    protocols: [
      { value: "adb", label: "ADB (Android Debug Bridge)", is_other: false },
      { value: "afc", label: "Apple File Conduit (AFC / iTunes)", is_other: false },
      { value: "jtag", label: "JTAG / ISP", is_other: false },
      { value: "chip_off", label: "Chip-Off", is_other: false },
      { value: "ffs", label: "Full File System (FFS)", is_other: false },
      { value: "logical", label: "Logical Acquisition", is_other: false },
      other,
    ],
  },
  {
    value: "computer",
    label: "Computer / Laptop",
    interfaces: [
      { value: "usb", label: "USB", is_other: false },
      { value: "thunderbolt", label: "Thunderbolt", is_other: false },
      { value: "sata", label: "SATA", is_other: false },
      { value: "nvme", label: "NVMe / M.2", is_other: false },
      { value: "pcie", label: "PCIe", is_other: false },
      { value: "ethernet", label: "Ethernet", is_other: false },
      { value: "wifi", label: "Wi-Fi", is_other: false },
      other,
    ],
    protocols: [
      { value: "write_blocker", label: "Hardware Write-Blocker", is_other: false },
      { value: "disk_image", label: "Disk Imaging (DD / E01 / AFF4)", is_other: false },
      { value: "network_share", label: "Network Share (SMB / NFS)", is_other: false },
      { value: "live_acquisition", label: "Live / Triage Acquisition", is_other: false },
      { value: "cold_boot", label: "Cold Boot Attack", is_other: false },
      other,
    ],
  },
  {
    value: "external_storage",
    label: "External Storage (HDD / SSD / USB Drive)",
    interfaces: [
      { value: "usb", label: "USB", is_other: false },
      { value: "sata", label: "SATA", is_other: false },
      { value: "nvme", label: "NVMe / M.2", is_other: false },
      { value: "esata", label: "eSATA", is_other: false },
      { value: "firewire", label: "FireWire / IEEE 1394", is_other: false },
      other,
    ],
    protocols: [
      { value: "write_blocker", label: "Hardware Write-Blocker", is_other: false },
      { value: "disk_image", label: "Disk Imaging (DD / E01 / AFF4)", is_other: false },
      { value: "hash_verify", label: "Hash Verification (MD5 / SHA-256)", is_other: false },
      { value: "logical", label: "Logical / File-Level Copy", is_other: false },
      other,
    ],
  },
  {
    value: "network_device",
    label: "Network Device (Router / Switch / Firewall)",
    interfaces: [
      { value: "ethernet", label: "Ethernet (RJ-45)", is_other: false },
      { value: "console_serial", label: "Console / Serial Port", is_other: false },
      { value: "usb", label: "USB", is_other: false },
      { value: "sfp", label: "SFP / Fiber", is_other: false },
      { value: "wifi", label: "Wi-Fi", is_other: false },
      other,
    ],
    protocols: [
      { value: "ssh", label: "SSH", is_other: false },
      { value: "telnet", label: "Telnet", is_other: false },
      { value: "snmp", label: "SNMP", is_other: false },
      { value: "netflow", label: "NetFlow / sFlow", is_other: false },
      { value: "serial_dump", label: "Serial Console Dump", is_other: false },
      { value: "pcap", label: "Packet Capture (PCAP)", is_other: false },
      other,
    ],
  },
  {
    value: "iot_embedded",
    label: "IoT / Embedded Device",
    interfaces: [
      { value: "usb", label: "USB", is_other: false },
      { value: "uart", label: "Serial / UART", is_other: false },
      { value: "jtag", label: "JTAG", is_other: false },
      { value: "spi", label: "SPI", is_other: false },
      { value: "i2c", label: "I²C", is_other: false },
      { value: "sd_card", label: "SD / eMMC", is_other: false },
      other,
    ],
    protocols: [
      { value: "jtag", label: "JTAG Debug", is_other: false },
      { value: "uart_dump", label: "UART Console Dump", is_other: false },
      { value: "spi_flash", label: "SPI Flash Dump", is_other: false },
      { value: "chip_off", label: "Chip-Off / eMMC Reader", is_other: false },
      { value: "openocd", label: "OpenOCD", is_other: false },
      other,
    ],
  },
  {
    value: "vehicle",
    label: "Vehicle / Telematics (Car / Truck / Motorcycle)",
    interfaces: [
      { value: "obd2", label: "OBD-II Port", is_other: false },
      { value: "usb", label: "USB", is_other: false },
      { value: "bluetooth", label: "Bluetooth", is_other: false },
      { value: "can_bus", label: "CAN Bus", is_other: false },
      { value: "sd_card", label: "SD / Memory Card", is_other: false },
      other,
    ],
    protocols: [
      { value: "obd2_protocol", label: "OBD-II (ISO 9141 / CAN)", is_other: false },
      { value: "can_bus_dump", label: "CAN Bus Dump", is_other: false },
      { value: "infotainment_logical", label: "Infotainment Logical Copy", is_other: false },
      { value: "manufacturer_tool", label: "Manufacturer / OEM Tool", is_other: false },
      other,
    ],
  },
  {
    value: "cloud_vm",
    label: "Cloud / Virtual Machine",
    interfaces: [
      { value: "api", label: "Cloud API", is_other: false },
      { value: "network", label: "Network / VPN", is_other: false },
      { value: "ssh", label: "SSH", is_other: false },
      other,
    ],
    protocols: [
      { value: "rest_api", label: "REST API (AWS / Azure / GCP)", is_other: false },
      { value: "ssh", label: "SSH Remote Acquisition", is_other: false },
      { value: "cloud_cli", label: "Cloud CLI (aws / az / gcloud)", is_other: false },
      { value: "vm_snapshot", label: "VM Snapshot / Disk Export", is_other: false },
      { value: "iscsi", label: "iSCSI Volume Mount", is_other: false },
      other,
    ],
  },
  {
    value: "drone_uav",
    label: "Drone / UAV",
    interfaces: [
      { value: "usb", label: "USB", is_other: false },
      { value: "wifi", label: "Wi-Fi", is_other: false },
      { value: "bluetooth", label: "Bluetooth", is_other: false },
      { value: "microsd", label: "MicroSD / Internal Flash", is_other: false },
      { value: "uart", label: "UART / Serial", is_other: false },
      other,
    ],
    protocols: [
      { value: "adb", label: "ADB (Android-based controllers)", is_other: false },
      { value: "usb_mass_storage", label: "USB Mass Storage", is_other: false },
      { value: "proprietary", label: "Proprietary (DJI / Parrot / etc.)", is_other: false },
      { value: "chip_off", label: "Chip-Off", is_other: false },
      other,
    ],
  },
  {
    value: "other_device",
    label: "Other Device Type",
    interfaces: [other],
    protocols: [other],
  },
];

/** Look up a device type config by its value string. */
export function getDeviceTypeConfig(value: string): DeviceTypeConfig | undefined {
  return DEVICE_TYPE_CONFIGS.find((c) => c.value === value);
}
