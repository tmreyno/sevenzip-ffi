use std::path::PathBuf;

fn main() {
    // Link to the 7z_ffi library built by CMake
    let project_root = PathBuf::from(env!("CARGO_MANIFEST_DIR")).parent().unwrap().to_path_buf();
    let build_dir = project_root.join("build");
    
    println!("cargo:rustc-link-search=native={}", build_dir.display());
    println!("cargo:rustc-link-lib=static=7z_ffi");
    
    // Tell cargo to rerun if C library changes
    println!("cargo:rerun-if-changed=../include/7z_ffi.h");
    println!("cargo:rerun-if-changed=../build/lib7z_ffi.a");
    
    // Link C++ standard library (needed for LZMA SDK)
    #[cfg(target_os = "macos")]
    {
        println!("cargo:rustc-link-lib=dylib=c++");
        println!("cargo:rustc-link-lib=framework=CoreFoundation");
    }
    
    #[cfg(target_os = "linux")]
    println!("cargo:rustc-link-lib=dylib=stdc++");
    
    #[cfg(target_os = "windows")]
    println!("cargo:rustc-link-lib=dylib=msvcrt");
    
    // Tell napi-rs we're linking a static library
    napi_build::setup();
}
