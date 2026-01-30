# Tauri Integration Guide

This guide shows how to integrate the 7z FFI SDK into your Tauri application.

## Method 1: Using Rust FFI (Recommended)

### Step 1: Build the Library

```bash
cd sevenzip-ffi
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### Step 2: Add to Your Tauri Project

1. Copy the built library to your Tauri project:
   - Linux: `lib7z_ffi.so` → `src-tauri/lib/`
   - macOS: `lib7z_ffi.dylib` → `src-tauri/lib/`
   - Windows: `7z_ffi.dll` → `src-tauri/lib/`

2. Create a Rust FFI wrapper in `src-tauri/src/sevenzip_ffi.rs`:

```rust
use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_int, c_void};

#[repr(C)]
pub struct SevenZipEntry {
    name: *mut c_char,
    size: u64,
    packed_size: u64,
    modified_time: u64,
    attributes: u32,
    is_directory: c_int,
}

#[repr(C)]
pub struct SevenZipList {
    entries: *mut SevenZipEntry,
    count: usize,
}

type ProgressCallback = extern "C" fn(completed: u64, total: u64, user_data: *mut c_void);

#[link(name = "7z_ffi")]
extern "C" {
    fn sevenzip_init() -> c_int;
    fn sevenzip_cleanup();
    fn sevenzip_extract(
        archive_path: *const c_char,
        output_dir: *const c_char,
        password: *const c_char,
        progress_callback: Option<ProgressCallback>,
        user_data: *mut c_void,
    ) -> c_int;
    fn sevenzip_list(
        archive_path: *const c_char,
        password: *const c_char,
        list: *mut *mut SevenZipList,
    ) -> c_int;
    fn sevenzip_free_list(list: *mut SevenZipList);
    fn sevenzip_get_error_message(error_code: c_int) -> *const c_char;
    fn sevenzip_get_version() -> *const c_char;
}

pub struct SevenZip;

impl SevenZip {
    pub fn init() -> Result<(), String> {
        unsafe {
            let result = sevenzip_init();
            if result == 0 {
                Ok(())
            } else {
                let msg = CStr::from_ptr(sevenzip_get_error_message(result));
                Err(msg.to_string_lossy().into_owned())
            }
        }
    }

    pub fn cleanup() {
        unsafe {
            sevenzip_cleanup();
        }
    }

    pub fn extract(archive_path: &str, output_dir: &str) -> Result<(), String> {
        let archive_path_c = CString::new(archive_path).unwrap();
        let output_dir_c = CString::new(output_dir).unwrap();

        unsafe {
            let result = sevenzip_extract(
                archive_path_c.as_ptr(),
                output_dir_c.as_ptr(),
                std::ptr::null(),
                None,
                std::ptr::null_mut(),
            );

            if result == 0 {
                Ok(())
            } else {
                let msg = CStr::from_ptr(sevenzip_get_error_message(result));
                Err(msg.to_string_lossy().into_owned())
            }
        }
    }

    pub fn list(archive_path: &str) -> Result<Vec<FileEntry>, String> {
        let archive_path_c = CString::new(archive_path).unwrap();
        let mut list_ptr: *mut SevenZipList = std::ptr::null_mut();

        unsafe {
            let result = sevenzip_list(
                archive_path_c.as_ptr(),
                std::ptr::null(),
                &mut list_ptr,
            );

            if result != 0 {
                let msg = CStr::from_ptr(sevenzip_get_error_message(result));
                return Err(msg.to_string_lossy().into_owned());
            }

            if list_ptr.is_null() {
                return Ok(Vec::new());
            }

            let list = &*list_ptr;
            let mut entries = Vec::new();

            for i in 0..list.count {
                let entry = &*list.entries.add(i);
                let name = if !entry.name.is_null() {
                    CStr::from_ptr(entry.name).to_string_lossy().into_owned()
                } else {
                    String::from("(unknown)")
                };

                entries.push(FileEntry {
                    name,
                    size: entry.size,
                    packed_size: entry.packed_size,
                    modified_time: entry.modified_time,
                    is_directory: entry.is_directory != 0,
                });
            }

            sevenzip_free_list(list_ptr);
            Ok(entries)
        }
    }

    pub fn version() -> String {
        unsafe {
            let version = CStr::from_ptr(sevenzip_get_version());
            version.to_string_lossy().into_owned()
        }
    }
}

#[derive(Debug, serde::Serialize)]
pub struct FileEntry {
    pub name: String,
    pub size: u64,
    pub packed_size: u64,
    pub modified_time: u64,
    pub is_directory: bool,
}
```

### Step 3: Create Tauri Commands

Add to `src-tauri/src/main.rs`:

```rust
mod sevenzip_ffi;

use sevenzip_ffi::{SevenZip, FileEntry};

#[tauri::command]
fn extract_archive(archive_path: String, output_dir: String) -> Result<(), String> {
    SevenZip::extract(&archive_path, &output_dir)
}

#[tauri::command]
fn list_archive(archive_path: String) -> Result<Vec<FileEntry>, String> {
    SevenZip::list(&archive_path)
}

#[tauri::command]
fn get_version() -> String {
    SevenZip::version()
}

fn main() {
    // Initialize 7z library
    SevenZip::init().expect("Failed to initialize 7z library");

    tauri::Builder::default()
        .invoke_handler(tauri::generate_handler![
            extract_archive,
            list_archive,
            get_version
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
```

### Step 4: Configure Cargo

Add to `src-tauri/Cargo.toml`:

```toml
[dependencies]
tauri = { version = "1.5", features = ["shell-open"] }
serde = { version = "1.0", features = ["derive"] }
serde_json = "1.0"

[build-dependencies]
tauri-build = { version = "1.5", features = [] }
```

### Step 5: Configure Build Script

Create/update `src-tauri/build.rs`:

```rust
fn main() {
    // Link to the 7z FFI library
    println!("cargo:rustc-link-search=native=lib");
    println!("cargo:rustc-link-lib=dylib=7z_ffi");
    
    tauri_build::build()
}
```

### Step 6: Use in Frontend

```typescript
import { invoke } from '@tauri-apps/api/tauri';

// Extract archive
async function extractArchive(archivePath: string, outputDir: string) {
  try {
    await invoke('extract_archive', { archivePath, outputDir });
    console.log('Extraction complete');
  } catch (error) {
    console.error('Extraction failed:', error);
  }
}

// List archive contents
async function listArchive(archivePath: string) {
  try {
    const entries = await invoke('list_archive', { archivePath });
    console.log('Archive contents:', entries);
    return entries;
  } catch (error) {
    console.error('Failed to list archive:', error);
  }
}

// Get version
async function getVersion() {
  const version = await invoke('get_version');
  console.log('7z FFI SDK version:', version);
}
```

## Method 2: Using Node Native Modules (Alternative)

If you're using a Node.js-based approach with Tauri, you can create a Node.js native addon:

1. Use `node-gyp` to build a Node.js native module
2. Wrap the 7z FFI library
3. Use it in your Electron/Node.js context

## Platform-Specific Notes

### macOS
- Library extension: `.dylib`
- May need to sign the library for distribution
- Set `DYLD_LIBRARY_PATH` if needed during development

### Linux
- Library extension: `.so`
- May need to set `LD_LIBRARY_PATH` during development
- Consider using `rpath` for distribution

### Windows
- Library extension: `.dll`
- Place DLL next to executable or in system PATH
- May need to include MSVC redistributables

## Packaging for Distribution

Add the library to Tauri's bundle resources in `tauri.conf.json`:

```json
{
  "tauri": {
    "bundle": {
      "resources": [
        "lib/*"
      ]
    }
  }
}
```

## Testing

Create tests in `src-tauri/tests/sevenzip_tests.rs`:

```rust
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_version() {
        SevenZip::init().unwrap();
        let version = SevenZip::version();
        assert!(!version.is_empty());
        SevenZip::cleanup();
    }

    #[test]
    fn test_list_archive() {
        SevenZip::init().unwrap();
        let result = SevenZip::list("test.7z");
        // Add assertions based on test archive
        SevenZip::cleanup();
    }
}
```
