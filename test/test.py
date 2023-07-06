from __future__ import print_function, unicode_literals

import os
import re
import socket
import sys
from time import sleep

# -----------  Config  ----------
PORT = 80
# -------------------------------

barcode = ["5449000004864", "6944284667082", "2000000059990" ,"2000000333885" ,"2000000059969" ,"2000000060019" ,"2000000060002" ,"2000000059303"]


def test():
    ip_address = input("Enter IP: ")
    test_count = 0

    for res in socket.getaddrinfo(ip_address, PORT, socket.AF_UNSPEC,
                                  socket.SOCK_STREAM, 0, socket.AI_PASSIVE):
        family_addr, socktype, proto, canonname, addr = res
    try:
        sock = socket.socket(family_addr, socket.SOCK_STREAM)
        sock.settimeout(60.0)
    except socket.error as msg:
        print('Could not create socket: ' + str(msg[0]) + ': ' + msg[1])
        raise
    try:
        sock.connect(addr)
    except socket.error as msg:
        print('Could not open socket: ', msg)
        sock.close()
        raise


    print("stand_start") 
    sock.sendall("stand_start\r\n".encode())
    sleep(8)

    for code in barcode:
        print("barcode "+code) 
        sock.sendall( ("barcode " + code + '\r\n').encode())
        scan_code = input()
        if scan_code == code:
            test_count += 1
        sleep(1)
    print("Test end, suc:", test_count)
    print("stand_end") 
    sock.sendall( "stand_end\r\n".encode())
    sleep(5)
    sock.close()


if __name__ == '__main__':
    test()
    input("Press Enter to continue...")