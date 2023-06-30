from requests import get
from PIL import ImageFile, ImageOps
import pytesseract, os, time, datetime
import paho.mqtt.client as mqtt

def get_current_values():
    """Downloads snapshot of the current status page for LHC at CERN, then runs OCR on the status bar,
    extracting measured current values of the two beams. The values are returned in a pair,"""

    image_req = get('https://vistar-capture.s3.cern.ch/lhc3.png')
    if not image_req.ok:
        print('Failed to download current data from CERN!')
        return False
    parser = ImageFile.Parser()
    parser.feed(image_req.content)
    img = parser.close().crop([1, 1, 1023, 37])
    img = ImageOps.invert(ImageOps.grayscale(img)).quantize(2)
    text = pytesseract.image_to_string(img).split(' ')
    try:
        I_b1 = float(text[-3])
        I_b2 = float(text[-1])
    except Exception as e:
        print(f'Failed to parse particle beam currents from CERN: {e}')
        return False
    print(pytesseract.image_to_string(img))
    return (I_b1, I_b2)

def __main__():
    client = mqtt.Client('vistar_sender-1', True)
    client.tls_set()
    client.username_pw_set(os.environ['MQTT_USERNAME'], os.environ['MQTT_PASSWORD'])
    client.enable_logger()

    while True:
        cret = client.connect(host=os.environ['MQTT_SERVER'], port=int(os.environ['MQTT_PORT']))
        next_call = time.time()

        while cret <= 0:
            print(datetime.datetime.now())
            result = get_current_values()
            if not result:
                client.disconnect()
                return;
        
            cret = client.publish('art01/cern/i_b1', result[0]) \
                and client.publish('art01/cern/i_b2', result[1])

            # First let client handle pending networking, then sleep
            # rest of the time ourselves
            next_call = next_call + 5
            sleeptime = next_call - time.time()
            if sleeptime >= 0:
                cret = client.loop(next_call - time.time())
            sleeptime = next_call - time.time()
            if sleeptime >= 0:
                time.sleep(next_call - time.time())

if __name__ == "__main__":
    __main__()

