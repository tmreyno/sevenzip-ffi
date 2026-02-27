# Using sevenzip-ffi in Tauri + SolidJS App

## Quick Start Guide

### Step 1: Add Rust Bindings to Your Tauri Project

In your Tauri project's `src-tauri/Cargo.toml`:

```toml
[dependencies]
tauri = { version = "2.0", features = [] }
serde = { version = "1", features = ["derive"] }
serde_json = "1"

# Add sevenzip-ffi Rust bindings
seven-zip = { path = "../sevenzip-ffi/rust" }
# Or from your path:
# seven-zip = { path = "/Users/terryreynolds/GitHub/sevenzip-ffi/rust" }
```

---

## Step 2: Create Tauri Commands

### `src-tauri/src/main.rs`:

```rust
// Prevent console window on Windows
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use seven_zip::{SevenZip, CompressionLevel};
use std::path::PathBuf;

/// Create a 7z archive from multiple files
#[tauri::command]
async fn create_archive(
    output_path: String,
    input_files: Vec<String>,
    compression_level: u8,
) -> Result<String, String> {
    // Convert compression level
    let level = match compression_level {
        0 => CompressionLevel::Store,
        1 => CompressionLevel::Fastest,
        3 => CompressionLevel::Fast,
        5 => CompressionLevel::Normal,
        7 => CompressionLevel::Maximum,
        9 => CompressionLevel::Ultra,
        _ => CompressionLevel::Normal,
    };
    
    // Convert strings to PathBuf
    let output = PathBuf::from(&output_path);
    let inputs: Vec<PathBuf> = input_files
        .iter()
        .map(|s| PathBuf::from(s))
        .collect();
    
    // Create archive
    SevenZip::compress_files(&inputs, &output, level)
        .map_err(|e| format!("Compression failed: {}", e))?;
    
    Ok(format!("Archive created: {}", output_path))
}

/// List contents of a 7z archive
#[tauri::command]
async fn list_archive(archive_path: String) -> Result<Vec<ArchiveFileInfo>, String> {
    let path = PathBuf::from(&archive_path);
    
    let entries = SevenZip::list_archive(&path)
        .map_err(|e| format!("Failed to list archive: {}", e))?;
    
    Ok(entries.into_iter().map(|e| ArchiveFileInfo {
        name: e.name,
        size: e.size,
        compressed_size: e.compressed_size,
        is_directory: e.is_directory,
    }).collect())
}

/// Extract archive to a directory
#[tauri::command]
async fn extract_archive(
    archive_path: String,
    output_dir: String,
) -> Result<String, String> {
    let archive = PathBuf::from(&archive_path);
    let output = PathBuf::from(&output_dir);
    
    SevenZip::extract_all(&archive, &output)
        .map_err(|e| format!("Extraction failed: {}", e))?;
    
    Ok(format!("Extracted to: {}", output_dir))
}

/// Create split/multi-volume archive
#[tauri::command]
async fn create_split_archive(
    output_path: String,
    input_files: Vec<String>,
    volume_size_mb: u64,
) -> Result<String, String> {
    let output = PathBuf::from(&output_path);
    let inputs: Vec<PathBuf> = input_files
        .iter()
        .map(|s| PathBuf::from(s))
        .collect();
    
    // Convert MB to bytes
    let volume_size = volume_size_mb * 1024 * 1024;
    
    SevenZip::compress_files_with_split(&inputs, &output, CompressionLevel::Normal, volume_size)
        .map_err(|e| format!("Split archive creation failed: {}", e))?;
    
    Ok(format!("Split archive created: {}.001, {}.002, ...", output_path, output_path))
}

// Struct for returning file info
#[derive(serde::Serialize)]
struct ArchiveFileInfo {
    name: String,
    size: u64,
    compressed_size: u64,
    is_directory: bool,
}

fn main() {
    tauri::Builder::default()
        .invoke_handler(tauri::generate_handler![
            create_archive,
            list_archive,
            extract_archive,
            create_split_archive,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
```

---

## Step 3: SolidJS Frontend

### `src/App.tsx`:

```tsx
import { createSignal, For } from 'solid-js';
import { invoke } from '@tauri-apps/api/core';
import { open, save } from '@tauri-apps/plugin-dialog';

interface ArchiveFileInfo {
  name: string;
  size: number;
  compressed_size: number;
  is_directory: boolean;
}

function App() {
  const [selectedFiles, setSelectedFiles] = createSignal<string[]>([]);
  const [archiveFiles, setArchiveFiles] = createSignal<ArchiveFileInfo[]>([]);
  const [status, setStatus] = createSignal('');
  const [compressionLevel, setCompressionLevel] = createSignal(5);

  // Select files to compress
  const selectFiles = async () => {
    const files = await open({
      multiple: true,
      directory: false,
    });
    
    if (files) {
      const fileList = Array.isArray(files) ? files : [files];
      setSelectedFiles(fileList);
      setStatus(`Selected ${fileList.length} file(s)`);
    }
  };

  // Create archive
  const createArchive = async () => {
    if (selectedFiles().length === 0) {
      setStatus('Please select files first');
      return;
    }

    const outputPath = await save({
      filters: [{ name: '7z Archive', extensions: ['7z'] }],
    });

    if (outputPath) {
      try {
        setStatus('Creating archive...');
        const result = await invoke<string>('create_archive', {
          outputPath,
          inputFiles: selectedFiles(),
          compressionLevel: compressionLevel(),
        });
        setStatus(result);
      } catch (error) {
        setStatus(`Error: ${error}`);
      }
    }
  };

  // Create split archive
  const createSplitArchive = async () => {
    if (selectedFiles().length === 0) {
      setStatus('Please select files first');
      return;
    }

    const volumeSize = prompt('Enter volume size in MB:', '100');
    if (!volumeSize) return;

    const outputPath = await save({
      filters: [{ name: '7z Archive', extensions: ['7z'] }],
    });

    if (outputPath) {
      try {
        setStatus('Creating split archive...');
        const result = await invoke<string>('create_split_archive', {
          outputPath,
          inputFiles: selectedFiles(),
          volumeSizeMb: parseInt(volumeSize),
        });
        setStatus(result);
      } catch (error) {
        setStatus(`Error: ${error}`);
      }
    }
  };

  // List archive contents
  const listArchive = async () => {
    const archivePath = await open({
      filters: [{ name: '7z Archive', extensions: ['7z', '001'] }],
    });

    if (archivePath && typeof archivePath === 'string') {
      try {
        setStatus('Reading archive...');
        const files = await invoke<ArchiveFileInfo[]>('list_archive', {
          archivePath,
        });
        setArchiveFiles(files);
        setStatus(`Archive contains ${files.length} item(s)`);
      } catch (error) {
        setStatus(`Error: ${error}`);
      }
    }
  };

  // Extract archive
  const extractArchive = async () => {
    const archivePath = await open({
      filters: [{ name: '7z Archive', extensions: ['7z', '001'] }],
    });

    if (archivePath && typeof archivePath === 'string') {
      const outputDir = await open({
        directory: true,
      });

      if (outputDir && typeof outputDir === 'string') {
        try {
          setStatus('Extracting...');
          const result = await invoke<string>('extract_archive', {
            archivePath,
            outputDir,
          });
          setStatus(result);
        } catch (error) {
          setStatus(`Error: ${error}`);
        }
      }
    }
  };

  return (
    <div class="container">
      <h1>7-Zip Archive Manager</h1>
      
      {/* Compression Section */}
      <div class="section">
        <h2>Create Archive</h2>
        <button onClick={selectFiles}>Select Files</button>
        <div>Selected: {selectedFiles().length} file(s)</div>
        
        <div class="compression-level">
          <label>
            Compression Level: {compressionLevel()}
            <input
              type="range"
              min="0"
              max="9"
              value={compressionLevel()}
              onInput={(e) => setCompressionLevel(parseInt(e.currentTarget.value))}
            />
          </label>
        </div>
        
        <button onClick={createArchive}>Create Archive</button>
        <button onClick={createSplitArchive}>Create Split Archive</button>
      </div>

      {/* Extraction Section */}
      <div class="section">
        <h2>Extract Archive</h2>
        <button onClick={listArchive}>List Archive Contents</button>
        <button onClick={extractArchive}>Extract Archive</button>
      </div>

      {/* Archive Contents */}
      {archiveFiles().length > 0 && (
        <div class="section">
          <h3>Archive Contents</h3>
          <table>
            <thead>
              <tr>
                <th>Name</th>
                <th>Size</th>
                <th>Compressed</th>
                <th>Type</th>
              </tr>
            </thead>
            <tbody>
              <For each={archiveFiles()}>
                {(file) => (
                  <tr>
                    <td>{file.name}</td>
                    <td>{formatSize(file.size)}</td>
                    <td>{formatSize(file.compressed_size)}</td>
                    <td>{file.is_directory ? 'DIR' : 'FILE'}</td>
                  </tr>
                )}
              </For>
            </tbody>
          </table>
        </div>
      )}

      {/* Status */}
      <div class="status">
        <strong>Status:</strong> {status()}
      </div>
    </div>
  );
}

function formatSize(bytes: number): string {
  if (bytes === 0) return '0 B';
  const k = 1024;
  const sizes = ['B', 'KB', 'MB', 'GB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return Math.round(bytes / Math.pow(k, i) * 100) / 100 + ' ' + sizes[i];
}

export default App;
```

### `src/App.css`:

```css
.container {
  max-width: 800px;
  margin: 0 auto;
  padding: 20px;
}

.section {
  margin: 20px 0;
  padding: 20px;
  border: 1px solid #ccc;
  border-radius: 8px;
}

button {
  margin: 5px;
  padding: 10px 20px;
  background: #0066cc;
  color: white;
  border: none;
  border-radius: 4px;
  cursor: pointer;
}

button:hover {
  background: #0052a3;
}

.compression-level {
  margin: 10px 0;
}

.compression-level input {
  width: 100%;
}

table {
  width: 100%;
  border-collapse: collapse;
  margin-top: 10px;
}

th, td {
  text-align: left;
  padding: 8px;
  border-bottom: 1px solid #ddd;
}

th {
  background-color: #f2f2f2;
}

.status {
  margin-top: 20px;
  padding: 10px;
  background: #f0f0f0;
  border-radius: 4px;
}
```

---

## Step 4: Build and Run

```bash
# From your Tauri project root
cd src-tauri

# Build (this will also build the sevenzip-ffi Rust bindings)
cargo build

# Run in development
cargo tauri dev

# Build for production
cargo tauri build
```

---

## Features Provided

✅ **Create Archives** - Single file archives with compression  
✅ **Create Split Archives** - Multi-volume archives  
✅ **List Contents** - View files in archive  
✅ **Extract Archives** - Extract to directory  
✅ **Compression Levels** - 0-9 (store to ultra)  
✅ **File Dialog Integration** - Native file picker  

---

## Project Structure

```
your-tauri-app/
├── src/                          # SolidJS frontend
│   ├── App.tsx                   # Main UI component
│   └── App.css                   # Styles
├── src-tauri/                    # Rust backend
│   ├── Cargo.toml                # Add seven-zip dependency
│   ├── src/
│   │   └── main.rs               # Tauri commands
│   └── target/                   # Build output
└── sevenzip-ffi/                 # This library
    └── rust/                     # Rust bindings
```

---

## Troubleshooting

### Build Errors

If you get linking errors, make sure the C library is built first:

```bash
cd /Users/terryreynolds/GitHub/sevenzip-ffi
cmake --build build -j8
```

### Path Issues

Update the path in `Cargo.toml` to point to your sevenzip-ffi location:

```toml
seven-zip = { path = "/Users/terryreynolds/GitHub/sevenzip-ffi/rust" }
```

---

## Next Steps

1. **Copy this code** to your Tauri project
2. **Update paths** in Cargo.toml
3. **Run `cargo build`** to compile
4. **Test with `cargo tauri dev`**
5. **Customize UI** to match your app design

🎉 **You now have a full 7-Zip archive manager in your Tauri + SolidJS app!**
