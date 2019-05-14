#!/usr/bin/env python3
import paho.mqtt.client as mqtt
#import time

BROKER_ADDRESS = "127.0.0.1"

def on_message(client, userdata, message):
    print("message received " ,str(message.payload.decode("utf-8")))
    print("message topic=",message.topic)
    print("message qos=",message.qos)
    print("message retain flag=",message.retain)

if __name__ == "__main__":
    print("MQTT subscriber")

    client = mqtt.Client()
    client.connect(BROKER_ADDRESS)
    client.subscribe("3/temperature/")
    client.subscribe("2/temperature/")
    client.subscribe("1/temperature/")
    client.on_message = on_message
    client.loop_forever()
