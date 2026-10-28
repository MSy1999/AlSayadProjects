# Project-local AlSayadLib modules
WifiManager is a radio-only redesign informed by the existing AlSayadLib/ESP32/WifiManager API. The original also contains CAN-logger HTTP endpoints, geolocation and certificate dependencies; those are intentionally absent here. This is not a live dependency on that external folder.
AnalogOutput is a new reusable LEDC PWM module, not a DAC. A single task owns each output. Share one configured timer among compatible channels.
The external library was read only and was not changed. These modules can be promoted upstream after review.
