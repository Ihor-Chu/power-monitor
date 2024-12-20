# power-monitor
Remote monitoring battery on UPS

Measure battery voltage and draw a chart on thingspeak.com

To use it:
Register at https://thingspeak.mathworks.com/; create channel; got API key (its free)
Edit power_monitor.ino; write api key, channel id and WIFI settings.

Get power from adjustable power supply 10..15V; measure some voltage values and save it to http://192.168.41./cm (do calibrate ADC).

Ger power from battery; see plots in https://thingspeak.mathworks.com/channels/${channel_id}
