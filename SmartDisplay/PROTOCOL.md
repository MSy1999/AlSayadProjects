# Room Monitor measurement protocol v1

The display hosts a WPA2 AP at `192.168.4.1`. The Measurement ESP32 joins it and sends one full snapshot every two seconds. Keep one request in flight; after temporary failures send the newest snapshot rather than replaying an old backlog.

## Request

`POST http://192.168.4.1/api/v1/measurements`

Use `Content-Type: application/json`, a body of 1-1024 bytes, and no NUL terminator. Member names/status strings are case-sensitive.

```json
{
  "version": 1,
  "device_id": "measurement-01",
  "readings": {
    "temperature_c": {"value": 23.4, "status": "valid"},
    "humidity_rh": {"value": 56.7, "status": "valid"},
    "brightness_pct": {"value": 0, "status": "valid"}
  }
}
```

All three entries are required. Device ID is 1-32 bytes and must match menuconfig. Valid temperature is finite and between -40 and 125 °C. Humidity and relative brightness are finite values from 0 to 100. These are receiver display bounds, not sensor accuracy claims. Brightness is never lux.

| Status | Value | Meaning |
| --- | --- | --- |
| `valid` | In-range finite number, including zero | Current successful attempt |
| `error` | `null` | Measurement attempt failed |
| `unavailable` | `null` | No measurement is available |

Independent failure example:

```json
{
  "version": 1,
  "device_id": "measurement-01",
  "readings": {
    "temperature_c": {"value": null, "status": "error"},
    "humidity_rh": {"value": 55, "status": "valid"},
    "brightness_pct": {"value": null, "status": "unavailable"}
  }
}
```

Optional `"simulated": true` marks test values in the Arabic title. Omit it or use false for real sensor readings. The test sender always sets true. It is presentation metadata, not authentication.

Required members must not be duplicated. Wrong types/ranges, NULs, nesting over 16 levels, malformed JSON, unsupported versions/devices and trailing non-whitespace are rejected. Extra metadata is ignored within those limits.

## Responses

| HTTP status | Meaning/action |
| --- | --- |
| 202 | Valid snapshot copied to processing queue |
| 400 | Invalid/incomplete payload; fix it |
| 408 | Body reception timed out |
| 413 | Empty or oversized body |
| 503 | Queue full; retry the newest reading later |

Success body: `{"accepted":true}`. Validation and capacity errors use `{"error":"invalid_measurement"}` and `{"error":"busy"}`. Acceptance does not confirm an SD write. The eight-event queue also carries brightness-save requests. Two AP clients are allowed, permitting a test PC alongside the Measurement ESP32.

## Time, states and history

Frames describe latest sensor attempts. Do not resend an old success as valid after a failed attempt. Receiver monotonic arrival time determines age; clock synchronization is unnecessary. After ten seconds without an accepted snapshot, numeric readings are hidden and marked stale.

Five-second buckets retain their most recent accepted readings/statuses. Missing buckets stay gaps rather than zero. RAM retains 120 buckets; supported SD records the same completed buckets asynchronously. Wi-Fi client association alone does not indicate current sensor data.

## Other transports

`AsMeasurement` contains the device ID, simulated marker and three typed readings. `AsTransport` delivers bounded frame bytes and gets acceptance errors through its callback. The private cJSON decoder is independent of Wi-Fi and the portable model. UART/BLE adapters can send the same JSON frames or decode another format and call `as_room_submit()` with typed data.
