/*!
 * 7z compression Node.js bindings via NAPI-RS
 * 
 * Provides zero-copy compression/decompression for Node.js and SolidJS
 */

#![deny(clippy::all)]

use napi::bindgen_prelude::*;
use napi_derive::napi;

/// FFI bindings to C library
mod ffi {
    use std::os::raw::{c_char, c_int, c_void};
    
    #[repr(C)]
    pub struct SevenZipErrorCode(pub c_int);
    
    impl SevenZipErrorCode {
        pub const OK: Self = Self(0);
        pub const INVALID_PARAM: Self = Self(6);
        pub const COMPRESS: Self = Self(5);
        pub const EXTRACT: Self = Self(4);
    }
    
    extern "C" {
        pub fn sevenzip_compress_buffer(
            input: *const u8,
            input_size: usize,
            output: *mut u8,
            output_size: *mut usize,
            level: c_int,
        ) -> SevenZipErrorCode;
        
        pub fn sevenzip_decompress_buffer(
            input: *const u8,
            input_size: usize,
            output: *mut u8,
            output_size: *mut usize,
        ) -> SevenZipErrorCode;
        
        pub fn sevenzip_compress_bound(input_size: usize) -> usize;
        
        pub fn sevenzip_compress_xz(
            input: *const u8,
            input_size: usize,
            output: *mut u8,
            output_size: *mut usize,
            level: c_int,
        ) -> SevenZipErrorCode;
        
        pub fn sevenzip_decompress_xz(
            input: *const u8,
            input_size: usize,
            output: *mut u8,
            output_size: *mut usize,
        ) -> SevenZipErrorCode;
        
        pub fn sevenzip_lzma_version() -> *const c_char;
        
        pub fn sevenzip_compress(
            input_path: *const c_char,
            output_path: *const c_char,
            level: c_int,
            password: *const c_char,
        ) -> SevenZipErrorCode;
        
        pub fn sevenzip_decompress(
            archive_path: *const c_char,
            output_dir: *const c_char,
            password: *const c_char,
        ) -> SevenZipErrorCode;
    }
}

/// Compression options
#[napi(object)]
pub struct CompressOptions {
    /// Compression level (0-9, default: 5)
    pub level: Option<i32>,
    /// Use XZ format instead of 7z (lighter, better for streaming)
    pub use_xz: Option<bool>,
}

/// Compress data buffer
/// 
/// # Examples
/// 
/// ```typescript
/// import { compress } from '@sevenzip/napi';
/// 
/// const data = Buffer.from('Hello, World!');
/// const compressed = await compress(data, { level: 9 });
/// console.log(`Compressed from ${data.length} to ${compressed.length} bytes`);
/// ```
#[napi]
pub async fn compress(data: Buffer, options: Option<CompressOptions>) -> Result<Buffer> {
    let opts = options.unwrap_or(CompressOptions {
        level: Some(5),
        use_xz: Some(false),
    });
    
    let level = opts.level.unwrap_or(5);
    let use_xz = opts.use_xz.unwrap_or(false);
    
    // Run compression in tokio thread pool to not block Node.js event loop
    tokio::task::spawn_blocking(move || {
        let input = data.as_ref();
        let max_size = unsafe { ffi::sevenzip_compress_bound(input.len()) };
        let mut output = vec![0u8; max_size];
        let mut output_size = max_size;
        
        let result = if use_xz {
            unsafe {
                ffi::sevenzip_compress_xz(
                    input.as_ptr(),
                    input.len(),
                    output.as_mut_ptr(),
                    &mut output_size,
                    level,
                )
            }
        } else {
            unsafe {
                ffi::sevenzip_compress_buffer(
                    input.as_ptr(),
                    input.len(),
                    output.as_mut_ptr(),
                    &mut output_size,
                    level,
                )
            }
        };
        
        if result.0 != 0 {
            return Err(Error::from_reason("Compression failed"));
        }
        
        output.truncate(output_size);
        Ok(Buffer::from(output))
    })
    .await
    .map_err(|e| Error::from_reason(format!("Task failed: {}", e)))?
}

/// Decompress data buffer
/// 
/// # Examples
/// 
/// ```typescript
/// import { decompress } from '@sevenzip/napi';
/// 
/// const compressed = Buffer.from(/* ... */);
/// const decompressed = await decompress(compressed, 1024 * 1024); // 1MB max
/// console.log(decompressed.toString());
/// ```
#[napi]
pub async fn decompress(data: Buffer, max_size: Option<u32>) -> Result<Buffer> {
    let max_output_size = max_size.unwrap_or(10 * 1024 * 1024) as usize; // 10MB default
    
    tokio::task::spawn_blocking(move || {
        let input = data.as_ref();
        let mut output = vec![0u8; max_output_size];
        let mut output_size = max_output_size;
        
        // Try XZ first, then LZMA
        let result = unsafe {
            ffi::sevenzip_decompress_xz(
                input.as_ptr(),
                input.len(),
                output.as_mut_ptr(),
                &mut output_size,
            )
        };
        
        let result = if result.0 != 0 {
            output_size = max_output_size;
            unsafe {
                ffi::sevenzip_decompress_buffer(
                    input.as_ptr(),
                    input.len(),
                    output.as_mut_ptr(),
                    &mut output_size,
                )
            }
        } else {
            result
        };
        
        if result.0 != 0 {
            return Err(Error::from_reason("Decompression failed"));
        }
        
        output.truncate(output_size);
        Ok(Buffer::from(output))
    })
    .await
    .map_err(|e| Error::from_reason(format!("Task failed: {}", e)))?
}

/// Compress file to 7z archive
/// 
/// # Examples
/// 
/// ```typescript
/// import { compressFile } from '@sevenzip/napi';
/// 
/// await compressFile('input.txt', 'output.7z', { level: 9 });
/// ```
#[napi]
pub async fn compress_file(
    input_path: String,
    output_path: String,
    options: Option<CompressOptions>,
) -> Result<()> {
    let level = options.and_then(|o| o.level).unwrap_or(5);
    
    tokio::task::spawn_blocking(move || {
        use std::ffi::CString;
        
        let input_cstr = CString::new(input_path).map_err(|_| Error::from_reason("Invalid input path"))?;
        let output_cstr = CString::new(output_path).map_err(|_| Error::from_reason("Invalid output path"))?;
        
        let result = unsafe {
            ffi::sevenzip_compress(
                input_cstr.as_ptr(),
                output_cstr.as_ptr(),
                level,
                std::ptr::null(),
            )
        };
        
        if result.0 != 0 {
            return Err(Error::from_reason("File compression failed"));
        }
        
        Ok(())
    })
    .await
    .map_err(|e| Error::from_reason(format!("Task failed: {}", e)))?
}

/// Extract 7z archive to directory
/// 
/// # Examples
/// 
/// ```typescript
/// import { extractFile } from '@sevenzip/napi';
/// 
/// await extractFile('archive.7z', './output');
/// ```
#[napi]
pub async fn extract_file(archive_path: String, output_dir: String) -> Result<()> {
    tokio::task::spawn_blocking(move || {
        use std::ffi::CString;
        
        let archive_cstr = CString::new(archive_path).map_err(|_| Error::from_reason("Invalid archive path"))?;
        let output_cstr = CString::new(output_dir).map_err(|_| Error::from_reason("Invalid output dir"))?;
        
        let result = unsafe {
            ffi::sevenzip_decompress(
                archive_cstr.as_ptr(),
                output_cstr.as_ptr(),
                std::ptr::null(),
            )
        };
        
        if result.0 != 0 {
            return Err(Error::from_reason("File extraction failed"));
        }
        
        Ok(())
    })
    .await
    .map_err(|e| Error::from_reason(format!("Task failed: {}", e)))?
}

/// Get LZMA SDK version
#[napi]
pub fn lzma_version() -> String {
    unsafe {
        let ptr = ffi::sevenzip_lzma_version();
        std::ffi::CStr::from_ptr(ptr)
            .to_string_lossy()
            .into_owned()
    }
}

/// Calculate maximum compressed size for buffer allocation
#[napi]
pub fn compress_bound(input_size: u32) -> u32 {
    unsafe { ffi::sevenzip_compress_bound(input_size as usize) as u32 }
}
