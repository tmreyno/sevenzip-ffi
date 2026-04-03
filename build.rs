use std::env;
use std::path::PathBuf;

fn main() {
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let build_dir = manifest_dir.join("build");

    // Prefer the static library path for the current platform.
    let static_lib = if cfg!(target_os = "windows") {
        let copied_lib = build_dir.join("7z_ffi.lib");
        let release_lib = build_dir.join("Release").join("7z_ffi.lib");
        if copied_lib.exists() {
            Some(copied_lib)
        } else if release_lib.exists() {
            Some(release_lib)
        } else {
            None
        }
    } else {
        let archive_lib = build_dir.join("lib7z_ffi.a");
        archive_lib.exists().then_some(archive_lib)
    };
    
    // Check if we should use static linking (default)
    let use_static = cfg!(feature = "static") || !cfg!(feature = "dynamic");

    if use_static {
        if let Some(lib_path) = static_lib.as_ref() {
            if let Some(lib_dir) = lib_path.parent() {
                println!("cargo:rustc-link-search=native={}", lib_dir.display());
            }
        println!("cargo:rustc-link-lib=static=7z_ffi");

        // Link C++ standard library for LZMA SDK
        #[cfg(target_os = "macos")]
        {
            println!("cargo:rustc-link-lib=dylib=c++");
            println!("cargo:rustc-link-lib=framework=CoreFoundation");
        }
        
        #[cfg(target_os = "linux")]
        {
            println!("cargo:rustc-link-lib=dylib=stdc++");
            println!("cargo:rustc-link-lib=dylib=pthread");
        }
        
        #[cfg(target_os = "windows")]
        {
            println!("cargo:rustc-link-lib=bcrypt");
            println!("cargo:rustc-link-lib=dylib=user32");
            println!("cargo:rustc-link-lib=dylib=ole32");
        }
        } else {
            println!("cargo:rustc-link-search=native={}", build_dir.display());
            // Fall back to dynamic linking
            println!("cargo:rustc-link-lib=dylib=7z_ffi");
        }
    } else {
        println!("cargo:rustc-link-search=native={}", build_dir.display());
        // Fall back to dynamic linking
        println!("cargo:rustc-link-lib=dylib=7z_ffi");
    }

    // Tell cargo to invalidate the built crate whenever the C library changes
    println!("cargo:rerun-if-changed=src/");
    println!("cargo:rerun-if-changed=include/");
    println!("cargo:rerun-if-changed=CMakeLists.txt");
    println!("cargo:rerun-if-changed=build/lib7z_ffi.a");
    println!("cargo:rerun-if-changed=build/7z_ffi.lib");
    println!("cargo:rerun-if-changed=build/Release/7z_ffi.lib");
}
