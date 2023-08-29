# smartRelay
Device for controlling electrical equipment connection to ~230V


### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

## Example Output

Running this example, you will see the following log output on the serial monitor:

```
I (302) ADC1_CH2: raw  data: 4095
I (302) ADC1_CH2: cali data: 3109 mV
I (1302) ADC2_CH0: raw  data: 0
I (1302) ADC2_CH0: cali data: 0 mV
I (2302) ADC1_CH2: raw  data: 4095
I (2302) ADC1_CH2: cali data: 3109 mV
I (3302) ADC2_CH0: raw  data: 0
I (3302) ADC2_CH0: cali data: 0 mV
I (4302) ADC1_CH2: raw  data: 4095
I (4302) ADC1_CH2: cali data: 3109 mV
...
```


