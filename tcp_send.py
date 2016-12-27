#!/usr/bin/env python
#coding: utf-8

import socket

host="raspberrypimojamoja.local"
port = 5000

client = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
client.connect((host,port))
client.send("0")