#!/usr/bin/env python

import asyncore
import socket
import sys

class UDPServer(asyncore.dispatcher):

	def __init__(self):
		asyncore.dispatcher.__init__(self)
		self.create_socket(socket.AF_INET, socket.SOCK_DGRAM)
		self.bind(('', 5000))

	def handle_connect(self):
		print "Started Server"

	def handle_read(self):
		data, addr = self.recvfrom(4096)

		self.servo(data)

		if data == "quit":
			raise asyncore.ExitNow('Stop Server')


	def handle_write(self):
		pass

	def servo(self, degree):
		try:
			with open("/dev/pwmwrite0", "w") as f:
				f.write(degree)
		except:
			sys.stderr.write("except pwmwrite0\n")

if __name__ == '__main__':
	udpserver = UDPServer() 
	try:
		asyncore.loop()
	except asyncore.ExitNow, e:
		print e
