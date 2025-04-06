PS/2 to I2C Convertor by MSP430G2553 

PS/2 power is up convert 5V from 3.3V used by MT3608. 

MSP430G2553 is not 5V Trerant. 

MPS430 connect PS/2 need use level convertor sach as FXMA2102. 

My module not have 32Khz crystal. Then can't use WDT function. 

P1.6 I2C SDA 
P1.7 I2C SCL 

P2.6  PS/2 signal clock 
P2.7  PS/2 signal data 
 

This is sample script by mruby. 


```
MSPADDR = 0x4a

SETLED = 0
GETKEY = 1

t = BsdIic.new(0)

# NUM Lock LED On
t.write(MSPADDR, SETLED, 4)

usleep 100_000

loop do
  cur = t.read(MSPADDR, 2, GETKEY)
  if cur != nil && cur[0] != 0
    if cur[1] == 0 then
      print cur[0].to_s(16) + "\n"
    else
      print cur[0].to_s(16) + " " + cur[1].to_s(16) + "\n"
    end
  end
  usleep 10_000
end

```
