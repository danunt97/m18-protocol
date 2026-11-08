# Bauteillage (x, y, Drehung, Seite) in mm ab linker oberer Platinenecke.
# Platine W x H (x = Einsteckrichtung, Messerspitzen zeigen nach +x), Akkukontakte auf der Unterseite.
# SuperMini um 180 Grad gedreht: USB-C zeigt zur unteren Kante (y=H, Seite B-) = in Steckrichtung rechts,
# damit der Stecker nicht gegen den Akku-Buckel laeuft.
PIN1_X=48.8            # Reihe Pin 1..9 (GPIO43, 44, 1..7); Reihe 10..18 (5V, GND, 3V3, 13..8) bei PIN1_X-15.24
PIN1_Y=H-2.2           # Pin 1 und 10 an der USB-Seite, Modulkante buendig mit der Platinenkante
P.update({
 'H1':(4.0,4.0,0,'F'), 'H2':(W-4.0,4.0,0,'F'), 'H3':(4.0,H-4.0,0,'F'), 'H4':(W-4.0,H-4.0,0,'F'),
 'U1':(PIN1_X, PIN1_Y, 180, 'F'),
 # Versorgung: Sicherung, Verpolschutz, TVS, C1 auf der Oberseite zwischen den Messern (dort nur SMD)
 'F1':(13.8,10.56,0,'F'), 'D1':(13.4,16.19,0,'F'), 'C1':(13.5,21.8,0,'F'), 'D2':(13.9,27.44,0,'F'),
 'U2':(22.3,23.0,0,'F'), 'D3':(25.6,27.6,180,'F'), 'C2':(21.3,30.75,0,'F'),
 'TP1':(21.0,4.4,0,'F'), 'TP2':(21.0,34.4,0,'F'),   # CHG+ oben bei B+, CHG- unten bei B-
 # LED links unten: ausserhalb von Display und USB-Stecker
 'LED1':(25.0,32.6,0,'F'),
 # J2-Sendestufe oben, R6 zwischen den Kontaktstiften
 'R1':(31.9,12.0,180,'F'), 'Q1':(27.2,12.4,180,'F'), 'R2':(26.4,15.2,0,'F'),
 'R4':(25.6,9.0,180,'F'), 'Q2':(21.8,11.4,270,'F'), 'R3':(21.8,7.5,0,'F'),
 'R5':(21.4,15.2,180,'F'), 'R6':(4.284,22.25,270,'F'), 'TP3':(4.284,19.0,0,'F'),
 # unter dem Modul (zwischen den Stiftreihen): J1-Empfangsstufe, I2C-Pull-ups, LED-Treiber
 'R8':(37.3,14.4,0,'F'), 'R9':(41.4,14.4,0,'F'), 'R10':(46.0,14.6,90,'F'),
 'Q3':(37.95,18.0,0,'F'), 'Q4':(43.5,18.3,0,'F'),
 'R7':(37.3,23.0,180,'F'),
 # I2C-Pull-ups oben rechts neben der OLED-Buchse
 'R11':(51.5,9.6,0,'F'), 'R12':(51.5,12.0,0,'F'),
 # LED-Treiber in der Reihenfolge der LED-Beinchen von links: rot (Q5), gruen (Q7), blau (Q6)
 'R16':(37.1,26.4,90,'F'), 'R18':(40.75,26.4,90,'F'), 'R17':(44.4,26.4,90,'F'),
 'Q5':(37.1,31.1,270,'F'), 'Q7':(40.75,31.1,270,'F'), 'Q6':(44.4,31.1,270,'F'),
 'R13':(37.1,35.1,0,'F'), 'R15':(41.0,35.1,0,'F'), 'R14':(44.9,35.1,0,'F'),
 # Stecker oben: J2 Lade-Steuerung (Reihe y=7.2), J1 OLED (Reihe y=2.6, von oben links->rechts GND VCC SCL SDA,
 # Display schaut ueber den SuperMini)
 'J2':(30.4,7.2,90,'F'), 'J1':(39.88,2.6,0,'F'),
})
