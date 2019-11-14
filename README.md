# ftp_server

The binary of this project is deployed at [ftp://ftp.x-studio.net/](ftp://ftp.x-studio.net/)

### build and run
#### build
1. Goto the parent project's root directory, the parent project is: [yasio](https://github.com/simdsoft/yasio)  
2. mkdir build  
3. cmake ..  
4. cmake --build . --config Release --target ftp_server  
#### run
5. cd examples/ftp_server  
6. ./ftp_server <path-to-wwwroot> <wan_ip>[optional]  
  
### references:  
ftp://ftp.gnu.org/  
https://tools.ietf.org/html/rfc959  

### pitfall:  
Some client's ip may block by server firewall strategy, please switch to mobile cellular network and try access the ftp again.
