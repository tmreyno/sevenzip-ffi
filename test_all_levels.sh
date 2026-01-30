#!/bin/bash

# Test all compression levels with SDK auto-optimization
# Shows performance and CPU usage for Levels 0-5

SOURCE="/Users/terryreynolds/1827-1001 Case With Data /2.Processed.Database"
SOURCE_SIZE="7.3 GB"

cat << 'HEADER'
╔════════════════════════════════════════════════════════════════════════╗
║          SDK AUTO-OPTIMIZATION TEST - ALL COMPRESSION LEVELS           ║
╚════════════════════════════════════════════════════════════════════════╝

Source:      7.3 GB Forensic Database (72 files)
Threads:     10 (all cores)
Split Size:  2 GB volumes
Dictionary:  SDK Auto-Optimized (varies by level)

SDK Dictionary Formulas:
  Level 0 (Store):   No compression
  Level 1 (Fastest): 1 << (1*2+16) = 256 KB
  Level 2:           1 << (2*2+16) = 512 KB  
  Level 3 (Fast):    1 << (3*2+16) = 1 MB
  Level 4:           1 << (4*2+16) = 4 MB
  Level 5 (Normal):  1 << (5*2+16) = 16 MB

HEADER

echo ""
echo "═══════════════════════════════════════════════════════════════════════════"
echo "STEP 2: Testing All Levels"
echo "═══════════════════════════════════════════════════════════════════════════"
echo ""

# Create a temporary Rust test program
cat > /tmp/test_all_levels.rs << 'RUSTCODE'
use seven_zip::{SevenZip, CompressionLevel, StreamOptions, Error};
use std::time::Instant;

fn test_level(sz: &SevenZip, level: CompressionLevel, level_name: &str, level_num: u8) -> Result<(), Error> {
    let source = "/Users/terryreynolds/1827-1001 Case With Data /2.Processed.Database";
    let output = format!("/tmp/test_level_{}.7z", level_num);
    
    // Clean up previous test
    let _ = std::fs::remove_file(&format!("{}.001", output));
    let _ = std::fs::remove_file(&format!("{}.002", output));
    let _ = std::fs::remove_file(&format!("{}.003", output));
    
    println!("\n╔════════════════════════════════════════════════════════════════════════╗");
    println!("║  LEVEL {} - {}                                                     ", level_num, level_name);
    println!("╚════════════════════════════════════════════════════════════════════════╝\n");
    
    let mut opts = StreamOptions::default();
    opts.num_threads = 10;
    opts.split_size = 2 * 1024 * 1024 * 1024;
    opts.chunk_size = 64 * 1024 * 1024;
    
    // Dictionary info based on level
    let dict_info = match level_num {
        0 => "N/A (Store only)",
        1 => "256 KB",
        2 => "512 KB",
        3 => "1 MB",
        4 => "4 MB",
        5 => "16 MB",
        _ => "Unknown"
    };
    
    println!("Configuration:");
    println!("  Level:       {} ({})", level_num, level_name);
    println!("  Dictionary:  {} (SDK auto-optimized)", dict_info);
    println!("  Threads:     10");
    println!("  Split Size:  2 GB");
    println!("\nCompressing...\n");
    
    let start = Instant::now();
    let start_time = start.clone();
    
    let progress_callback = Box::new(move |processed: u64, total: u64, _fb: u64, _ft: u64, filename: &str| {
        if total > 0 {
            let pct = (processed as f64 / total as f64) * 100.0;
            let elapsed = start_time.elapsed().as_secs_f64();
            let speed = if elapsed > 0.0 { processed as f64 / elapsed / 1_000_000.0 } else { 0.0 };
            print!("\r[{:50}] {:.1}% | {:.1} MB/s | {} ", 
                "=".repeat((pct / 2.0) as usize),
                pct,
                speed,
                filename.split('/').last().unwrap_or(filename));
            use std::io::Write;
            std::io::stdout().flush().ok();
        }
    });
    
    sz.create_archive_streaming(
        &output,
        &[source],
        level,
        Some(&opts),
        Some(progress_callback)
    )?;
    
    let duration = start.elapsed();
    println!("\n");
    
    // Get file sizes
    let vol1_size = std::fs::metadata(format!("{}.001", output)).ok().map(|m| m.len()).unwrap_or(0);
    let vol2_size = std::fs::metadata(format!("{}.002", output)).ok().map(|m| m.len()).unwrap_or(0);
    let vol3_size = std::fs::metadata(format!("{}.003", output)).ok().map(|m| m.len()).unwrap_or(0);
    let total_size = vol1_size + vol2_size + vol3_size;
    
    let volumes = if vol3_size > 0 { 3 } else if vol2_size > 0 { 2 } else { 1 };
    
    println!("Results:");
    println!("  Time:        {:.2}s ({:.0}:{:02})", 
        duration.as_secs_f64(), 
        duration.as_secs() / 60,
        duration.as_secs() % 60);
    println!("  Speed:       {:.1} MB/s", 7300.0 / duration.as_secs_f64());
    println!("  Total Size:  {:.2} GB", total_size as f64 / 1_000_000_000.0);
    println!("  Volumes:     {}", volumes);
    println!("  Ratio:       {:.2}:1", 7.3 / (total_size as f64 / 1_000_000_000.0));
    
    Ok(())
}

fn main() -> Result<(), Error> {
    let sz = SevenZip::new()?;
    
    test_level(&sz, CompressionLevel::Store, "Store (No Compression)", 0)?;
    test_level(&sz, CompressionLevel::Fastest, "Fastest", 1)?;
    test_level(&sz, CompressionLevel::Fast, "Fast", 3)?;
    test_level(&sz, CompressionLevel::Normal, "Normal", 5)?;
    
    println!("\n╔════════════════════════════════════════════════════════════════════════╗");
    println!("║                       ALL TESTS COMPLETE                               ║");
    println!("╚════════════════════════════════════════════════════════════════════════╝\n");
    
    Ok(())
}
RUSTCODE

echo "STEP 3: Building comprehensive test suite..."
echo ""

# Copy to rust examples
cp /tmp/test_all_levels.rs rust/examples/

cd rust
cargo build --release --example test_all_levels 2>&1 | grep -E "(Compiling|Finished)"

echo ""
echo "═══════════════════════════════════════════════════════════════════════════"
echo "STEP 4: Running Comprehensive Tests"
echo "═══════════════════════════════════════════════════════════════════════════"
echo ""

time ./target/release/examples/test_all_levels

echo ""
echo "═══════════════════════════════════════════════════════════════════════════"
echo "STEP 5: Final Archive Sizes"
echo "═══════════════════════════════════════════════════════════════════════════"
echo ""

ls -lh /tmp/test_level_*.7z* 2>/dev/null | awk '{print $9, $5}'

echo ""
echo "═══════════════════════════════════════════════════════════════════════════"
echo "All tests complete!"
echo "═══════════════════════════════════════════════════════════════════════════"
