EESchema Schematic File Version 2
LIBS:power
LIBS:device
LIBS:transistors
LIBS:conn
LIBS:linear
LIBS:regul
LIBS:74xx
LIBS:cmos4000
LIBS:adc-dac
LIBS:memory
LIBS:xilinx
LIBS:microcontrollers
LIBS:dsp
LIBS:microchip
LIBS:analog_switches
LIBS:motorola
LIBS:texas
LIBS:intel
LIBS:audio
LIBS:interface
LIBS:digital-audio
LIBS:philips
LIBS:display
LIBS:cypress
LIBS:siliconi
LIBS:opto
LIBS:atmel
LIBS:contrib
LIBS:valves
LIBS:power-cache
EELAYER 25 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 1 1
Title ""
Date ""
Rev ""
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L IRF9540N Q1
U 1 1 5983549B
P 6000 2150
F 0 "Q1" H 6250 2225 50  0000 L CNN
F 1 "IRF9540N" H 6250 2150 50  0000 L CNN
F 2 "TO_SOT_Packages_THT:TO-220_Horizontal" H 6250 2075 50  0001 L CIN
F 3 "" H 6000 2150 50  0000 L CNN
	1    6000 2150
	0    -1   -1   0   
$EndComp
$Comp
L GNDREF #PWR01
U 1 1 59835551
P 6050 3200
F 0 "#PWR01" H 6050 2950 50  0001 C CNN
F 1 "GNDREF" H 6050 3050 50  0000 C CNN
F 2 "" H 6050 3200 50  0000 C CNN
F 3 "" H 6050 3200 50  0000 C CNN
	1    6050 3200
	1    0    0    -1  
$EndComp
Wire Wire Line
	6200 2050 7550 2050
Wire Wire Line
	5800 2050 4650 2050
Wire Wire Line
	6050 2850 6050 3200
Wire Wire Line
	4600 3100 7500 3100
Connection ~ 6050 3100
$Comp
L R R1
U 1 1 5983559F
P 6050 2700
F 0 "R1" V 6130 2700 50  0000 C CNN
F 1 "R" V 6050 2700 50  0000 C CNN
F 2 "Resistors_ThroughHole:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal" V 5980 2700 50  0001 C CNN
F 3 "" H 6050 2700 50  0000 C CNN
	1    6050 2700
	1    0    0    -1  
$EndComp
Wire Wire Line
	6050 2350 6050 2550
$Comp
L ZENER D1
U 1 1 598355FA
P 6400 2350
F 0 "D1" H 6400 2450 50  0000 C CNN
F 1 "ZENER" H 6400 2250 50  0000 C CNN
F 2 "Diodes_ThroughHole:D_DO-35_SOD27_P7.62mm_Horizontal" H 6400 2350 50  0001 C CNN
F 3 "" H 6400 2350 50  0000 C CNN
	1    6400 2350
	0    1    1    0   
$EndComp
Wire Wire Line
	6400 2150 6400 2050
Connection ~ 6400 2050
Wire Wire Line
	6050 2550 6400 2550
$Comp
L VCC #PWR?
U 1 1 59837B67
P 4650 1950
F 0 "#PWR?" H 4650 1800 50  0001 C CNN
F 1 "VCC" H 4650 2100 50  0000 C CNN
F 2 "" H 4650 1950 50  0000 C CNN
F 3 "" H 4650 1950 50  0000 C CNN
	1    4650 1950
	1    0    0    -1  
$EndComp
$Comp
L VDD #PWR?
U 1 1 59837B84
P 7550 1950
F 0 "#PWR?" H 7550 1800 50  0001 C CNN
F 1 "VDD" H 7550 2100 50  0000 C CNN
F 2 "" H 7550 1950 50  0000 C CNN
F 3 "" H 7550 1950 50  0000 C CNN
	1    7550 1950
	1    0    0    -1  
$EndComp
Wire Wire Line
	7550 2050 7550 1950
Wire Wire Line
	4650 2050 4650 1950
$EndSCHEMATC
