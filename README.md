# Pingo
A project to ping every single IPv4 address to experiment with basic networking in Linux. (ICMP, ip headers, raw sockets, etc)

## Dependencies
This project attempts to be mostly POSIX compliant but with a few Linux specific assumptions for quick implementation.
### Build Tools
- CMake
- CMake compatible C++ compiler
### Libraries
- POSIX Threads (pthread)
- OpenSSL (libcrypto)
- PNG (libpng)
### Permissions
By default, most Linux distros do not allow processes to open raw sockets without elevated privileges.  This means Pingo will not be able to send ping requests unless run as root or is allowed the CAP_NET_RAW capability.  If only processing previously collected data, this is capability is not required since Pingo will not open any socket.

Linux command to give Pingo CAP_NET_RAW capability: `sudo setcap cap_net_raw+ep pingo`.

## Building
Pingo may be built using the following commands:
```
mkdir build
cd build
cmake ..
make
```

## Usage
The basic usage of Pingo is with the command `pingo [OPTION]...`.  

If no options are provided, Pingo will start pinging every IP address starting with address 0.0.0.0 or at the first IP not pinged in the Pingo data saved in the current working directory.  New data will be saved to the working directory.  This initial IP address may be be specified with the option `-i <###.###.###.###>` and the directory to read and write data may be specified with the option `-d <directory>`.  

The dataset may be validated for corruption and completeness with the option `-v` and a Hilbert Curve PNG of the collected data may be generated with the option `-H <Hilbert Curve order>`.  

Running Pingo with the option `-h` will output a complete list of options.