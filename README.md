# grpc-test
A recipe for reproducing the error.  

Build
-----
make -j [jobs]

Run
---
main this_host [hosts...]  
example:  
for host 0: ./main 0 172.22.0.2 172.22.0.3  
for host 1: ./main 1 172.22.0.2 172.22.0.3  

The host 1 (right) doesn't receive the request from host 0 at iteration 342.
![screenshot](https://raw.githubusercontent.com/CaryLorrk/grpc-test/master/screenshot.png)
