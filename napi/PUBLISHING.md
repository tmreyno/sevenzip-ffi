# Publishing @sevenzip/napi to npm

Complete guide for building and publishing the 7z compression library as an npm package.

## Prerequisites

1. **Required Tools**:
   ```bash
   # Install Rust
   curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
   
   # Install Node.js (v16+)
   # macOS:
   brew install node
   
   # Install NAPI-RS CLI
   npm install -g @napi-rs/cli
   ```

2. **Build C Library First**:
   ```bash
   cd /Users/terryreynolds/GitHub/sevenzip-ffi
   
   # Configure CMake
   mkdir -p build
   cd build
   cmake .. -DCMAKE_BUILD_TYPE=Release
   
   # Build the C library
   cmake --build . --config Release
   
   # Verify lib7z_ffi.a was created
   ls -lh lib7z_ffi.a
   ```

## Local Development Build

```bash
cd /Users/terryreynolds/GitHub/sevenzip-ffi/napi

# Install dependencies
npm install

# Build the native addon
npm run build

# Test the build
node -e "const z = require('.'); console.log('LZMA SDK:', z.lzmaVersion())"

# Run examples
node examples/node-example.js
```

## Multi-Platform Builds

### Option 1: Local Cross-Compilation

Build for multiple platforms on your machine:

```bash
# Add Rust targets
rustup target add x86_64-apple-darwin
rustup target add aarch64-apple-darwin
rustup target add x86_64-unknown-linux-gnu
rustup target add aarch64-unknown-linux-gnu

# Build for each platform
npm run build -- --target x86_64-apple-darwin
npm run build -- --target aarch64-apple-darwin
npm run build -- --target x86_64-unknown-linux-gnu

# Create artifacts
npm run artifacts
```

### Option 2: GitHub Actions (Recommended)

Create `.github/workflows/publish.yml`:

```yaml
name: Build and Publish

on:
  push:
    tags:
      - 'v*'
  workflow_dispatch:

jobs:
  build:
    strategy:
      matrix:
        settings:
          - host: macos-latest
            target: x86_64-apple-darwin
            build: npm run build -- --target x86_64-apple-darwin
          - host: macos-latest
            target: aarch64-apple-darwin
            build: npm run build -- --target aarch64-apple-darwin
          - host: ubuntu-latest
            target: x86_64-unknown-linux-gnu
            build: npm run build -- --target x86_64-unknown-linux-gnu
          - host: ubuntu-latest
            target: aarch64-unknown-linux-gnu
            build: |
              sudo apt-get update
              sudo apt-get install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
              npm run build -- --target aarch64-unknown-linux-gnu
          - host: ubuntu-latest
            target: x86_64-unknown-linux-musl
            build: npm run build -- --target x86_64-unknown-linux-musl
          - host: windows-latest
            target: x86_64-pc-windows-msvc
            build: npm run build -- --target x86_64-pc-windows-msvc
    
    runs-on: ${{ matrix.settings.host }}
    
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: true
      
      - name: Setup Node.js
        uses: actions/setup-node@v4
        with:
          node-version: 18
      
      - name: Setup Rust
        uses: dtolnay/rust-toolchain@stable
        with:
          targets: ${{ matrix.settings.target }}
      
      - name: Build C Library
        run: |
          mkdir -p build
          cd build
          cmake .. -DCMAKE_BUILD_TYPE=Release
          cmake --build . --config Release
      
      - name: Install Dependencies
        run: cd napi && npm install
      
      - name: Build Native Addon
        run: cd napi && ${{ matrix.settings.build }}
      
      - name: Upload Artifacts
        uses: actions/upload-artifact@v4
        with:
          name: bindings-${{ matrix.settings.target }}
          path: napi/*.node
  
  publish:
    needs: build
    runs-on: ubuntu-latest
    
    steps:
      - uses: actions/checkout@v4
      
      - name: Setup Node.js
        uses: actions/setup-node@v4
        with:
          node-version: 18
          registry-url: 'https://registry.npmjs.org'
      
      - name: Download All Artifacts
        uses: actions/download-artifact@v4
        with:
          path: napi/artifacts
      
      - name: Publish to npm
        run: |
          cd napi
          npm publish --access public
        env:
          NODE_AUTH_TOKEN: ${{ secrets.NPM_TOKEN }}
      
      - name: Publish Platform Packages
        run: |
          cd napi
          npm run artifacts
          for pkg in npm/*; do
            cd "$pkg"
            npm publish --access public
            cd ../..
          done
        env:
          NODE_AUTH_TOKEN: ${{ secrets.NPM_TOKEN }}
```

## Publishing Steps

### 1. Prepare for Release

```bash
cd /Users/terryreynolds/GitHub/sevenzip-ffi/napi

# Update version
npm version patch  # or minor, major

# Test build
npm run build

# Run tests
npm test  # (once tests are implemented)

# Run examples
node examples/node-example.js
```

### 2. Build Platform Binaries

```bash
# Option A: Use GitHub Actions (push tag)
git tag v1.0.0
git push origin v1.0.0

# Option B: Build locally for current platform
npm run build
npm run artifacts
```

### 3. Publish to npm

```bash
# Login to npm
npm login

# Dry run to verify package contents
npm publish --dry-run

# Publish main package
npm publish --access public

# Publish platform-specific packages (if using artifacts)
cd npm/@sevenzip/napi-darwin-x64
npm publish --access public

cd ../napi-darwin-arm64
npm publish --access public

# ... repeat for other platforms
```

## Package Structure

After publishing, your npm registry will have:

- `@sevenzip/napi` (main package)
  - Contains `index.js`, `index.d.ts`
  - Optional dependencies for each platform
  
- `@sevenzip/napi-darwin-x64`
- `@sevenzip/napi-darwin-arm64`
- `@sevenzip/napi-linux-x64-gnu`
- `@sevenzip/napi-linux-x64-musl`
- `@sevenzip/napi-linux-arm64-gnu`
- `@sevenzip/napi-win32-x64-msvc`

## Installation for Users

Users install the main package, and npm automatically installs the correct platform binary:

```bash
npm install @sevenzip/napi
```

The package automatically detects the platform and loads the correct native addon.

## Usage in Projects

### Node.js

```javascript
const { compress, decompress, lzmaVersion } = require('@sevenzip/napi');

console.log('LZMA SDK:', lzmaVersion());

const data = Buffer.from('Hello World');
const compressed = await compress(data, { level: 9 });
const decompressed = await decompress(compressed);
```

### TypeScript

```typescript
import { compress, decompress, CompressOptions } from '@sevenzip/napi';

const options: CompressOptions = { level: 9, useXz: false };
const compressed = await compress(Buffer.from('Hello'), options);
```

### SolidJS/Vite

```javascript
import { compress, decompress } from '@sevenzip/napi';

// Use in server-side code only (Node.js)
// For browser, you'll need WebAssembly version
```

## Troubleshooting

### Build Errors

1. **Missing lib7z_ffi.a**:
   ```bash
   cd .. && mkdir -p build && cd build
   cmake .. && cmake --build .
   ```

2. **Rust linking errors**:
   ```bash
   # macOS: Install Xcode Command Line Tools
   xcode-select --install
   
   # Linux: Install build essentials
   sudo apt-get install build-essential
   ```

3. **NAPI version mismatch**:
   ```bash
   rm -rf node_modules package-lock.json
   npm install
   ```

### Platform-Specific Issues

**macOS**: Ensure Xcode and libc++ are available
**Linux**: May need libstdc++ or musl depending on target
**Windows**: Requires Visual Studio Build Tools

## Version Management

Follow semantic versioning:

- **Patch** (1.0.x): Bug fixes, performance improvements
- **Minor** (1.x.0): New features, backward compatible
- **Major** (x.0.0): Breaking changes

```bash
npm version patch  # 1.0.0 -> 1.0.1
npm version minor  # 1.0.1 -> 1.1.0
npm version major  # 1.1.0 -> 2.0.0
```

## Testing Before Publishing

```bash
# Local testing
npm pack
npm install -g sevenzip-napi-1.0.0.tgz

# Test in another project
mkdir test-project
cd test-project
npm init -y
npm install ../napi/sevenzip-napi-1.0.0.tgz
node -e "console.log(require('@sevenzip/napi').lzmaVersion())"
```

## Post-Publishing

1. **Verify on npm**: https://www.npmjs.com/package/@sevenzip/napi
2. **Update README**: Add installation instructions
3. **Tag GitHub release**: Match npm version
4. **Update CHANGELOG**: Document changes

## CI/CD Integration

Add to `package.json`:

```json
{
  "scripts": {
    "prepublishOnly": "npm run build && npm test",
    "postpublish": "git tag v$npm_package_version && git push --tags"
  }
}
```

## Support Matrix

| Platform | Architecture | Node Version | Status |
|----------|-------------|--------------|--------|
| macOS    | x64         | 16+          | ✅ Tested |
| macOS    | ARM64       | 16+          | ✅ Tested |
| Linux    | x64 (GNU)   | 16+          | ✅ Tested |
| Linux    | x64 (MUSL)  | 16+          | ✅ Tested |
| Linux    | ARM64       | 16+          | ⚠️ Not tested |
| Windows  | x64         | 16+          | ⚠️ Not tested |
| FreeBSD  | x64         | 16+          | ❌ Experimental |

---

**Ready to publish!** 🚀

For issues or questions, see:
- GitHub: https://github.com/terryreynolds/sevenzip-ffi
- npm: https://www.npmjs.com/package/@sevenzip/napi
