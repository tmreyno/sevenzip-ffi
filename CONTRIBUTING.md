# Contributing to 7z FFI SDK

Thank you for your interest in contributing to the 7z FFI SDK! This document provides guidelines and instructions for contributing to the project.

## 🎯 How Can I Contribute?

### 1. Report Bugs

Found a bug? Help us fix it!

**Before submitting:**
- Check if the issue already exists
- Verify it's reproducible
- Collect relevant information

**What to include:**
- Clear description of the problem
- Steps to reproduce
- Expected vs actual behavior
- Platform (OS, architecture)
- Version of the library
- Minimal code example if possible
- Error messages or logs

### 2. Suggest Features

Have an idea for improvement?

**Before suggesting:**
- Check existing feature requests
- Review the roadmap (ROADMAP.md)
- Consider if it fits the project scope

**What to include:**
- Clear use case
- Expected behavior
- Why it's useful
- Potential implementation approach
- Breaking changes considerations

### 3. Submit Code

Ready to write code?

**Good first issues:**
- Documentation improvements
- Additional examples
- Test coverage
- Bug fixes with clear reproduction steps

**Larger contributions:**
- Compression implementation
- Cross-platform testing
- Performance optimizations
- New features from TODO.md

## 🛠️ Development Setup

### Prerequisites

```bash
# macOS
brew install cmake p7zip

# Ubuntu/Debian
sudo apt-get install cmake p7zip-full build-essential

# Fedora/RHEL
sudo dnf install cmake p7zip gcc-c++

# Windows
# Install CMake from cmake.org
# Install Visual Studio with C++ tools
# Install 7-Zip
```

### Getting Started

```bash
# Clone the repository
git clone <repository-url>
cd sevenzip-ffi

# Download LZMA SDK
./setup_lzma.sh

# Build
mkdir build && cd build
cmake ..
cmake --build .

# Run tests
./examples/example_list ../test_archive.7z
./examples/example_extract ../test_archive.7z test_output
```

## 📝 Coding Standards

### C Code Style

```c
// Use descriptive names
SevenZipErrorCode sevenzip_extract(const char* archive_path, ...);

// Comments for complex logic
/* Calculate buffer size based on archive format */
size_t buffer_size = calculate_optimal_buffer(archive_type);

// Error handling
if (!archive_path) {
    return SEVENZIP_ERROR_INVALID_PARAM;
}

// Memory management - always free what you allocate
char* buffer = malloc(size);
if (!buffer) {
    return SEVENZIP_ERROR_MEMORY;
}
// ... use buffer ...
free(buffer);
```

### Code Guidelines

1. **FFI-Friendly API**
   - Use simple C types (no C++ classes in public API)
   - All functions return error codes
   - Use out-parameters for complex returns
   - Provide cleanup functions

2. **Memory Management**
   - Caller owns allocated memory unless documented otherwise
   - Provide free functions for complex structures
   - No memory leaks (verify with valgrind)
   - Handle allocation failures gracefully

3. **Error Handling**
   - Always check return values
   - Provide meaningful error codes
   - Include error messages
   - Clean up on error paths

4. **Platform Compatibility**
   - Use CMake for build configuration
   - Avoid platform-specific code when possible
   - Use `#ifdef` for platform differences
   - Test on multiple platforms

5. **Documentation**
   - Document all public functions in header
   - Include usage examples
   - Document limitations and edge cases
   - Keep README and guides updated

### Naming Conventions

```c
// Functions: sevenzip_action_name
SevenZipErrorCode sevenzip_extract(...);
SevenZipErrorCode sevenzip_list(...);

// Types: SevenZipTypeName
typedef struct SevenZipEntry { ... } SevenZipEntry;
typedef enum SevenZipErrorCode { ... } SevenZipErrorCode;

// Constants: SEVENZIP_CONSTANT_NAME
#define SEVENZIP_VERSION "1.0.0"
#define SEVENZIP_MAX_PATH 4096

// Internal functions: prefix with underscore
static int _internal_helper_function(...);
```

## 🔄 Pull Request Process

### Before Submitting

1. **Create an issue first** (for significant changes)
2. **Fork the repository**
3. **Create a feature branch**
   ```bash
   git checkout -b feature/your-feature-name
   ```
4. **Make your changes**
5. **Test thoroughly**
6. **Update documentation**
7. **Commit with clear messages**

### Commit Messages

Format:
```
[Type] Brief description (50 chars or less)

More detailed explanation if needed. Wrap at 72 characters.
Explain what and why, not how.

Fixes #123
```

Types:
- `[Feature]` - New functionality
- `[Fix]` - Bug fixes
- `[Docs]` - Documentation changes
- `[Test]` - Test additions or changes
- `[Refactor]` - Code restructuring
- `[Build]` - Build system changes
- `[Perf]` - Performance improvements

Examples:
```
[Feature] Add selective file extraction support

Implements selective extraction allowing users to extract
specific files from archives by name or pattern.

Fixes #45
```

```
[Fix] Correct memory leak in list function

Fixed leak in sevenzip_list when allocation fails
for entry names. Now properly frees partial allocations
on error paths.

Fixes #78
```

### Pull Request Checklist

- [ ] Code follows project style guidelines
- [ ] Comments added for complex logic
- [ ] Documentation updated (if applicable)
- [ ] Examples added/updated (if applicable)
- [ ] All existing tests pass
- [ ] New tests added for new functionality
- [ ] No memory leaks (verified with valgrind if possible)
- [ ] Builds on at least one platform
- [ ] Commit messages are clear and descriptive

### Review Process

1. **Automated checks** run (when CI is set up)
2. **Maintainer review** (usually within 3-5 days)
3. **Feedback addressed** by contributor
4. **Final approval** and merge

## 🧪 Testing

### Running Tests

```bash
# Build with examples
cd build
cmake --build .

# Run manual tests
./examples/example_list ../test_archive.7z
./examples/example_extract ../test_archive.7z output/

# Run demo
cd ..
./demo.sh
```

### Adding Tests

When adding features, include:
- Unit tests (when we have test framework)
- Integration examples
- Edge case handling
- Error condition testing

### Test Coverage

We aim for:
- 90%+ code coverage
- All public API functions tested
- Error paths validated
- Platform-specific code tested on relevant platforms

## 📚 Documentation

### Types of Documentation

1. **Code Comments**
   - Explain why, not what
   - Document assumptions
   - Note limitations

2. **API Documentation**
   - Function parameters
   - Return values
   - Error conditions
   - Usage examples

3. **User Guides**
   - QUICKSTART.md
   - Integration guides
   - Tutorials

4. **Developer Docs**
   - Architecture decisions
   - Implementation notes
   - TODO and roadmap

### Documentation Standards

- Clear and concise
- Include code examples
- Keep up-to-date with code
- Verify examples actually work

## 🏗️ Architecture Guidelines

### Project Structure

```
sevenzip-ffi/
├── include/          # Public headers only
├── src/              # Implementation files
├── lzma/             # LZMA SDK (external)
├── examples/         # Example programs
├── tests/            # Test files (future)
└── docs/             # Additional documentation
```

### Adding New Features

1. **Design the API** (discuss with maintainers first)
2. **Update header file** (include/7z_ffi.h)
3. **Implement functionality** (src/)
4. **Add example** (examples/)
5. **Document** (comments, README updates)
6. **Test** (manual and automated)

### Compatibility

- Maintain FFI compatibility
- Avoid breaking changes in minor versions
- Use semantic versioning
- Deprecate before removing

## 🤝 Community

### Communication

- **Issues**: Bug reports, feature requests
- **Pull Requests**: Code contributions
- **Discussions**: General questions, ideas
- **Email**: For security issues

### Code of Conduct

- Be respectful and inclusive
- Assume good intent
- Provide constructive feedback
- Help others learn

### Recognition

- All contributors listed in CONTRIBUTORS.md
- Significant contributions acknowledged in releases
- Community showcase for projects using the library

## 🎯 Priority Areas

Currently looking for help with:

1. **Additional Format Support**
   - ZIP, TAR, XZ format support
   - Format auto-detection

2. **Cross-Platform Testing**
   - Windows builds and testing
   - Linux distribution testing
   - Platform-specific fixes

3. **Documentation**
   - More usage examples
   - Video tutorials
   - Best practices guide

4. **Performance**
   - Profiling and optimization
   - Memory usage improvements

## 📋 Checklist for Maintainers

When reviewing PRs:

- [ ] Code quality and style
- [ ] API design consistency
- [ ] Documentation completeness
- [ ] Test coverage
- [ ] Performance implications
- [ ] Breaking change assessment
- [ ] Security considerations
- [ ] Platform compatibility

## 🙏 Thank You!

Every contribution matters, whether it's:
- Fixing a typo
- Reporting a bug
- Adding a feature
- Improving documentation
- Helping other users

Thank you for helping make 7z FFI SDK better!

## 📄 License

By contributing to this project, you agree that your contributions will be licensed under the same terms as the project (see LICENSE file).

---

**Questions?** Open an issue with the "question" label or start a discussion.

**Ready to contribute?** Check out the [good first issues](https://github.com/tmreyno/sevenzip-ffi/labels/good%20first%20issue)!
