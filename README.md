# PIC18K40
Arduino as PIC18K40 programmer
This programmer only works with the relatively new PIC18F??K40 series, as it uses a new programming algorithm.

## Compile

Use Arduino IDE to compile and upload the sketch under folder `PIC18ISP`.

To compile the PC software, you need to install Qt first. Please visit qt.io for more information.

```
cmake [This repository] -DCMAKE_PREFIX_PATH=[Path to Qt];[Path to qhexedit2]
make
```
