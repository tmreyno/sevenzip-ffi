//! Evidence Collection Form - Device Type Data Model
//!
//! Defines device types and their associated acquisition interfaces and protocols
//! for the forensic evidence collection form.
//!
//! When a device type is selected in the UI, the form dynamically shows only
//! the interfaces and protocols that are relevant to that device type.
//! An "Other" option is always included so the examiner can enter custom values.

use serde::{Deserialize, Serialize};

/// A single selectable option in a form dropdown (interface or protocol).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FormOption {
    /// Machine-readable identifier (used as the `<option value="...">`)
    pub value: String,
    /// Human-readable label shown in the dropdown
    pub label: String,
    /// Whether this is the catch-all "Other" option that triggers a free-text input
    pub is_other: bool,
}

impl FormOption {
    fn new(value: &str, label: &str) -> Self {
        FormOption {
            value: value.to_string(),
            label: label.to_string(),
            is_other: false,
        }
    }

    fn other() -> Self {
        FormOption {
            value: "other".to_string(),
            label: "Other (specify below)".to_string(),
            is_other: true,
        }
    }
}

/// Device type descriptor with its common acquisition interfaces and protocols.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DeviceTypeConfig {
    /// Machine-readable identifier
    pub value: String,
    /// Human-readable device type label
    pub label: String,
    /// Common physical/logical interfaces for this device type.
    /// The "Other" option is appended automatically.
    pub interfaces: Vec<FormOption>,
    /// Common acquisition protocols for this device type.
    /// The "Other" option is appended automatically.
    pub protocols: Vec<FormOption>,
}

/// Returns the complete list of device types with their associated
/// interface and protocol options for the evidence collection form.
///
/// The last entry in each list is always the "Other" catch-all option.
pub fn device_type_configs() -> Vec<DeviceTypeConfig> {
    vec![
        DeviceTypeConfig {
            value: "mobile".to_string(),
            label: "Mobile Device (Phone / Tablet)".to_string(),
            interfaces: vec![
                FormOption::new("usb", "USB"),
                FormOption::new("wifi", "Wi-Fi"),
                FormOption::new("bluetooth", "Bluetooth"),
                FormOption::new("sim", "SIM Card Slot"),
                FormOption::new("microsd", "MicroSD / Memory Card"),
                FormOption::other(),
            ],
            protocols: vec![
                FormOption::new("adb", "ADB (Android Debug Bridge)"),
                FormOption::new("afc", "Apple File Conduit (AFC / iTunes)"),
                FormOption::new("jtag", "JTAG / ISP"),
                FormOption::new("chip_off", "Chip-Off"),
                FormOption::new("ffs", "Full File System (FFS)"),
                FormOption::new("logical", "Logical Acquisition"),
                FormOption::other(),
            ],
        },
        DeviceTypeConfig {
            value: "computer".to_string(),
            label: "Computer / Laptop".to_string(),
            interfaces: vec![
                FormOption::new("usb", "USB"),
                FormOption::new("thunderbolt", "Thunderbolt"),
                FormOption::new("sata", "SATA"),
                FormOption::new("nvme", "NVMe / M.2"),
                FormOption::new("pcie", "PCIe"),
                FormOption::new("ethernet", "Ethernet"),
                FormOption::new("wifi", "Wi-Fi"),
                FormOption::other(),
            ],
            protocols: vec![
                FormOption::new("write_blocker", "Hardware Write-Blocker"),
                FormOption::new("disk_image", "Disk Imaging (DD / E01 / AFF4)"),
                FormOption::new("network_share", "Network Share (SMB / NFS)"),
                FormOption::new("live_acquisition", "Live / Triage Acquisition"),
                FormOption::new("cold_boot", "Cold Boot Attack"),
                FormOption::other(),
            ],
        },
        DeviceTypeConfig {
            value: "external_storage".to_string(),
            label: "External Storage (HDD / SSD / USB Drive)".to_string(),
            interfaces: vec![
                FormOption::new("usb", "USB"),
                FormOption::new("sata", "SATA"),
                FormOption::new("nvme", "NVMe / M.2"),
                FormOption::new("esata", "eSATA"),
                FormOption::new("firewire", "FireWire / IEEE 1394"),
                FormOption::other(),
            ],
            protocols: vec![
                FormOption::new("write_blocker", "Hardware Write-Blocker"),
                FormOption::new("disk_image", "Disk Imaging (DD / E01 / AFF4)"),
                FormOption::new("hash_verify", "Hash Verification (MD5 / SHA-256)"),
                FormOption::new("logical", "Logical / File-Level Copy"),
                FormOption::other(),
            ],
        },
        DeviceTypeConfig {
            value: "network_device".to_string(),
            label: "Network Device (Router / Switch / Firewall)".to_string(),
            interfaces: vec![
                FormOption::new("ethernet", "Ethernet (RJ-45)"),
                FormOption::new("console_serial", "Console / Serial Port"),
                FormOption::new("usb", "USB"),
                FormOption::new("sfp", "SFP / Fiber"),
                FormOption::new("wifi", "Wi-Fi"),
                FormOption::other(),
            ],
            protocols: vec![
                FormOption::new("ssh", "SSH"),
                FormOption::new("telnet", "Telnet"),
                FormOption::new("snmp", "SNMP"),
                FormOption::new("netflow", "NetFlow / sFlow"),
                FormOption::new("serial_dump", "Serial Console Dump"),
                FormOption::new("pcap", "Packet Capture (PCAP)"),
                FormOption::other(),
            ],
        },
        DeviceTypeConfig {
            value: "iot_embedded".to_string(),
            label: "IoT / Embedded Device".to_string(),
            interfaces: vec![
                FormOption::new("usb", "USB"),
                FormOption::new("uart", "Serial / UART"),
                FormOption::new("jtag", "JTAG"),
                FormOption::new("spi", "SPI"),
                FormOption::new("i2c", "I²C"),
                FormOption::new("sd_card", "SD / eMMC"),
                FormOption::other(),
            ],
            protocols: vec![
                FormOption::new("jtag", "JTAG Debug"),
                FormOption::new("uart_dump", "UART Console Dump"),
                FormOption::new("spi_flash", "SPI Flash Dump"),
                FormOption::new("chip_off", "Chip-Off / eMMC Reader"),
                FormOption::new("openocd", "OpenOCD"),
                FormOption::other(),
            ],
        },
        DeviceTypeConfig {
            value: "vehicle".to_string(),
            label: "Vehicle / Telematics (Car / Truck / Motorcycle)".to_string(),
            interfaces: vec![
                FormOption::new("obd2", "OBD-II Port"),
                FormOption::new("usb", "USB"),
                FormOption::new("bluetooth", "Bluetooth"),
                FormOption::new("can_bus", "CAN Bus"),
                FormOption::new("sd_card", "SD / Memory Card"),
                FormOption::other(),
            ],
            protocols: vec![
                FormOption::new("obd2_protocol", "OBD-II (ISO 9141 / CAN)"),
                FormOption::new("can_bus_dump", "CAN Bus Dump"),
                FormOption::new("infotainment_logical", "Infotainment Logical Copy"),
                FormOption::new("manufacturer_tool", "Manufacturer / OEM Tool"),
                FormOption::other(),
            ],
        },
        DeviceTypeConfig {
            value: "cloud_vm".to_string(),
            label: "Cloud / Virtual Machine".to_string(),
            interfaces: vec![
                FormOption::new("api", "Cloud API"),
                FormOption::new("network", "Network / VPN"),
                FormOption::new("ssh", "SSH"),
                FormOption::other(),
            ],
            protocols: vec![
                FormOption::new("rest_api", "REST API (AWS / Azure / GCP)"),
                FormOption::new("ssh", "SSH Remote Acquisition"),
                FormOption::new("cloud_cli", "Cloud CLI (aws / az / gcloud)"),
                FormOption::new("vm_snapshot", "VM Snapshot / Disk Export"),
                FormOption::new("iscsi", "iSCSI Volume Mount"),
                FormOption::other(),
            ],
        },
        DeviceTypeConfig {
            value: "drone_uav".to_string(),
            label: "Drone / UAV".to_string(),
            interfaces: vec![
                FormOption::new("usb", "USB"),
                FormOption::new("wifi", "Wi-Fi"),
                FormOption::new("bluetooth", "Bluetooth"),
                FormOption::new("microsd", "MicroSD / Internal Flash"),
                FormOption::new("uart", "UART / Serial"),
                FormOption::other(),
            ],
            protocols: vec![
                FormOption::new("adb", "ADB (Android-based controllers)"),
                FormOption::new("usb_mass_storage", "USB Mass Storage"),
                FormOption::new("proprietary", "Proprietary (DJI / Parrot / etc.)"),
                FormOption::new("chip_off", "Chip-Off"),
                FormOption::other(),
            ],
        },
        DeviceTypeConfig {
            value: "other_device".to_string(),
            label: "Other Device Type".to_string(),
            interfaces: vec![FormOption::other()],
            protocols: vec![FormOption::other()],
        },
    ]
}

/// Returns all device type configurations as JSON.
///
/// Register this as a Tauri command in your `main.rs` / `lib.rs`:
/// ```rust,ignore
/// tauri::Builder::default()
///     .invoke_handler(tauri::generate_handler![
///         evidence_form::get_device_type_configs,
///     ])
/// ```
///
/// Then call it from the TypeScript frontend:
/// ```ts
/// import { invoke } from '@tauri-apps/api/core';
/// const configs = await invoke<DeviceTypeConfig[]>('get_device_type_configs');
/// ```
///
/// The `#[tauri::command]` attribute is applied only when the `tauri` feature
/// is enabled so this module can be unit-tested without a full Tauri build.
#[cfg_attr(feature = "tauri", tauri::command)]
pub fn get_device_type_configs() -> Vec<DeviceTypeConfig> {
    device_type_configs()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn every_device_type_has_other_interface() {
        for config in device_type_configs() {
            let has_other = config.interfaces.iter().any(|o| o.is_other);
            assert!(
                has_other,
                "Device type '{}' is missing an 'Other' interface option",
                config.label
            );
        }
    }

    #[test]
    fn every_device_type_has_other_protocol() {
        for config in device_type_configs() {
            let has_other = config.protocols.iter().any(|o| o.is_other);
            assert!(
                has_other,
                "Device type '{}' is missing an 'Other' protocol option",
                config.label
            );
        }
    }

    #[test]
    fn all_specific_device_types_have_non_empty_lists() {
        for config in device_type_configs() {
            // "other_device" is intentionally a catch-all with only the Other option.
            if config.value == "other_device" {
                continue;
            }
            assert!(
                config.interfaces.len() > 1,
                "Device type '{}' should have at least one specific interface",
                config.label
            );
            assert!(
                config.protocols.len() > 1,
                "Device type '{}' should have at least one specific protocol",
                config.label
            );
        }
    }

    #[test]
    fn other_option_is_always_last() {
        for config in device_type_configs() {
            // "other_device" only has the Other option, skip it
            if config.value == "other_device" {
                continue;
            }
            let last_iface = config.interfaces.last().unwrap();
            assert!(
                last_iface.is_other,
                "Device type '{}': 'Other' interface option should be last",
                config.label
            );
            let last_proto = config.protocols.last().unwrap();
            assert!(
                last_proto.is_other,
                "Device type '{}': 'Other' protocol option should be last",
                config.label
            );
        }
    }

    #[test]
    fn no_duplicate_values_within_device_type() {
        for config in device_type_configs() {
            let mut iface_values: Vec<&str> = config.interfaces.iter().map(|o| o.value.as_str()).collect();
            let original_len = iface_values.len();
            iface_values.dedup();
            iface_values.sort_unstable();
            iface_values.dedup();
            assert_eq!(
                iface_values.len(),
                original_len,
                "Device type '{}' has duplicate interface values",
                config.label
            );
        }
    }
}
