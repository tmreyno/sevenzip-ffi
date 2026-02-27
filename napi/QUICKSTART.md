# Quick Start: @sevenzip/napi

Get started with 7z compression in Node.js in under 5 minutes.

## Installation

```bash
npm install @sevenzip/napi
```

The package automatically installs the correct native addon for your platform (macOS, Linux, or Windows).

## Basic Usage

### Compress & Decompress Buffers

```javascript
const { compress, decompress } = require('@sevenzip/napi');

// Compress data
const input = Buffer.from('Hello, World!');
const compressed = await compress(input, { level: 9 });

console.log(`Compressed: ${input.length} → ${compressed.length} bytes`);

// Decompress data
const output = await decompress(compressed);
console.log(output.toString()); // "Hello, World!"
```

### File Operations

```javascript
const { compressFile, extractFile } = require('@sevenzip/napi');

// Compress a file
await compressFile('document.txt', 'archive.7z');

// Extract an archive
await extractFile('archive.7z', './output/');
```

### TypeScript

```typescript
import { compress, CompressOptions } from '@sevenzip/napi';

const options: CompressOptions = {
  level: 9,      // 0-9 compression level
  useXz: false   // Use XZ format instead of LZMA
};

const compressed = await compress(Buffer.from('data'), options);
```

## Compression Levels

| Level | Speed | Ratio | Use Case |
|-------|-------|-------|----------|
| 0     | Fastest | Lowest | Real-time streaming |
| 1-3   | Fast | Low | Quick backups |
| 5     | **Balanced** | **Good** | **General use** |
| 7-9   | Slow | Best | Archival storage |

## Format Options

### LZMA (Default)

Best for: Cross-platform compatibility, 7z archives

```javascript
const compressed = await compress(data, { level: 5 });
```

### XZ Format

Best for: Linux/Unix environments, better error detection

```javascript
const compressed = await compress(data, { level: 5, useXz: true });
```

## Real-World Examples

### API Response Compression

```javascript
const express = require('express');
const { compress } = require('@sevenzip/napi');

app.get('/api/data', async (req, res) => {
  const data = await fetchLargeData();
  const json = JSON.stringify(data);
  
  if (req.headers['accept-encoding']?.includes('7z')) {
    const compressed = await compress(Buffer.from(json), { level: 5 });
    res.set('Content-Encoding', '7z');
    res.send(compressed);
  } else {
    res.json(data);
  }
});
```

### Log File Archival

```javascript
const fs = require('fs').promises;
const { compressFile } = require('@sevenzip/napi');

async function archiveLogs() {
  const files = await fs.readdir('/var/log/app');
  
  for (const file of files) {
    if (file.endsWith('.log')) {
      await compressFile(`/var/log/app/${file}`, `/var/log/archive/${file}.7z`);
      await fs.unlink(`/var/log/app/${file}`);
    }
  }
}
```

### Stream Processing

```javascript
const { compress, compressBound } = require('@sevenzip/napi');

async function compressStream(readable) {
  const chunks = [];
  
  for await (const chunk of readable) {
    chunks.push(chunk);
  }
  
  const buffer = Buffer.concat(chunks);
  const maxSize = compressBound(buffer.length);
  
  console.log(`Max compressed size: ${maxSize} bytes`);
  
  return await compress(buffer, { level: 6 });
}
```

## Performance Tips

1. **Choose the right level**: Level 5 is optimal for most use cases
2. **Pre-allocate buffers**: Use `compressBound()` to calculate max size
3. **Async operations**: All functions are async and non-blocking
4. **Large files**: Use file operations instead of loading into memory
5. **Batch operations**: Compress multiple files into one archive

## API Reference

### compress(input, options?)

Compress a buffer.

- **input**: `Buffer` - Data to compress
- **options**: `CompressOptions` - Compression settings
  - `level`: `number` - 0-9 (default: 5)
  - `useXz`: `boolean` - Use XZ format (default: false)
- **Returns**: `Promise<Buffer>` - Compressed data

### decompress(input)

Decompress a buffer (auto-detects format).

- **input**: `Buffer` - Compressed data
- **Returns**: `Promise<Buffer>` - Decompressed data

### compressFile(inputPath, outputPath)

Compress a file to a 7z archive.

- **inputPath**: `string` - Path to input file
- **outputPath**: `string` - Path to output .7z file
- **Returns**: `Promise<void>`

### extractFile(archivePath, outputDir)

Extract a 7z archive.

- **archivePath**: `string` - Path to .7z file
- **outputDir**: `string` - Directory to extract to
- **Returns**: `Promise<void>`

### compressBound(inputSize)

Calculate maximum compressed size.

- **inputSize**: `number` - Original data size
- **Returns**: `number` - Maximum possible compressed size

### lzmaVersion()

Get LZMA SDK version.

- **Returns**: `string` - Version string (e.g., "23.01")

## Error Handling

```javascript
try {
  const compressed = await compress(data);
} catch (error) {
  if (error.message.includes('buffer too small')) {
    // Increase buffer size
  } else if (error.message.includes('invalid format')) {
    // Check input data format
  } else {
    console.error('Compression failed:', error);
  }
}
```

## Platform Support

- ✅ macOS (x64, ARM64)
- ✅ Linux (x64, ARM64, MUSL)
- ✅ Windows (x64, ia32, ARM64)
- ✅ FreeBSD (experimental)

## Version Info

```javascript
const { lzmaVersion } = require('@sevenzip/napi');

console.log('LZMA SDK:', lzmaVersion()); // "23.01"
```

## Next Steps

- See [PUBLISHING.md](./PUBLISHING.md) for publishing guide
- Check [examples/](./examples/) for more code samples
- Read [README.md](./README.md) for detailed documentation

## Support

- GitHub Issues: Report bugs or request features
- npm: https://www.npmjs.com/package/@sevenzip/napi
- LZMA SDK: https://www.7-zip.org/sdk.html

---

**Happy compressing!** 🗜️
