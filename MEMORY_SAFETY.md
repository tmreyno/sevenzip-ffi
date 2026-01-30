# Memory Safety Guide - Large File Compression

## ⚠️ Critical Issue: Memory Exhaustion

The 7z FFI SDK provides two compression APIs with **very different memory characteristics**:

### 1. `create_archive()` - IN-MEMORY COMPRESSION ❌ Not for large files

```rust
// WARNING: This loads entire files into RAM!
sz.create_archive("output.7z", &["/path/to/large/folder"], level, None)?;
```

**Problem**: This function reads entire files into memory before compression.
- For an large data directory, this will attempt to allocate ~large of RAM
- Will cause system failure, swap thrashing, or OOM killer activation
- macOS will become unresponsive and may force a reboot

**Safe Usage**: Only for files < 1GB total

### 2. `create_archive_streaming()` - STREAMING COMPRESSION ✅ Use for large files

```rust
// SAFE: Uses 64MB chunks, never loads entire files
let mut opts = StreamOptions::default();
opts.chunk_size = 64 * 1024 * 1024;  // 64MB chunks
opts.split_size = 8 * 1024 * 1024 * 1024;  // 8GB split volumes

sz.create_archive_streaming(
    "output.7z",
    &["/path/to/large/folder"],
    CompressionLevel::Normal,
    Some(&opts),
    Some(progress_callback)
)?;
```

**Features**:
- Processes files in configurable chunks (default 64MB)
- Never loads entire files into memory
- Peak memory usage: ~200-500MB regardless of input size
- Supports split volumes for very large archives
- Progress callbacks show real-time status

## Memory Comparison

| Method | large Input | Peak RAM Usage | Safe? |
|--------|------------|----------------|-------|
| `create_archive()` | large | ~large+ | ❌ NO |
| `create_archive_streaming()` | large | ~400MB | ✅ YES |

## When to Use Each Method

### Use `create_archive()` for:
- Small files (< 100MB each)
- Quick one-off compressions
- Testing and development
- Total input < 1GB

### Use `create_archive_streaming()` for:
- Large files (any size)
- Large files (large+)
- Deployments
- Split archive creation
- Progress monitoring needs

## C API Equivalents

| Rust Method | C Function | Memory Safe |
|-------------|------------|-------------|
| `create_archive()` | `sevenzip_create_7z()` | ❌ No |
| `create_archive_streaming()` | `sevenzip_create_7z_streaming()` | ✅ Yes |

## Example: Safe large Compression

```rust
use seven_zip::{SevenZip, CompressionLevel, StreamOptions};

fn compress_large_files(input_path: &str, output_path: &str) -> Result<(), seven_zip::Error> {
    let sz = SevenZip::new()?;
    
    let mut opts = StreamOptions::default();
    opts.num_threads = 8;                           // Use 8 CPU threads
    opts.chunk_size = 64 * 1024 * 1024;             // 64MB chunks (memory-safe)
    opts.split_size = 8 * 1024 * 1024 * 1024;       // 8GB split volumes
    
    let start = std::time::Instant::now();
    
    sz.create_archive_streaming(
        output_path,
        &[input_path],
        CompressionLevel::Normal,
        Some(&opts),
        Some(Box::new(move |processed, total, _, _, filename| {
            let pct = if total > 0 { (processed as f64 / total as f64) * 100.0 } else { 0.0 };
            let elapsed = start.elapsed().as_secs_f64();
            let speed = if elapsed > 0.0 { processed as f64 / elapsed / 1_000_000.0 } else { 0.0 };
            print!("\r[{:50}] {:.1}% | {:.1} MB/s | {}", 
                "=".repeat((pct / 2.0) as usize), pct, speed, 
                filename.split('/').last().unwrap_or(filename));
            std::io::Write::flush(&mut std::io::stdout()).ok();
        }))
    )?;
    
    println!("\n✓ Compression complete!");
    Ok(())
}
```

## Recommended Settings for Large Archives

Recommended settings for large file archival:

```rust
let mut opts = StreamOptions::default();
opts.num_threads = 8;                    // Adjust based on CPU cores
opts.chunk_size = 64 * 1024 * 1024;      // 64MB - good balance
opts.split_size = 8ULL * 1024 * 1024 * 1024;  // 8GB volumes
opts.solid = true;                        // Better compression
```

**Expected Performance**:
- Compression speed: ~20-50 MB/s (depends on data type)
- Time for large: ~30-60 minutes
- Output size: ~40-60% of input (depends on data compressibility)
- Memory usage: ~400-500MB peak

## Monitoring Memory During Compression

```bash
# Monitor memory usage in another terminal
while true; do
    ps aux | grep archive_tool | grep -v grep | awk '{print $6/1024 " MB"}'
    sleep 5
done
```

If you see memory exceeding 1GB during compression, you're using the wrong API!

## Recovery from Memory Exhaustion

If the system became unresponsive during compression:

1. Force reboot if necessary (hold power button)
2. After reboot, remove partial archive files:
   ```bash
   rm -f output.7z output.7z.*
   ```
3. Use the streaming API instead:
   ```bash
   cargo run --release --example archive_tool -- compress output.7z /path/to/input
   ```

## Updated: January 29, 2026

The `archive_tool` example has been fixed to use `create_archive_streaming()` 
by default, preventing memory exhaustion for large data directories.
