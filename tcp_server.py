#!/usr/bin/env python
# coding:utf-8

import asyncore
import socket
import sys


class TCPServer(asyncore.dispatcher):
    def __init__(self):
        asyncore.dispatcher.__init__(self)
        self.create_socket(socket.AF_INET, socket.SOCK_STREAM)
        self.set_reuse_addr()
        self.bind(("", 5000))
        self.listen(5)

    def handle_accept(self):
        pair = self.accept()
        if pair is not None:
            sock, addr = pair
            data = sock.recv(4095)
            self.servo(data)

            if data == "quit":
                raise asyncore.ExitNow("Stop Server")

    def servo(self, degree):
        try:
            with open("/dev/pwmservo0", "w") as f:
                f.write(degree)
        except:
            sys.stderr.write("except pwmservo0\n")


if __name__ == '__main__':
    server = TCPServer()
    try:
        asyncore.loop()
    except asyncore.ExitNow, e:
        print e
