A TFTP server program

How to build:
cd TFTP

make

How to use:
sudo ./server <port num>

Example:

1. run TFTP server

sudo ./server 69

2. open a new shell to run TFTP client (mac) locally and retrieve file from the server

tftp 127.0.0.1

tftp> get test

Received 10 bytes in 0.0 seconds

tftp> get server

Received 13832 bytes in 0.0 seconds
