"""SIMULATED measurements for the Room Monitor receiver; not sensor firmware."""
import argparse
import json
import math
import time
import urllib.error
import urllib.request


def frame(device, index, scenario):
    readings = {
        'temperature_c': {'value': round(23 + math.sin(index / 8) * 2, 1), 'status': 'valid'},
        'humidity_rh': {'value': round(55 + math.sin(index / 11) * 5, 1), 'status': 'valid'},
        'brightness_pct': {'value': round(50 + math.sin(index / 6) * 30, 1), 'status': 'valid'},
    }
    if scenario == 'zero':
        for reading in readings.values():
            reading['value'] = 0
    if scenario == 'faults' and index % 8 in (3, 4):
        readings['temperature_c'] = {'value': None, 'status': 'error'}
        readings['brightness_pct'] = {'value': None, 'status': 'unavailable'}
    return {'version': 1, 'device_id': device, 'simulated': True, 'readings': readings}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url', default='http://192.168.4.1/api/v1/measurements')
    parser.add_argument('--device', default='measurement-01')
    parser.add_argument('--scenario', choices=['normal', 'zero', 'faults', 'outage', 'malformed'],
                        default='normal')
    parser.add_argument('--count', type=int, default=0, help='0 = until Ctrl+C')
    parser.add_argument('--interval', type=float, default=2)
    parser.add_argument('--dry-run', action='store_true', help='Print one frame without sending')
    args = parser.parse_args()
    if args.count < 0 or args.interval <= 0:
        parser.error('count must be nonnegative and interval must be positive')
    print('SIMULATED / TEST DATA - no physical sensors are being read', flush=True)
    index = 0
    try:
        while args.count == 0 or index < args.count:
            payload = json.dumps(frame(args.device, index, args.scenario), separators=(',', ':'))
            if args.scenario == 'malformed':
                payload += ' trailing-data'
            if args.dry_run:
                print(payload)
                return
            if args.scenario == 'outage' and index > 0 and index % 8 == 0:
                print('SIMULATED outage: pausing for 12 seconds', flush=True)
                time.sleep(12)
            request = urllib.request.Request(args.url, data=payload.encode('utf-8'), method='POST',
                                             headers={'Content-Type': 'application/json'})
            try:
                with urllib.request.urlopen(request, timeout=3) as response:
                    print(f'TEST frame {index}: HTTP {response.status} {response.read().decode()}', flush=True)
            except (urllib.error.URLError, TimeoutError) as error:
                print(f'TEST frame {index}: {error}', flush=True)
            index += 1
            if args.count == 0 or index < args.count:
                time.sleep(args.interval)
    except KeyboardInterrupt:
        print('\nTest sender stopped')


if __name__ == '__main__':
    main()
