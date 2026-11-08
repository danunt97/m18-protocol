# Bauteillage (x, y, Drehung, Seite) in mm ab linker oberer Platinenecke.
# Platine W x H (x = Einsteckrichtung, Messerspitzen bei x=0), Akkukontakte auf der Unterseite.
PIN1_X=W-2.2           # U1 Pin 1 (GPIO43, USB-C zeigt nach rechts)
R1Y=YC-7.62; R2Y=YC+7.62
def px(k): return PIN1_X-2.54*(k-1)     # x von Pin k (1..9) einer Reihe
P.update({
 'H1':(4.0,4.0,0,'F'), 'H2':(W-4.0,4.0,0,'F'), 'H3':(4.0,H-4.0,0,'F'), 'H4':(W-4.0,H-4.0,0,'F'),
 'U1':(PIN1_X, R1Y, 270, 'F'),
 # J2-Sendestufe oben in der Mitte, R6 zwischen den Kontaktstiften
 'R1':(31.9,12.0,180,'F'), 'Q1':(27.2,12.4,180,'F'), 'R2':(31.9,14.4,0,'F'),
 'R4':(25.6,9.0,180,'F'), 'Q2':(21.8,11.4,270,'F'), 'R3':(21.8,7.5,0,'F'),
 'R5':(21.4,15.2,180,'F'), 'R6':(14.72,16.8,270,'F'), 'R7':(36.6,23.0,180,'F'),
 # Versorgung unten in der Mitte
 'TP1':(21.0,34.4,0,'F'), 'TP2':(21.0,4.4,0,'F'),
 'F1':(27.8,35.6,0,'F'), 'D1':(27.8,31.6,0,'F'), 'D2':(28.6,27.6,0,'F'),
 'C1':(21.0,26.6,180,'F'), 'U2':(22.3,23.0,0,'F'), 'C2':(32.2,21.0,90,'F'),
 'D3':(46.0,31.0,0,'F'),
 # LED unten rechts, Treiber unter dem Modul
 'LED1':(35.2,31.2,0,'F'),
 'R13':(36.6,36.6,0,'F'), 'R14':(40.6,36.6,0,'F'), 'R15':(44.6,36.6,0,'F'),
 'Q5':(48.2,21.9,270,'F'), 'Q6':(51.8,21.9,270,'F'), 'Q7':(55.4,21.9,270,'F'),
 'R16':(48.2,16.8,90,'F'), 'R17':(51.8,16.8,90,'F'), 'R18':(55.4,16.8,90,'F'),
 # J1-Empfangsstufe + I2C unter dem Modul
 'R8':(36.6,14.4,0,'F'), 'Q3':(36.8,18.0,0,'F'), 'R9':(41.4,14.4,0,'F'),
 'Q4':(42.2,18.2,0,'F'), 'R10':(46.0,15.0,90,'F'),
 'R11':(40.6,23.4,0,'F'), 'R12':(44.45,23.4,0,'F'),
 # Stecker oben: J2 Lade-Steuerung (Reihe y=7.2), J1 OLED (Reihe y=2.6, von oben links->rechts GND VCC SCL SDA,
 # Display schaut ueber den SuperMini)
 'J2':(30.4,7.2,90,'F'), 'J1':(39.88,2.6,0,'F'),
})
