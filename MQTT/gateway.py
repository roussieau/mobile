#!/usr/bin/env python3
import sys
import serial
import threading
import paho.mqtt.client as mqtt
BROKER_ADDRESS = "127.0.0.1"
	

def establish(device_name) :

	def listen_sensor() :
		data_to_send_to_mqtt = ""
		while 1 :
			data = sock.readline()
			data_string = data.decode("utf-8")
			if(data_string[0] =="!") :
				for e in data_string.split(";") :
					send_to_mqtt(e[1:])
			else :
				print(data)

	def user_input() :
		while 1 :
			msg = input("Type configuration commands : \n")
			sock.write(str.encode(msg))
			sock.write(b"\n")

	def send_to_mqtt(data) :
		diff = data.split(":")
		if (len(diff) != 2): return
		updated = parse(diff[0])
		client.publish(updated, diff[1])

	def parse(data) :
		arr = data.split("/")
		last = arr[-1]
		last = "temperature" if last == "0" else "humidity"
		arr = arr[:-1] 
		arr.append(last)
		parsed = ""
		for e in arr :
			parsed +=(e + "/")
		print(parsed)
		return parsed

	def on_message(client, userdata, message) :
		msg = message.payload.decode("utf-8")
		if(msg == "1") :
		    sock.write(b"power off\n")
		else :
		    sock.write(b"power on\n")

	sock = serial.Serial(device_name)

	client = mqtt.Client("gateway")
	client.connect(BROKER_ADDRESS)
	client.subscribe("$SYS/broker/subscriptions/count")
	client.on_message = on_message

	listening_thread = threading.Thread(target = listen_sensor, args = ())
	user_input_thread = threading.Thread(target = user_input, args = ())
	listening_thread.start()
	user_input_thread.start()

	client.loop_forever()

if __name__ == "__main__":
	establish(sys.argv[1])
