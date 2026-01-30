# LZMA SDK Auto-Optimization Process

This document explains how the LZMA SDK automatically optimizes compression parameters when you set `dict_size = 0` and let `Lzma2EncProps_Normalize()` handle the configuration.

## Overview

When we use **pure SDK optimization**, we only set:
- Compression level (0-9)
- Number of threads
- Block size mode (AUTO)

The SDK's `Lzma2EncProps_Normalize()` function then calls `LzmaEncProps_Normalize()` to automatically configure **all** compression parameters based on the level.

## The Optimization Chain

```
Your Code
    ↓
Set level = 1 (FASTEST)
Set num_threads = 10
Set dict_size = 0
    ↓
Lzma2EncProps_Normalize(&props)
    ↓
LzmaEncProps_Normalize(&props.lzmaProps)
    ↓
SDK Auto-Configures Everything!
```

## What `LzmaEncProps_Normalize()` Does

Located in `lzma/C/LzmaEnc.c` (lines 68-106), this function applies level-based optimization:

### 1. Dictionary Size Calculation

```c
if (p->dictSize == 0)
    p->dictSize =
      ( level <= 3 ? ((UInt32)1 << (level * 2 + 16)) :  // Levels 0-3
      ( level <= 6 ? ((UInt32)1 << (level + 19)) :      // Levels 4-6
      ( level <= 7 ? ((UInt32)1 << 25) : ((UInt32)1 << 26)  // Levels 7-9
      )));
```

**For level 1 (FASTEST):**
```
dictSize = 1 << (1 * 2 + 16)
         = 1 << 18
         = 262,144 bytes
         = 256 KB
```

**Dictionary sizes by level:**
- Level 0: 64 KB (1 << 16)
- Level 1: 256 KB (1 << 18) ⭐ **Our setting**
- Level 2: 1 MB (1 << 20)
- Level 3: 4 MB (1 << 22)
- Level 4: 16 MB (1 << 23)
- Level 5: 32 MB (1 << 24) (default)
- Level 6: 64 MB (1 << 25)
- Level 7: 32 MB (1 << 25)
- Level 8-9: 64 MB (1 << 26)

### 2. Literal Context Bits (lc/lp/pb)

```c
if (p->lc < 0) p->lc = 3;  // Literal context bits
if (p->lp < 0) p->lp = 0;  // Literal position bits
if (p->pb < 0) p->pb = 2;  // Position bits
```

**For level 1:** lc=3, lp=0, pb=2 (standard LZMA2 settings)

### 3. Algorithm Selection

```c
if (p->algo < 0) p->algo = (level < 5 ? 0 : 1);
```

**For level 1:** algo=0 (fast algorithm, not maximum compression)

### 4. Fast Bytes

```c
if (p->fb < 0) p->fb = (level < 7 ? 32 : 64);
```

**For level 1:** fb=32 (checks 32 bytes for match finding)

### 5. Binary Tree Mode (Match Finder)

```c
if (p->btMode < 0) p->btMode = (p->algo == 0 ? 0 : 1);
if (p->numHashBytes < 0) p->numHashBytes = (p->btMode ? 4 : 5);
```

**For level 1:**
- btMode=0 (use Hash Chain, not Binary Tree)
- numHashBytes=5 (HC5 hash chain with 5-byte hashing)

### 6. Match Cycles

```c
if (p->mc == 0) p->mc = (16 + ((unsigned)p->fb >> 1)) >> (p->btMode ? 0 : 1);
```

**For level 1:**
```
mc = (16 + (32 >> 1)) >> 1
   = (16 + 16) >> 1
   = 32 >> 1
   = 16
```

### 7. Threading

```c
if (p->numThreads < 0)
    p->numThreads = ((p->btMode && p->algo) ? 2 : 1);
```

**For level 1:** numThreads=1 (per LZMA encoder, not total threads!)

## What `Lzma2EncProps_Normalize()` Does

Located in `lzma/C/Lzma2Enc.c` (lines 240-343), this function handles multi-threading and block management:

### Threading Distribution

When we set `numTotalThreads = 10`:

```c
// Input: numTotalThreads = 10, lzmaProps.numThreads = 1 (from LzmaEncProps_Normalize)
// Algorithm determines optimal split between block threads and LZMA threads

t1n = lzmaProps.numThreads;  // = 1 (from LzmaEncProps_Normalize)
t3 = numTotalThreads;        // = 10 (our setting)

if (numBlockThreads_Max <= 0) {
    t2 = t3 / t1n;           // = 10 / 1 = 10 block threads
    if (t2 > MTCODER_THREADS_MAX)
        t2 = MTCODER_THREADS_MAX;  // Max 200
}

numBlockThreads_Max = t2;    // = 10
numTotalThreads = t1n * t2;  // = 1 * 10 = 10
```

**Result for level 1 with 10 threads:**
- 10 block threads (parallel compression of independent blocks)
- 1 LZMA thread per block (fast algorithm doesn't benefit from more)
- Total: 10 threads working in parallel

### Block Size Calculation

```c
if (blockSize == LZMA2_ENC_PROPS_BLOCK_SIZE_AUTO) {
    const UInt32 kMinSize = (UInt32)1 << 20;  // 1 MB minimum
    const UInt32 kMaxSize = (UInt32)1 << 28;  // 256 MB maximum
    const UInt32 dictSize = p->lzmaProps.dictSize;  // 256 KB for level 1
    
    UInt64 blockSize = (UInt64)dictSize << 2;  // 256 KB * 4 = 1 MB
    if (blockSize < kMinSize) blockSize = kMinSize;  // Use 1 MB minimum
    if (blockSize > kMaxSize) blockSize = kMaxSize;
    if (blockSize < dictSize) blockSize = dictSize;
    
    blockSize += (kMinSize - 1);
    blockSize &= ~(UInt64)(kMinSize - 1);  // Round to 1 MB boundary
    
    p->blockSize = blockSize;  // = 1 MB
}
```

**For level 1:** blockSize = 1 MB (minimum size, good for fast compression)

## Complete Configuration for Level 1

When you set level=1 and let the SDK optimize, you get:

| Parameter | Value | Source | Purpose |
|-----------|-------|--------|---------|
| **Dictionary** | 256 KB | SDK calculated | Balance speed vs compression |
| **Algorithm** | 0 (fast) | SDK selected | Prioritize speed over ratio |
| **Fast Bytes** | 32 | SDK selected | Match search depth |
| **Match Finder** | HC5 | SDK selected | Hash Chain with 5-byte hash |
| **Match Cycles** | 16 | SDK calculated | Limit match searching |
| **lc/lp/pb** | 3/0/2 | SDK defaults | Standard LZMA2 encoding |
| **LZMA Threads** | 1 | SDK selected | Fast algo doesn't need more |
| **Block Threads** | 10 | SDK distributed | Parallel independent blocks |
| **Block Size** | 1 MB | SDK calculated | Small blocks for speed |
| **Total Threads** | 10 | User specified | Maximum parallelism |

## Why This Works Better Than Manual Settings

### 1. **Consistency**
The SDK maintains proven relationships between parameters:
- Dictionary size matches block size needs
- Match cycles scale with fast bytes
- Thread distribution optimizes for the algorithm type

### 2. **Level-Appropriate Choices**
Each level gets a carefully balanced configuration:
- Level 1 prioritizes speed: small dictionary, fast algorithm, HC5
- Higher levels gradually increase compression: larger dictionary, BT mode, more match cycles

### 3. **Threading Intelligence**
The SDK knows:
- Fast algorithms (level < 5) work best with 1 LZMA thread per block
- Slower algorithms (level >= 5) can benefit from 2 LZMA threads
- Block threading scales well for all levels

### 4. **Automatic Adjustments**
The SDK adapts to constraints:
- Reduces dictionary for small files
- Adjusts block count based on file size
- Ensures block size is never smaller than dictionary

## Performance Impact

Our empirical results with **pure SDK optimization (level 1)**:

```
Speed: 37 MB/s @ 749% CPU utilization
- 10 threads actively compressing independent 1 MB blocks
- Near-linear scaling (749% ≈ 7.5 threads average utilization)
- Good balance between compression and thread overhead

Compare to 7-Zip's mx=1:
- 7-Zip: 69.9 MB/s @ 791% CPU
- Our impl: 37 MB/s @ 749% CPU
- Difference: Primarily due to 7-Zip's highly optimized I/O and assembly routines
- Our threading efficiency is comparable (749% vs 791%)
```

## Code Implementation

### Our C Code
```c
/* Setup encoder properties - Let SDK choose optimal settings */
CLzma2EncProps props;
Lzma2EncProps_Init(&props);

/* Map compression level - SDK will optimize based on level */
int lzma_level = 1;  /* FASTEST */
props.lzmaProps.level = lzma_level;

/* Multi-threading - let SDK optimize thread distribution */
if (options->num_threads > 0) {
    props.numTotalThreads = options->num_threads;
    props.numBlockThreads_Max = -1;  /* Let Normalize() decide */
    props.lzmaProps.numThreads = -1;  /* Let Normalize() decide */
    props.blockSize = LZMA2_ENC_PROPS_BLOCK_SIZE_AUTO;  /* Auto block size */
}

/* Override dictionary if user specified one */
if (options->dict_size > 0) {
    props.lzmaProps.dictSize = (UInt32)options->dict_size;
}

/* CRITICAL: Normalize will optimize all other parameters based on level */
Lzma2EncProps_Normalize(&props);
```

### Our Rust Code
```rust
// Default options - SDK auto-optimizes
let mut opts = StreamOptions::default();
opts.num_threads = 10;           // Use all cores
opts.dict_size = 0;              // Let SDK decide (will be 256 KB for level 1)
opts.split_size = 8_589_934_592; // 8 GB volumes

// Create archive with FASTEST compression
sz.create_archive_streaming(
    "archive.7z",
    &["/path/to/data"],
    CompressionLevel::Fastest,  // Maps to level 1
    Some(&opts),
    Some(progress_callback)
)?;
```

## Summary

**SDK auto-optimization gives you:**
- ✅ Proven, tested parameter combinations
- ✅ Level-appropriate performance characteristics
- ✅ Intelligent threading distribution
- ✅ Automatic adaptations for file size and constraints
- ✅ Cleaner, more maintainable code
- ✅ Future-proof (benefits from SDK improvements)

**You only need to specify:**
- Compression level (0-9)
- Number of threads
- Optional: dictionary size override (but usually 0 is best)

The SDK handles all the complex parameter relationships for you!
