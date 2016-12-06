#define _CLK 12 //2
#define _DAT 13 //1
#define _MCLR 11

#define TERAB 26
#define TERAR 3
#define TPINT 3

//Big endian [0][1][2][3]
void EncodeUInt16(uint16_t i, unsigned char o[]) {
  o[0] = i >> 8;
  o[1] = i;
}

uint16_t DecodeUInt16(unsigned char i[]) {
  int16_t ret = i[0];
  ret <<= 8;
  ret += i[1];
  return ret;
}

void EncodeUInt32(uint32_t i, unsigned char o[]) {
  o[0] = i >> 24;
  o[1] = i >> 16;
  o[2] = i >> 8;
  o[3] = i;
}

uint32_t DecodeUInt32(unsigned char i[]) {
  return ((uint32_t)i[0] << 24) + ((uint32_t)i[1] << 16) +
         ((uint32_t)i[2] << 8) + (uint32_t)i[3];
}

void setup() {
  Serial.begin(115200);
  pinMode(_CLK, OUTPUT);
  pinMode(_DAT, OUTPUT);
  pinMode(_MCLR, OUTPUT);
  digitalWrite(_MCLR, HIGH);
}

void erase_chip() {
  ICSPSend(0x80);
  SendPayload(0x00, 0x00, 0x00);
  ICSPSend(0x18);
  delay(TERAB);
}

void enable_programming() {
  digitalWrite(_MCLR, LOW);
  delay(1);
  ICSPSend('M');
  ICSPSend('C');
  ICSPSend('H');
  ICSPSend('P');
  delay(1);
}

void disable_prog() {
  digitalWrite(_MCLR, HIGH);
}

uint8_t buff[256];

uint8_t getch() {
  while (!Serial.available());
  return Serial.read();
}

void fill(int n) {
  for (int x = 0; x < n; x++) {
    buff[x] = getch();
  }
}

enum {
  CMD_START_PROG = 0x40, //@
  CMD_END_PROG, //A
  CMD_LATCH_DATA, //B
  CMD_ERASE_ROW, //C
  CMD_WRITE_ROW, //D
  CMD_ERASE_CHIP, //E
  CMD_QUERYINFO, //F
  CMD_WRITE_CONF, //G
  ICMD_LOAD_PC, //H
  ICMD_BULK_ERASE, //I
  ICMD_ROW_ERASE,
  ICMD_READ_DATA_PCINC,
  ICMD_READ_DATA,
  ICMD_LOAD_DATA_PCINC,
  ICMD_LOAD_DATA,
  ICMD_INCREMENT_ADDRESS,
  ICMD_INTERNALLY_TIMED_PROGRAM,
  CMD_OK = 0x20
};

#define RSP_OK 0x61
#define RSP_ERROR 0x62
#define RSP_OUT_OF_SYNC 0x63

void loop() {
  uint8_t cmd = getch();
  switch (cmd) {
    case CMD_START_PROG:
      fill(1);
      if (buff[0] != CMD_OK)
        break;
      enable_programming();
      Serial.write(RSP_OK);
      break;
    case CMD_END_PROG:
      fill(1);
      if (buff[0] != CMD_OK)
        break;
      disable_prog();
      Serial.write(RSP_OK);
      break;
    case CMD_LATCH_DATA: {
        fill(4);
        uint32_t addr = DecodeUInt32(buff);
        fill(2);
        uint16_t len = DecodeUInt16(buff);
        fill(len);
        if (len % 2 == 1) {
          buff[len] = 0xFF;
          len++;
        }
        ICSPSend(0x80);
        SendPayload(addr);
        for (int i = 0; i < len; i += 2) {
          ICSPSend(0x02);
          SendPayload(0x00, buff[i + 1], buff[i]);
        }
        fill(1);
        if (buff[0] != CMD_OK)
          break;
        Serial.write(RSP_OK);
        break;
      }
    case CMD_ERASE_ROW: {
        fill(5);
        if (buff[4] != CMD_OK)
          break;
        uint32_t addr = DecodeUInt32(buff);
        ICSPSend(0x80);
        SendPayload(addr);
        ICSPSend(0xF0);
        delay(TERAR);
        Serial.print("Erased");
        Serial.print(addr);
        Serial.write(RSP_OK);
        break;
      }
    case CMD_WRITE_ROW: {
        fill(5);
        if (buff[4] != CMD_OK)
          break;
        uint32_t addr = DecodeUInt32(buff);
        ICSPSend(0x80);
        SendPayload(addr);
        ICSPSend(0xE0);
        delay(TPINT);
        Serial.print("Wrote");
        Serial.print(addr);
        Serial.write(RSP_OK);
        break;
      }
    case CMD_ERASE_CHIP:
      fill(1);
      if (buff[0] != CMD_OK)
        break;
      erase_chip();
      Serial.write(RSP_OK);
      break;
    case CMD_QUERYINFO:
      fill(1);
      if (buff[0] != CMD_OK)
        break;

      ICSPSend(0x80);
      SendPayload(0x3F, 0xFF, 0xFC);
      for (int i = 0; i < 2; i++) {
        ICSPSend(B11111110);
        PrintPayload();
      }/*
      ICSPSend(0x80);
      SendPayload(0x20, 0x00, 0x00);
      for (int i = 0; i < 8; i++) {
        ICSPSend(B11111110);
        PrintPayload();
      }*/
      ICSPSend(0x80);
      SendPayload(0x30, 0x00, 0x00);
      for (int i = 0; i < 6; i++) {
        ICSPSend(B11111110);
        PrintPayload();
      }
      Serial.println("@0x00");
      ICSPSend(0x80);
      SendPayload(0x00, 0x00, 0x00);
      for (int i = 0; i < 64; i++) {
        ICSPSend(0xFE);
        PrintPayload();
      }
      Serial.println("@0x1ff00");
      ICSPSend(0x80);
      SendPayload(0x1ff00);
      for (int i = 0; i < 64; i++) {
        ICSPSend(0xFE);
        PrintPayload();
      }
      Serial.write(RSP_OK);
      break;
    case CMD_WRITE_CONF:
      fill(13);
      if (buff[12] != CMD_OK)
        break;
      uint32_t curOff;
      for (curOff = 0; curOff < 0x0C; curOff += 2) {
        ICSPSend(0x80); //Load PC
        SendPayload(0x300000 + curOff);
        ICSPSend(0x00); //Load data for NVM
        SendPayload(0x00, buff[curOff + 1], buff[curOff]);
        ICSPSend(0xE0); //Begin internally timed prog
        delay(TPINT); //Wait for Tpint
      }
      Serial.write(RSP_OK);
      break;
    case ICMD_LOAD_PC: {
        fill(4);
        uint32_t addr = DecodeUInt32(buff);
        ICSPSend(0x80);
        SendPayload(addr);
        Serial.write(RSP_OK);
        break;
      }
    case ICMD_BULK_ERASE: {
        ICSPSend(0x18);
        delay(TERAB);
        Serial.write(RSP_OK);
        break;
      }
    case ICMD_ROW_ERASE: {
        ICSPSend(0xF0);
        delay(TERAR);
        Serial.write(RSP_OK);
        break;
      }
    case ICMD_READ_DATA_PCINC: {
        ICSPSend(0xFE);
        PrintPayload();
        Serial.write(RSP_OK);
        break;
      }
    case ICMD_READ_DATA: {
        ICSPSend(0xFC);
        PrintPayload();
        Serial.write(RSP_OK);
        break;
      }
    case ICMD_LOAD_DATA_PCINC: {
        fill(2);
        ICSPSend(0x02);
        SendPayload(0x00, buff[0], buff[1]);
        Serial.write(RSP_OK);
        break;
      }
    case ICMD_LOAD_DATA: {
        fill(2);
        ICSPSend(0x00);
        SendPayload(0x00, buff[0], buff[1]);
        Serial.write(RSP_OK);
        break;
      }
    case ICMD_INCREMENT_ADDRESS: {
        ICSPSend(0xF8);
        Serial.write(RSP_OK);
        break;
      }
    case ICMD_INTERNALLY_TIMED_PROGRAM: {
        ICSPSend(0xE0);
        delay(TPINT);
        Serial.write(RSP_OK);
        break;
      }
    default:
      break;
  }
}

void ICSPSend(uint8_t data) {
  pinMode(_DAT, OUTPUT);
  for (int i = 0x80; i != 0; i >>= 1) {
    digitalWrite(_DAT, data & i);
    digitalWrite(_CLK, HIGH);
    delayMicroseconds(1);
    digitalWrite(_CLK, LOW);
    delayMicroseconds(1);
  }
}

void PrintPayload() {
  uint32_t data = 0;
  pinMode(_DAT, INPUT_PULLUP);
  for (int i = 0; i < 24; i++) {
    digitalWrite(_CLK, HIGH);
    delayMicroseconds(1);
    data <<= 1;
    data |= digitalRead(_DAT);
    digitalWrite(_CLK, LOW);
    delayMicroseconds(1);
  }
  data >>= 1;
  Serial.write((data >> 8) & 0xFF);
  Serial.write(data & 0xFF);
}

void SendPayload(uint32_t i32) {
  SendPayload(i32 >> 16, i32 >> 8, i32);
}

void SendPayload(uint8_t a, uint8_t b, uint8_t c) {
  pinMode(_DAT, OUTPUT);
  digitalWrite(_DAT, LOW);
  digitalWrite(_CLK, HIGH);
  delayMicroseconds(1);
  digitalWrite(_CLK, LOW);
  delayMicroseconds(1);
  for (int i = 0x20; i != 0; i >>= 1) {
    digitalWrite(_DAT, a & i);
    digitalWrite(_CLK, HIGH);
    delayMicroseconds(1);
    digitalWrite(_CLK, LOW);
    delayMicroseconds(1);
  }
  for (int i = 0x80; i != 0; i >>= 1) {
    digitalWrite(_DAT, b & i);
    digitalWrite(_CLK, HIGH);
    delayMicroseconds(1);
    digitalWrite(_CLK, LOW);
    delayMicroseconds(1);
  }
  for (int i = 0x80; i != 0; i >>= 1) {
    digitalWrite(_DAT, c & i);
    digitalWrite(_CLK, HIGH);
    delayMicroseconds(1);
    digitalWrite(_CLK, LOW);
    delayMicroseconds(1);
  }
  digitalWrite(_DAT, LOW);
  digitalWrite(_CLK, HIGH);
  delayMicroseconds(1);
  digitalWrite(_CLK, LOW);
  delayMicroseconds(1);
}

