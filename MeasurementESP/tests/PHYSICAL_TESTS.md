# Physical acceptance record

Date: 2026-10-28. Status: **partial user-reported run; sensor acceptance incomplete**. The supplied reference selects DevKitC V4 / WROOM-32E, KY-015 on GPIO27,
and KY-018 on GPIO34 through the documented divider. Drivers have been implemented.
Actual module revisions, flash size, electrical wiring, calibration endpoints,
reference instruments and sender serial port remain unverified.
No board was flashed by the agent. The user ran firmware and supplied:

```text
I (180878) MeasurementESP: HTTP 202 accepted (222 bytes)
I (182608) AsBoard: KY-018 raw average=0.00 (GPIO34 ADC1/6)
I (182608) MeasurementESP: sensor errors T/H/L=40/40/0; minimum heap=199660
```

The user confirmed the light sensor is not connected. Its raw zero is not a sensor
measurement; acquisition is now disabled. HTTP acceptance is observed, but successful
DHT acquisition is not established. Exact wiring, capture diagnostics and remaining
acceptance checks are pending. The error counters are cumulative failed snapshots,
not counts of physical DHT transactions.

| Check | Status / evidence to record |
| --- | --- |
| PC on display AP: normal/faults/outage/zero scenarios | Pending; actual display/AP required |
| Display first, sender first, simultaneous cold boot | Pending; DHCP time and HTTP 202 |
| Real sensor units, accuracy and plausible values | Pending; reference instruments and tolerances |
| Test indicator only with explicit simulation | Pending |
| Disconnect/reconnect each sensor independently | Pending; error statuses and recovery delay |
| Stop delivery for >10 seconds | Pending; receiver stale state |
| Power-cycle display during sampling | Pending; reconnect delay, newest frame age |
| Extended operation | Pending; duration, cadence, request failures, minimum heap trend |
| Light dark/bright calibration including raw direction | Pending; raw endpoints, conditions, repeated samples |
| GPIO/voltage/pull-up/power and ADC-with-Wi-Fi checks | Pending; board schematic and sensor datasheet references |

Before testing sender hardware, from SmartDisplay on a PC connected to its AP:

```powershell
python tools/AsMeasurementSender.py --scenario normal --count 10
python tools/AsMeasurementSender.py --scenario faults --count 10
python tools/AsMeasurementSender.py --scenario outage --count 10
python tools/AsMeasurementSender.py --scenario zero --count 5
```

Supply the configured `--device` when it differs from `measurement-01`.
These PC scenarios verify the receiver path only. They were not run in this session.

## Calibration measurements

| Item | Recorded value |
| --- | --- |
| Actual board/module revision and flash size | Pending inspection |
| Measured ADC node minimum/maximum voltage | Pending; expected <=1.65 V |
| Dark raw average, variation and conditions | Pending |
| Bright raw average, variation and conditions | Pending |
| Observed raw direction | Pending; do not assume polarity |
| Enabled menuconfig endpoint values | Not enabled; placeholders are not calibration |
| DHT reference temperature/RH and observed error | Pending |

## Subsequent user report

The user has now connected KY-018 and does not have C1. C1 is optional filtering;
both divider resistors remain required by this wiring. Light acquisition has been
re-enabled, with per-sample raw logging while uncalibrated. Percent values remain
unavailable until measured endpoints are configured. No new raw ADC observations
have yet been supplied after connecting the sensor.

The user supplied this DHT diagnostic:

```text
W (40646) AsDht11: GPIO27 stage=receive_wait error=ESP_ERR_TIMEOUT idle=1 now=1 symbols=0
W (40646) AsBoard: DHT11 GPIO27 transaction: ESP_ERR_TIMEOUT; both readings error
```

This establishes a timeout waiting for the RMT completion callback, not proof that
no sensor pulses existed. Added start-pulse low-level verification and successful
DHT value logs. DHT wiring/pull-up verification and subsequent logs remain pending.

## Pull-up fitted; recurring RMT timeout

The user fitted external R3 (10 kohm), but reported receive-completion timeouts at
894646, 916646, 938646, 960646, 982646 and 1006646 ms (22-second spacing).
`idle=1 now=1 symbols=0` means no completion event was obtained in that attempt.
HTTP 202 traffic continued, alternating blocks of 222/234-byte bodies. Payload
length alone does not establish sensor status. T/H failure counters increased;
reported minimum heap remained 200124 bytes in the supplied excerpt.

Software follow-up: read failures now retry at the next scheduled sample instead
of imposing a ten-second delay. Initialization failures still back off ten seconds.
Selected GPIO capture to compare against the RMT path; it uses the existing decoder,
an 8 ms capture deadline, and no cached or synthetic replacements. Added explicit
T/H/L status labels to HTTP logs. New board-run evidence is required to establish
whether this resolves acquisition. No hardware was flashed by the agent.

## GPIO run received; alignment fix pending board confirmation

User's pasted log reports GPIO captures of 82 pulses rejected as response_preamble,
while Wi-Fi/DHCP and HTTP 202 succeed. Every logged light ADC average is 0.00; no
response to changed illumination or measured GPIO34 voltage has yet been supplied.
Software reproduced/fixed an initial pull-up-rise alignment defect, described in
RESULTS.md. New board results are pending. Light percentage remains deliberately
unavailable until distinct, physically measured calibration endpoints are configured.

## Release-high failure; edge-stream synchronization update

The user reports `gpio_release_high` timeout with idle=1, now=0 and pulses=0 after
fitting the pull-up. The line can already be LOW for the sensor's response when
software first observes it; the prior mandatory short HIGH check was too strict.
A delayed-first-observation GPIO fixture reproduced rejection of a valid waveform.

Replaced the fixed-phase release check with input-mode edge-stream capture through
end-of-frame idle, response-preamble location, and allowance for a partly observed
first LOW only. The complete response HIGH, 40 bits, checksum and sensor range
checks remain. New startup marker: `edge-sync=2`. Host cases pass, but physical
success after flashing is still pending. The user's KY-018 raw readings remain
zero; flashlight/known-divider-input diagnostics are still pending, without a meter.
