# grpc-test
Build
-----
make -j [jobs]

Run
---
main this_host [hosts...]  
example:  
for host 0: ./main 0 172.22.0.2 172.22.0.3  
for host 1: ./main 1 172.22.0.2 172.22.0.3  

A recipe for reproducing the error.  
The host 0 (left) doesn't receive the request from host 1 at iteration 133.
![screenshot](https://raw.githubusercontent.com/CaryLorrk/grpc-test/master/screenshot.png)
