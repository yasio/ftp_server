# ftp_server

The binary of this project is deployed at <ftp://ftp.yasio.org/>

### build and run
#### build
Goto the parent project's root directory, the parent project is: [yasio](https://github.com/yasio/yasio)  
```sh
mkdir build  
cmake ..  
cmake --build . --config Release --target ftp_server  
```
#### run
Continue run ftp server after build finished  
```sh
cd examples/ftp_server  
./ftp_server <path-to-wwwroot> <wan_ip>[optional]  
```
  
### references:  
<ftp://ftp.gnu.org/>  
https://tools.ietf.org/html/rfc959  

### pitfall:  
Some client's ip may block by server firewall strategy, please switch to mobile cellular network and try access the ftp again.
