# DARE
Automated registration tool for Foothill and De Anza College.

## Features
- Automatic course registration and waitlisting
- Real-time monitoring for seats in filled courses
- Option to specify backup CRNs in case courses are full
- Option to automatically drop a course when another course opens
- Support for asynchronous instances
- Discord notifications for registration updates and errors

## Download
[Here](https://github.com/platterss/dare/releases/latest) is the link to the latest release.

Prebuilt packages are available for Windows, macOS, and Linux, for both x64 and ARM64 architectures. 
These are pulled from [GitHub Actions](https://github.com/platterss/dare/actions).

As these binaries are not signed, it is likely you will have to [add an exception on Windows Defender](https://support.microsoft.com/en-us/windows/virus-and-threat-protection-in-the-windows-security-app-1362f4cd-d71a-b52a-0b66-c2820032b65e)
or [allow the app to run on macOS](https://support.apple.com/en-us/102445#openanyway) (both require admin rights).
This is unavoidable unless you build the program yourself.

## Usage
The program can either be run directly or invoked through the command line.

Please read the [wiki](https://github.com/platterss/dare/wiki/Configuration) to see how to properly configure and use the program.

## Build
You don't need to do this if you just want to use the program.
This is just for developers.

### Prerequisites
- CMake 3.21 or higher
- C++ compiler with C++23 support
- vcpkg

### Compiling
Replace `$VCPKG_ROOT` with your vcpkg installation path if it's not already in your environment variables.
```
git clone https://github.com/platterss/dare
cd dare
vcpkg install
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

Make sure a `configs` folder is in the runtime directory.

## Contributing
Contributions are very welcome! 

For any problems or suggestions, feel free to create an [issue](https://github.com/platterss/dare/issues). If you have any fixes or improvements, you can create a [pull request](https://github.com/platterss/dare/pulls).  

Please do not include any sensitive information (CWIDs, passwords, Discord webhooks) in issue reports or pull requests.
