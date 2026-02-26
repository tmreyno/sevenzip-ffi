# Tauri + SolidJS Integration Guide

Complete guide for integrating **sevenzip-ffi** into a Tauri application
with a SolidJS frontend, including the **Evidence Collection Form** with
dynamic device-type-aware field selection.

---

## Table of Contents

1. [Project Setup](#1-project-setup)
2. [Rust Backend – Evidence Form Module](#2-rust-backend--evidence-form-module)
3. [Frontend – TypeScript Data Model](#3-frontend--typescript-data-model)
4. [Frontend – EvidenceCollectionForm Component](#4-frontend--evidencecollectionform-component)
5. [Tauri Command Wiring](#5-tauri-command-wiring)
6. [How Dynamic Fields Work](#6-how-dynamic-fields-work)
7. [Customising Device Types](#7-customising-device-types)

---

## 1. Project Setup

```bash
# Create a new Tauri + SolidJS project
npm create tauri-app@latest my-forensic-app -- --template solid-ts
cd my-forensic-app

# Add seven-zip as a workspace dependency
# In src-tauri/Cargo.toml add:
#   seven-zip = { path = "../path/to/sevenzip-ffi" }
```

---

## 2. Rust Backend – Evidence Form Module

Copy `tauri/src-tauri/src/evidence_form.rs` into your Tauri project's
`src-tauri/src/` directory.

The module exposes:

| Item | Description |
|---|---|
| `DeviceTypeConfig` | Struct: value, label, interfaces, protocols |
| `FormOption` | Struct: value, label, is_other |
| `device_type_configs()` | Returns `Vec<DeviceTypeConfig>` with all built-in device types |
| `get_device_type_configs()` | `#[tauri::command]` wrapper – call from the frontend via `invoke()` |

Add the module to `src-tauri/src/main.rs` (or `lib.rs`):

```rust
mod evidence_form;

fn main() {
    tauri::Builder::default()
        .invoke_handler(tauri::generate_handler![
            evidence_form::get_device_type_configs,
            // … other commands
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
```

Add `serde` to `src-tauri/Cargo.toml`:

```toml
[dependencies]
serde = { version = "1", features = ["derive"] }
serde_json = "1"
tauri = { version = "2", features = [] }
```

---

## 3. Frontend – TypeScript Data Model

Copy `tauri/src/deviceTypeData.ts` to your project's `src/` directory.

The file provides:

| Export | Description |
|---|---|
| `DEVICE_TYPE_CONFIGS` | Static array of all device type configurations |
| `DeviceTypeConfig` | TypeScript interface |
| `FormOption` | TypeScript interface |
| `getDeviceTypeConfig(value)` | Look up a config by device type value string |

> **Tip:** At runtime you can replace the static array with data from
> the Tauri command to keep the Rust and TypeScript lists in sync:
>
> ```ts
> import { invoke } from '@tauri-apps/api/core';
> import type { DeviceTypeConfig } from './deviceTypeData';
>
> const configs = await invoke<DeviceTypeConfig[]>('get_device_type_configs');
> ```

---

## 4. Frontend – EvidenceCollectionForm Component

Copy these two files to `src/components/`:

- `tauri/src/components/EvidenceCollectionForm.tsx`
- `tauri/src/components/EvidenceCollectionForm.css`

Import in your app:

```tsx
// src/App.tsx
import { EvidenceCollectionForm, EvidenceFormData } from './components/EvidenceCollectionForm';
import './components/EvidenceCollectionForm.css';

const App = () => {
  const handleSubmit = (data: EvidenceFormData) => {
    console.log('Evidence record saved:', data);
    // Persist to disk, archive with sevenzip-ffi, etc.
  };

  return <EvidenceCollectionForm onSubmit={handleSubmit} />;
};
```

---

## 5. Tauri Command Wiring

The `get_device_type_configs` command lets the frontend fetch the
authoritative list of device types from Rust at startup:

```ts
// src/deviceTypeStore.ts
import { createSignal, onMount } from 'solid-js';
import { invoke } from '@tauri-apps/api/core';
import { DEVICE_TYPE_CONFIGS, DeviceTypeConfig } from './deviceTypeData';

export const [deviceTypeConfigs, setDeviceTypeConfigs] =
  createSignal<DeviceTypeConfig[]>(DEVICE_TYPE_CONFIGS); // fallback to static

onMount(async () => {
  try {
    const configs = await invoke<DeviceTypeConfig[]>('get_device_type_configs');
    setDeviceTypeConfigs(configs);
  } catch (e) {
    console.warn('Could not load device type configs from backend, using static data', e);
  }
});
```

---

## 6. How Dynamic Fields Work

```
User selects Device Type
         │
         ▼
handleDeviceTypeChange(value)
         │  resets interface, interfaceOther,
         │         protocol,  protocolOther
         ▼
deviceConfig() = getDeviceTypeConfig(value)
         │
         ├─► Interface dropdown repopulates with config.interfaces
         └─► Protocol dropdown repopulates with config.protocols

Each dropdown always ends with:
  "Other (specify below)"  ← is_other: true

When "Other" is selected:
  └─► A free-text <input> appears below the dropdown
      so the examiner can enter a custom value.
```

### Device types and their default options

| Device Type | Example Interfaces | Example Protocols |
|---|---|---|
| Mobile Device | USB, Wi-Fi, Bluetooth, SIM, MicroSD | ADB, AFC/iTunes, JTAG, Chip-Off, FFS, Logical |
| Computer / Laptop | USB, Thunderbolt, SATA, NVMe, PCIe | Write-Blocker, Disk Imaging, Live Acquisition |
| External Storage | USB, SATA, NVMe, eSATA, FireWire | Write-Blocker, Disk Imaging, Hash Verify |
| Network Device | Ethernet, Console/Serial, USB, SFP | SSH, Telnet, SNMP, PCAP |
| IoT / Embedded | USB, UART, JTAG, SPI, I²C, SD/eMMC | JTAG, UART Dump, SPI Flash, Chip-Off |
| Vehicle / Telematics | OBD-II, USB, Bluetooth, CAN Bus | OBD-II, CAN Bus Dump, Manufacturer Tool |
| Cloud / VM | Cloud API, Network/VPN, SSH | REST API, Cloud CLI, VM Snapshot |
| Drone / UAV | USB, Wi-Fi, Bluetooth, MicroSD, UART | ADB, USB Mass Storage, Proprietary |
| Other Device | — | — |

All dropdowns always include **"Other (specify below)"** as the last
option.

---

## 7. Customising Device Types

### Add a new device type (Rust)

In `evidence_form.rs`, add a new entry to the `vec![]` inside
`device_type_configs()`:

```rust
DeviceTypeConfig {
    value: "game_console".to_string(),
    label: "Game Console".to_string(),
    interfaces: vec![
        FormOption::new("usb", "USB"),
        FormOption::new("hdmi", "HDMI"),
        FormOption::new("wifi", "Wi-Fi"),
        FormOption::other(),
    ],
    protocols: vec![
        FormOption::new("usb_mass_storage", "USB Mass Storage"),
        FormOption::new("chip_off", "Chip-Off / eMMC"),
        FormOption::other(),
    ],
},
```

Then mirror the change in `deviceTypeData.ts`.

### Add a new device type (TypeScript only)

If you don't need the Rust backend command, just add an entry to
`DEVICE_TYPE_CONFIGS` in `deviceTypeData.ts` using the same shape.

### Constraints enforced by tests

The unit tests in `evidence_form.rs` verify that:

- Every device type has at least one non-"Other" interface and protocol.
- The `Other` option is always the last entry in each list.
- No duplicate `value` strings exist within a device type.

Run them with:

```bash
cd tauri/src-tauri
cargo test
```
