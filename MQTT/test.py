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
            send_to_mqtt(data.decode("utf-8"))

    def user_input() :
        while 1 :
            msg = input("Type configuration commands : ")
            sock.write(str.encode(msg))
            sock.write(b"\n")


    def send_to_mqtt(data) :
        s = data.split(" ")
        client.publish(s[0], s[1])

    client = mqtt.Client("test")
    client.connect(BROKER_ADDRESS)

    sock = serial.Serial(device_name)

    listening_thread = threading.Thread(target = listen_sensor, args = ())
    user_input_thread = threading.Thread(target = user_input, args = ())
    listening_thread.start()
    user_input_thread.start()

if __name__ == "__main__":
    establish(sys.argv[1])
