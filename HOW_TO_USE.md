# How to Use sevenzip-ffi NAPI Bindings

## 🚀 Quick Start (30 seconds)

### 1. Load the Module

```javascript
// Option 1: Direct load (for development)
const z = require('./napi/sevenzip-napi.node');

// Option 2: After npm install (once published)
// const z = require('@sevenzip/napi');
```

### 2. Compress Data

```javascript
const input = Buffer.from('Hello, World!');
const compressed = await z.compress(input, { level: 5 });
console.log(`${input.length} → ${compressed.length} bytes`);
```

### 3. Decompress Data

```javascript
const decompressed = await z.decompress(compressed);
console.log(decompressed.toString()); // "Hello, World!"
```

**That's it!** 🎉

---

## 📖 Complete API Reference

### `lzmaVersion()` → string

Get LZMA SDK version.

```javascript
console.log(z.lzmaVersion()); // "23.01"
```

### `compressBound(inputSize)` → number

Calculate maximum compressed size.

```javascript
const maxSize = z.compressBound(1024);
console.log(maxSize); // ~1500 bytes
```

### `compress(buffer, options?)` → Promise<Buffer>

Compress a buffer asynchronously.

**Parameters:**
- `buffer` - Buffer to compress
- `options` (optional)
  - `level` (0-9, default: 5) - Compression level
  - `useXz` (boolean, default: false) - Use XZ format

```javascript
// Simple compression
const compressed = await z.compress(data);

// With options
const compressed = await z.compress(data, { 
  level: 9,     // Best compression
  useXz: false  // Use LZMA format
});

// XZ format
const compressed = await z.compress(data, { 
  level: 6, 
  useXz: true 
});
```

**Compression Levels:**
- `0` - No compression (fastest)
- `1-3` - Fast compression
- `5` - **Balanced (recommended)**
- `7-9` - Best compression (slower)

### `decompress(buffer)` → Promise<Buffer>

Decompress a buffer asynchronously. Auto-detects format (LZMA or XZ).

```javascript
const decompressed = await z.decompress(compressed);
```

---

## 💡 Common Use Cases

### 1. Compress API Responses

```javascript
const express = require('express');
const z = require('./napi/sevenzip-napi.node');

app.get('/api/data', async (req, res) => {
  const data = await fetchLargeData();
  const json = JSON.stringify(data);
  
  const compressed = await z.compress(Buffer.from(json), { level: 5 });
  
  res.set('Content-Encoding', '7z');
  res.set('Content-Type', 'application/octet-stream');
  res.send(compressed);
});
```

### 2. Compress Before Storing

```javascript
const fs = require('fs').promises;
const z = require('./napi/sevenzip-napi.node');

async function saveCompressed(filename, data) {
  const buffer = Buffer.from(JSON.stringify(data));
  const compressed = await z.compress(buffer, { level: 9 });
  await fs.writeFile(`${filename}.7z`, compressed);
  
  console.log(`Saved: ${buffer.length} → ${compressed.length} bytes`);
}

async function loadCompressed(filename) {
  const compressed = await fs.readFile(`${filename}.7z`);
  const decompressed = await z.decompress(compressed);
  return JSON.parse(decompressed.toString());
}
```

### 3. Real-time Compression (WebSocket)

```javascript
const WebSocket = require('ws');
const z = require('./napi/sevenzip-napi.node');

const wss = new WebSocket.Server({ port: 8080 });

wss.on('connection', (ws) => {
  ws.on('message', async (data) => {
    // Compress before sending
    const compressed = await z.compress(data, { level: 3 });
    ws.send(compressed);
  });
});
```

### 4. Batch Processing

```javascript
const z = require('./napi/sevenzip-napi.node');

async function compressBatch(items) {
  const results = await Promise.all(
    items.map(item => 
      z.compress(Buffer.from(item), { level: 5 })
    )
  );
  
  return results;
}
```

### 5. Stream-like Processing

```javascript
async function compressLargeData(data) {
  const chunkSize = 1024 * 1024; // 1 MB chunks
  const chunks = [];
  
  for (let i = 0; i < data.length; i += chunkSize) {
    const chunk = data.slice(i, i + chunkSize);
    const compressed = await z.compress(chunk, { level: 5 });
    chunks.push(compressed);
  }
  
  return chunks;
}
```

---

## 🔧 TypeScript Usage

The package includes TypeScript definitions:

```typescript
import { 
  compress, 
  decompress, 
  compressBound,
  lzmaVersion,
  CompressOptions 
} from './napi/sevenzip-napi.node';

// Type-safe options
const options: CompressOptions = {
  level: 9,
  useXz: false
};

// Async with proper types
async function example(data: Buffer): Promise<Buffer> {
  const compressed: Buffer = await compress(data, options);
  const decompressed: Buffer = await decompress(compressed);
  return decompressed;
}

// Get version
const version: string = lzmaVersion();
console.log(version); // "23.01"
```

---

## ⚡ Performance Tips

### 1. Choose the Right Level

```javascript
// Fast compression for real-time
const fast = await z.compress(data, { level: 1 });

// Balanced for general use
const balanced = await z.compress(data, { level: 5 });

// Best for archives
const best = await z.compress(data, { level: 9 });
```

### 2. Pre-allocate Buffers

```javascript
// Calculate max size first
const maxSize = z.compressBound(data.length);
console.log(`Need at most ${maxSize} bytes`);

const compressed = await z.compress(data);
```

### 3. Batch Operations

```javascript
// Process multiple items concurrently
const results = await Promise.all([
  z.compress(buffer1),
  z.compress(buffer2),
  z.compress(buffer3)
]);
```

### 4. Reuse Buffers (Advanced)

```javascript
// For repeated operations, consider buffer pools
const pool = [];

async function compressWithPool(data) {
  const maxSize = z.compressBound(data.length);
  const buffer = pool.pop() || Buffer.allocUnsafe(maxSize);
  
  const compressed = await z.compress(data);
  
  // Return buffer to pool
  if (buffer.length < maxSize * 10) {
    pool.push(buffer);
  }
  
  return compressed;
}
```

---

## 🚨 Error Handling

### Try-Catch Pattern

```javascript
try {
  const compressed = await z.compress(data);
} catch (error) {
  if (error.message.includes('buffer too small')) {
    // Handle insufficient buffer
  } else if (error.message.includes('invalid format')) {
    // Handle invalid input
  } else {
    console.error('Compression failed:', error);
  }
}
```

### Validation

```javascript
function validateInput(buffer) {
  if (!Buffer.isBuffer(buffer)) {
    throw new TypeError('Input must be a Buffer');
  }
  if (buffer.length === 0) {
    throw new Error('Input buffer is empty');
  }
  if (buffer.length > 1024 * 1024 * 100) { // 100 MB
    throw new Error('Input too large');
  }
}

async function safeCompress(data) {
  validateInput(data);
  return await z.compress(data);
}
```

---

## 📦 Integration Examples

### Express.js Middleware

```javascript
function compressionMiddleware(req, res, next) {
  const originalSend = res.send;
  
  res.send = async function(data) {
    if (req.accepts('7z') && data.length > 1024) {
      const compressed = await z.compress(Buffer.from(data), { level: 5 });
      res.set('Content-Encoding', '7z');
      return originalSend.call(this, compressed);
    }
    return originalSend.call(this, data);
  };
  
  next();
}

app.use(compressionMiddleware);
```

### Redis Cache Helper

```javascript
const redis = require('redis');
const z = require('./napi/sevenzip-napi.node');

const client = redis.createClient();

async function cacheSet(key, value, ttl = 3600) {
  const json = JSON.stringify(value);
  const compressed = await z.compress(Buffer.from(json), { level: 9 });
  await client.set(key, compressed, 'EX', ttl);
}

async function cacheGet(key) {
  const compressed = await client.getBuffer(key);
  if (!compressed) return null;
  
  const decompressed = await z.decompress(compressed);
  return JSON.parse(decompressed.toString());
}
```

---

## 🎯 Quick Reference

| Function | Input | Output | Async |
|----------|-------|--------|-------|
| `lzmaVersion()` | - | string | No |
| `compressBound(size)` | number | number | No |
| `compress(buffer, opts?)` | Buffer | Promise<Buffer> | Yes |
| `decompress(buffer)` | Buffer | Promise<Buffer> | Yes |

**Default Compression Level**: 5 (balanced)  
**Supported Formats**: LZMA, XZ (LZMA2)  
**Node.js Version**: 16.0.0 or higher  
**LZMA SDK**: 23.01

---

## 🎓 Examples to Run

1. **Quick Start**:
   ```bash
   cd napi
   node quick-example.js
   ```

2. **Simple Test**:
   ```bash
   node test.js
   ```

3. **Full Examples** (after fixing imports):
   ```bash
   node examples/node-example.js
   ```

---

## 📚 More Resources

- **QUICKSTART.md** - Detailed getting started guide
- **PUBLISHING.md** - How to publish as npm package
- **index.d.ts** - Full TypeScript definitions
- **SUCCESS_REPORT.md** - Test results and benchmarks

---

## ❓ Troubleshooting

### Module not found

```bash
# Make sure you're in the correct directory
cd /Users/terryreynolds/GitHub/sevenzip-ffi/napi

# Check if .node file exists
ls -lh sevenzip-napi.node
```

### Async errors

```javascript
// ✗ Wrong (missing await)
const compressed = z.compress(data);

// ✓ Correct (with await)
const compressed = await z.compress(data);
```

### Buffer errors

```javascript
// ✗ Wrong (string input)
await z.compress('hello');

// ✓ Correct (Buffer input)
await z.compress(Buffer.from('hello'));
```

---

**Ready to compress!** 🗜️ Run `node napi/quick-example.js` to see it in action!
