import requests, json, threading, time, datetime, os
import paho.mqtt.client as mqtt


"""# Inicializace

Aplikace https://cadweb.bnl.gov/dashs/Operations/BroadcastWeb/BroadcastMain.dash
je psaná ve frameworku Vaadin, který pro aktualizaci UI využívá svého jazyka UIDL.

Trochu info zde: https://mvysny.github.io/Vaadin8-communication-explained/

Následující kus kódu pošle inicializační požadavek na server. Odpovědí je JSON který obsahuje token pro další požadavky a informace k inicializaci komponent UI s prvotními daty (mimochodem jsou tam i aktuální data grafu, který se na stránce zobrazuje).

Pak jde periodicky posílat požadavek na aktualizace hodnot v příslušných částech UI, resp. přímo pro ID nějakého prvku.
"""

def init_connection(tab, s):
    curdate = int(time.time() * 3)
    init_message = {
        'v-browserDetails': 1,
        'theme': 'dashs-dark',
        'v-appId': 'dashs-95355681',
        # 4 arguments specifying screen and client window resolution
        # (doesn't seem to matter much)
        'v-sh': 768,
        'v-sw': 1366,
        'v-ch': 768,
        'v-cw': 1366,
        'v-curdate': curdate, # Unix timestamp
        # Timezone offset?
        'v-tzo': -120,
        'v-dstd': 60,
        'v-rtzo': -60,
        'v-dston': True,
        'v-vw': 1366,
        'v-vh': 768,
        'v-loc': tab,
        'v-wn': '759'
    }
    init_url = tab + '?v-{}'.format(curdate)

    init_post = s.post(init_url, init_message)
    init_resp = json.loads(init_post.text)
    init_uidl = json.loads(init_resp['uidl'])
    return init_resp['v-uiId'], init_uidl


def find_interesting_types(typemap, types):
    our_types = {}
    for element in typemap.items():
        if element[0] == 'gov.bnl.cad.vchart.VChart'\
        or element[0] == 'com.vaadin.ui.Label':
        # Save both name (for lookup later) and an empty array
        # for items which are of that type.
            our_types[element[1]] = {'name': element[0], 'items': []}

    for element in types.items():
        t = int(element[1])
        if t in our_types:
            our_types[t]['items'].append(element[0])

    return our_types

def make_update_msg(components, UIDL_Data):
    poll_java_method = 'poll'
    poll_java_ns = 'com.vaadin.shared.ui.ui.UIServerRpc'
    poll_array = []
    for comp in components:
        poll_array.append([comp, poll_java_ns, poll_java_method, []])
    msg = {
        'csrfToken': UIDL_Data['token'],
        'clientId': UIDL_Data['clientId'],
        'syncId': UIDL_Data['syncId'],
        'rpc': poll_array,
        'wsver': '7.7.0'
    }
    return msg

def send_update_request(components, UIDL_Data):
    msg = make_update_msg(components, UIDL_Data)
    resp = UIDL_Data['requests_session'].post(UIDL_Data['uidl_url'], json=msg)
    result = json.loads(resp.text[9:-1:1])

    clientId = 0
    syncId = 0
    if 'clientId' in result:
        clientId = result['clientId']
        syncId = result['syncId']
    UIDL_Data['clientId'] = clientId
    UIDL_Data['syncId'] = syncId

    return result


def __main__():
    s = requests.Session()

    # Which tab of the application to get. Uncomment a tab here:
    tabs = ['https://cadweb.bnl.gov/dashs/Operations/BroadcastWeb/Injection.dash',\
        'https://cadweb.bnl.gov/dashs/Operations/BroadcastWeb/Ramp.dash',\
        'https://cadweb.bnl.gov/dashs/Operations/BroadcastWeb/Store.dash']
    tab = tabs[int(os.environ['CAD_DATA_TAB'])]

    uidl_id, uidl = init_connection(tab, s)
    token = uidl['Vaadin-Security-Key']

    our_types = find_interesting_types(uidl['typeMappings'], uidl['types'])

    # Into separate variables
    our_charts = [x for x in our_types.items() if x[1]['name'] == 'gov.bnl.cad.vchart.VChart'][0][1]['items']
    our_charts.sort(key= lambda v: int(v))
    our_labels = [x for x in our_types.items() if x[1]['name'] == 'com.vaadin.ui.Label'][0][1]['items']
    our_labels.sort(key= lambda v: int(v))

    # Which items are to be polled
    to_sync = [x for x in uidl['state'] if 'pollInterval' in uidl['state'][x]]

    we_want = ['Blue Loss Rate', 'Yellow Loss Rate']
    prefixes = ['Blue', 'Yellow']
    to_send = {}
    vals = {}
    pick_next = None
    prefix = ''
    for label in our_labels:
        text = uidl['state'][label]['text']
        if pick_next:
            to_send[label] = f'{prefix}{pick_next}'
            vals[label] = text
            pick_next = None
        if text in prefixes:
            prefix = f'{text} '
        if f'{prefix}{text}' in we_want:
            pick_next = text

    uidl_url = 'https://cadweb.bnl.gov/dashs/UIDL/?v-uiId={}'.format(uidl_id)
    clientId = 0
    syncId = uidl['syncId']

    UIDL_Data = {
        'clientId': clientId,
        'syncId': syncId,
        'uidl_url': uidl_url,
        'token': token,
        'requests_session': s
    }

    topic_names = {}
    for id in to_send:
        topic_names[id] = ['art01/star/' + to_send[id].lower().replace(' ', '-')]

    client = mqtt.Client('cad_sender-1', True)
    client.tls_set()
    client.username_pw_set(os.environ['MQTT_USERNAME'], os.environ['MQTT_PASSWORD'])
    client.enable_logger()

    while True:
        cret = client.connect(host=os.environ['MQTT_SERVER'], port=int(os.environ['MQTT_PORT']))
        next_call = time.time()

        while cret <= 0:
            print(datetime.datetime.now())
            result = send_update_request(to_sync, UIDL_Data)
            if 'state' not in result:
                client.disconnect()
                return

            for id in to_send:
                if id in result['state']:
                    try:
                        val = result['state'][id]['text']
                    except KeyError:
                        client.disconnect()
                        return
                    vals[id] = val

            for id in to_send:
                print(to_send[id])
                print(vals[id])
                fval = float(vals[id].split()[0])
                for topic_name in topic_names[id]:
                    cret = client.publish(topic_name, fval)

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
