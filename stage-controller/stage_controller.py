import paho.mqtt.client as mqtt
import os
import socket
from encodings import utf_8

ANIMATION_MESSAGE = 'AdvanceStagePulsing 3'
ANIMATION_MESSAGE_IP = '127.0.0.1'
ANIMATION_MESSAGE_PORT = 1234

class AnimationDecider:
    def __init__(self, initial_value, decision_boundary) -> None:
        self.value = initial_value
        self.boundary = decision_boundary
    
    def should_animate(self, data):
        result = False
        if self.value < self.boundary and data > self.boundary:
            result = True
        self.value = data
        return result

def __main__():
    client = mqtt.Client('art01-recv', clean_session=True, reconnect_on_failure=True)
    client.tls_set()
    client.username_pw_set(os.environ['MQTT_USERNAME'], os.environ['MQTT_PASSWORD'])
    client.enable_logger()

    if 'ANIMATION_MESSAGE' in os.environ:
        ANIMATION_MESSAGE = os.environ['ANIMATION_MESSAGE']
    if 'ANIMATION_MESSAGE_IP' in os.environ:
        ANIMATION_MESSAGE_IP = os.environ['ANIMATION_MESSAGE_IP']
    if 'ANIMATION_MESSAGE_PORT' in os.environ:
        ANIMATION_MESSAGE_PORT = os.environ['ANIMATION_MESSAGE_PORT']

    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    def on_message(client, userdata: map, msg: mqtt.MQTTMessage):
        if not msg.topic in userdata:
            return
        decider: AnimationDecider = userdata[msg.topic]
        try:
            data = float(utf_8.decode(msg.payload)[0])
        except Exception as e:
            print(f'Failed to convert payload to a float: {e}', flush=True)
            return
        if (decider.should_animate(data)):
            print(f'Sending animation message!', flush=True)
            s.sendto(ANIMATION_MESSAGE, (ANIMATION_MESSAGE_IP, ANIMATION_MESSAGE_PORT))
    
    m = {'art01/star/yellow-loss-rate': AnimationDecider(0.0, 20.0),
#         'art01/star/blue-loss-rate': AnimationDecider(0.0, 20.0),
#         'art01/cern/i_b1': AnimationDecider(0.0, 2000.0),
         'art01/cern/i_b2': AnimationDecider(0.0, 2000.0)}
    client.user_data_set(m)
    print(f'Connecting to MQTT server: {os.environ["MQTT_SERVER"]}:{os.environ["MQTT_PORT"]}', flush=True)
    client.connect(os.environ['MQTT_SERVER'], int(os.environ['MQTT_PORT']))
    print(f'Connected!', flush=True)
    client.subscribe('art01/#')

    client.loop_forever()

if __name__ == '__main__':
    __main__()