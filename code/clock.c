#include <16f887.h>
#fuses INTRC_IO
#use delay(clock=8000000)   // 8 MHz internal oscillator, chu ky may 0.5us
#use i2c(master, sda=PIN_C4, scl=PIN_C3, slow)

#define SER PIN_D0 // Data input pin of 74HC595
#define SCK PIN_D2 // Clock pin of 74HC595
#define RCK PIN_D4 // Latch pin of 74HC595
#define MOD PIN_B0
#define INC PIN_B7
#define LED0 PIN_A0
#define LED1 PIN_A1
#define LED2 PIN_A2
#define LED3 PIN_A3
#define LED4 PIN_A4
#define LED5 PIN_A5
#define LED6 PIN_A6
#define LED7 PIN_A7
#define S_COI PIN_D6

unsigned int8 second, minute, hour, date, month, year, day;
// Ma led 7 doan common anode tu 0 den 9, common anode = 0 la sang
const unsigned int8 MaLed7Doan[10] = {0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82, 0xF8, 0x80, 0x90};

const unsigned int16 TIME_DELAY = 30; // so lan chay ham HienThiVal
                                      // chay ham HienThiVal_With_Delay voi delay_time = 30 se cho thoi gian hien thi la: 0.24 seconds
const unsigned int8 NUMBER = 1; // thoi gian hien thi = NUMBER * 0.24 seconds = 0.24 seconds
const unsigned int8 DichCot[16] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};
unsigned int8 ValHienThi[16] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
unsigned int8 vmsg_pattern[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // ex:________
                                0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // Placeholder for second/minute/hour, ex:04.02.2026__
                                0x87, 0x8b, 0xe3, 0xff, 0xc6, 0xc8, 0xff, 0xff, // ex:thu_04__
                                0x87, 0xaf, 0xa3, 0xab, 0xff, 0xa1, 0xfb, 0xab, 0x8b, 0xff. // ex:tran_dinh_
                                0x87, 0x8b, 0xe3, 0xa3, 0xab, 0x90, // ex:thuong
                                0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}; // ex:________
                                                                                 // ________04.02.2026__thu_04__tran_dinh_thuong________
unsigned int8 pos = 0;
int1 flag_coi = 0;
void xuat_1byte(unsigned int8 bytexuat);
void xuat_4byte_latch(unsigned int32 xuat4byte);
void HienThiVal(unsigned int8 Val[16]);
void HienThiVal_With_Delay(unsigned int8 Val[16], unsigned int16 delay_time);
void HienThiVal_With_Delay_number(unsigned int16 delay_time, unsigned int8 number);
void get_data();
void Convert_BCD_to_Decimal();
void Convert_Decimal_to_BCD();
void ds1307_write(unsigned int8 address, unsigned int8 data);
void Adjust_Time_Calendar();
void bao_thoi_gian();
void setup_Time_Calendar();
void latch();

void xuat_1byte(unsigned int8 bytexuat)
{

    unsigned int8 i;

    for (i = 0; i < 8; i++)
    {
        output_bit(SER, (bytexuat & 0x80) ? 1 : 0); // Ghi bit MSB truoc, xuat tu trai sang phai
        output_low(SCK);
        delay_us(3); // delay nho de dam bao xung du dai tren phan cung, co the bo phan nay
        output_high(SCK);
        bytexuat = bytexuat << 1;
    }
}

void latch()
{
    output_low(RCK); // Chốt dữ liệu
    delay_us(3);
    output_high(RCK); // Chốt dữ liệu
}

void xuat_4byte_latch(unsigned int32 xuat4byte)
{
    unsigned int8 byte3, byte2, byte1, byte0;
    
    byte0 = (unsigned int8)(xuat4byte & 0x000000FF);
    byte1 = (unsigned int8)((xuat4byte >> 8) & 0x000000FF);
    byte2 = (unsigned int8)((xuat4byte >> 16) & 0x000000FF);
    byte3 = (unsigned int8)((xuat4byte >> 24) & 0x000000FF);

    xuat_1byte(byte3); // Xuat byte cao nhat truoc, xuat tu trai sang phai
    xuat_1byte(byte2);
    xuat_1byte(byte1);
    xuat_1byte(byte0);
    
    latch();
}

void HienThiVal(unsigned int8 Val[16]) // moi lan chay ham mat 8ms
{
    unsigned int32 value;
    for (int8 i = 0; i < 16; i++)
    {
        if (i < 8)
            value = (unsigned int32)Val[i] << 24 | (unsigned int32)DichCot[i] << 16 & (unsigned int32)0xffffff00; 
            // Dich cot tu trai sang phai, bit 24-31 la gia tri led 7 doan, bit 16-23 la dich cot, bit 0-15 la 0
        else
            value = (unsigned int32)Val[i] << 8 | (unsigned int32)DichCot[i] & (unsigned int32)0xff00ffff;
            // Dich cot tu trai sang phai, bit 8-15 la gia tri led 7 doan, bit 0-7 la dich cot, bit 16-31 la 0
        xuat_4byte_latch(value);
        delay_us(500);
    }
}

void HienThiVal_With_Delay(unsigned int8 Val[16], unsigned int16 delay_time) // delay_time so lan chay ham HienThiVal
////////////////////////////////////////  USE  ////////////////////////////////////////
//                                                                                   //
//        time = 0.5; // Seconds to display                                          //
//        delay_time = (time * 1000 / 8); // 8 = 16 * 0.5, 8 ms                      //
//        delay_time = 63 lan                                                        //
//                                                                                   //
//////////////////////////////////////  END USE  //////////////////////////////////////
{
    while (delay_time-- > 0)
    {
        HienThiVal(Val);
    }
}

void HienThiVal_With_Delay_number(unsigned int16 delay_time, unsigned int8 number)
{
    while (number > 0)
    {
        unsigned int8 i;
        get_data();
        for (i = 0; i < 8; i++)
        {
            ValHienThi[i] = vmsg_pattern[pos + 7 - i];
        }
        HienThiVal_With_Delay(ValHienThi, delay_time);
        if (flag_coi) bao_thoi_gian();
        Adjust_Time_Calendar();
        number--;
    } 
}

void bao_thoi_gian()
{
    get_data();
    Convert_BCD_to_Decimal();
    if ((second == 0 || second == 1 || second == 2 || second == 3) && (minute == 0) && (hour == 12 || hour == 18))
    {
        for (int8 i = 0; i < 6; i++)
        {
            output_high(LED0);
            output_high(LED1);
            output_high(LED2);
            output_high(LED3);
            output_high(LED4);
            output_high(LED5);
            output_high(LED6);
            output_high(LED7);
            output_high(S_COI);
            delay_ms(500);
            output_low(LED0);
            output_low(LED1);
            output_low(LED2);
            output_low(LED3);
            output_low(LED4);
            output_low(LED5);
            output_low(LED6);
            output_low(LED7);
            output_low(S_COI);
            delay_ms(500);
        }
    }
    if ((second == 0 || second == 1 || second == 2 || second == 3) && (minute == 0) && ((hour >= 6 && hour <= 11) || (hour >= 13 && hour <= 17)))
    {
        for (int8 i = 0; i < 3; i++)
        {
            output_high(LED0);
            output_high(LED1);
            output_high(LED2);
            output_high(LED3);
            output_high(LED4);
            output_high(LED5);
            output_high(LED6);
            output_high(LED7);
            output_high(S_COI);
            delay_ms(500);
            output_low(LED0);
            output_low(LED1);
            output_low(LED2);
            output_low(LED3);
            output_low(LED4);
            output_low(LED5);
            output_low(LED6);
            output_low(LED7);
            output_low(S_COI);
            delay_ms(500);
        }
    }
    Convert_Decimal_to_BCD();
    output_high(LED0);
    output_high(LED1);
    output_high(LED2);
    output_high(LED3);
    output_high(LED4);
    output_high(LED5);
    output_high(LED6);
    output_high(LED7);
}

void get_data()
{
    i2c_start();
    i2c_write(0xD0);
    i2c_write(0x00);
    i2c_start();
    i2c_write(0xD1);
    second = i2c_read(1);
    minute = i2c_read(1);
    hour   = i2c_read(1);
    day    = i2c_read(1); // day of week, 1 = Sunday, 2 = Monday, ..., 7 = Saturday
    date   = i2c_read(1);
    month  = i2c_read(1);
    year   = i2c_read(0);
    i2c_stop();

    vmsg_pattern[8] = MaLed7Doan[date / 16];
    vmsg_pattern[9] = MaLed7Doan[date % 16] & 0x7f; // dot
    vmsg_pattern[10] = MaLed7Doan[month / 16];
    vmsg_pattern[11] = MaLed7Doan[month % 16] & 0x7f; // dot
    vmsg_pattern[12] = MaLed7Doan[2];
    vmsg_pattern[13] = MaLed7Doan[0];
    vmsg_pattern[14] = MaLed7Doan[year / 16];
    vmsg_pattern[15] = MaLed7Doan[year % 16];
    if (day == 1) // cn
    {
        vmsg_pattern[22] = 0xc6;
        vmsg_pattern[23] = 0xc8;
    }
    else
    {
        vmsg_pattern[22] = MaLed7Doan[0];
        vmsg_pattern[23] = MaLed7Doan[day];
    }

    ValHienThi[8] = MaLed7Doan[second % 16];
    ValHienThi[9] = MaLed7Doan[second / 16];
    ValHienThi[10] = MaLed7Doan[minute % 16] & 0x7f; // dot
    ValHienThi[11] = MaLed7Doan[minute / 16];
    ValHienThi[12] = MaLed7Doan[hour % 16] & 0x7f; // dot
    ValHienThi[13] = MaLed7Doan[hour / 16];
    ValHienThi[14] = 0xFF;
    ValHienThi[15] = 0xFF;
}

void Convert_BCD_to_Decimal()
{
    second = (second >> 4) * 10 + (second & 0x0F);
    minute = (minute >> 4) * 10 + (minute & 0x0F);
    hour   = (hour >> 4) * 10 + (hour & 0x0F);
    date   = (date >> 4) * 10 + (date & 0x0F);
    month  = (month >> 4) * 10 + (month & 0x0F);
    year   = (year >> 4) * 10 + (year & 0x0F);
}

void Convert_Decimal_to_BCD()
{
    // x = (x / 10) << 4 + (x % 10)
    second = ((second / 10) << 4) + (second % 10);
    minute = ((minute / 10) << 4) + (minute % 10);
    hour   = ((hour / 10) << 4) + (hour % 10);
    date   = ((date / 10) << 4) + (date % 10);
    month  = ((month / 10) << 4) + (month % 10);
    year   = ((year / 10) << 4) + (year % 10);
}

void ds1307_write(unsigned int8 address, unsigned int8 data)
{
    i2c_start();
    i2c_write(0xD0);
    i2c_write(address);
    i2c_write(data);
    i2c_stop();
}

// Hàm điều chỉnh thời gian tự động
// Nhấn INC để vào chế độ điều chỉnh
// Giá trị thiết lập được lưu vào bộ nhớ EEPROM của pic 16f887
// Nếu thời gian nhanh hơn thực tế thì giá trị mang giá trị âm, ngược lại nếu thời gian chậm hơn thực tế thì giá trị mang giá trị dương
void save_Time_Offset_to_EEPROM() {
    signed int8 time_offset = 0;
    signed int8 valueee = 10;
    if (!input(INC)) {
        while (TRUE) {
            if (!input(MOD)) valueee++;
            if (valueee >= 59) valueee = 0;
            time_offset = valueee - 30; // offset from -30 to +29
            // Display offset value
            ValHienThi[14] = 0xFF;
            ValHienThi[15] = 0xFF;
            HienThiVal_With_Delay(ValHienThi, TIME_DELAY);
            if (time_offset < 0) {
                ValHienThi[14] = MaLed7Doan[(-time_offset) % 16] & 0x7f; // dot
                ValHienThi[15] = MaLed7Doan[(-time_offset) / 16];
            } else {
                ValHienThi[14] = MaLed7Doan[time_offset % 16] & 0x7f; // dot
                ValHienThi[15] = MaLed7Doan[time_offset / 16];
            }
            HienThiVal_With_Delay(ValHienThi, TIME_DELAY);
            if (!input(INC)) {
                delay_ms(20);
                if (!input(INC)) {
                    while (input(INC) == 0) {}
                }
                break;
            }
            
        }
    }
}

void Adjust_Time_Calendar()
{
    if (!input(MOD))
        {
            Convert_BCD_to_Decimal();
            // Set seconds to 00
            second = 0;

            // Adjust minutes
            while (TRUE)
            {
                if (!input(INC)) minute++;
                if (minute > 59) minute = 0;
                Convert_Decimal_to_BCD();
                ValHienThi[10] = 0xFF;
                ValHienThi[11] = 0xFF;
                HienThiVal_With_Delay(ValHienThi, TIME_DELAY);
                ValHienThi[10] = MaLed7Doan[minute % 16] & 0x7f; // dot
                ValHienThi[11] = MaLed7Doan[minute / 16];
                HienThiVal_With_Delay(ValHienThi, TIME_DELAY);
                Convert_BCD_to_Decimal();
                if (!input(MOD))
                {
                    delay_ms(20);
                    if (!input(MOD))
                    {
                        while (input(MOD) == 0) {}
                    }
                    break;
                }
            }

            // Adjust hours
            while (TRUE)
            {
                if (!input(INC)) hour++;
                if (hour > 23) hour = 0;
                Convert_Decimal_to_BCD();
                ValHienThi[12] = 0xFF;
                ValHienThi[13] = 0xFF;
                HienThiVal_With_Delay(ValHienThi, TIME_DELAY);
                ValHienThi[12] = MaLed7Doan[hour % 16] & 0x7f; // dot
                ValHienThi[13] = MaLed7Doan[hour / 16];
                HienThiVal_With_Delay(ValHienThi, TIME_DELAY);
                Convert_BCD_to_Decimal();
                if (!input(MOD))
                {
                    delay_ms(20);
                    if (!input(MOD))
                    {
                        while (input(MOD) == 0) {}
                    }
                    break;
                }
            }

            // Adjust date
            while (TRUE)
            {
                if (!input(INC)) date++;
                if (date > 31) date = 1;
                Convert_Decimal_to_BCD();
                ValHienThi[6] = 0xFF;
                ValHienThi[7] = 0xFF;
                HienThiVal_With_Delay(ValHienThi, TIME_DELAY);
                ValHienThi[6] = MaLed7Doan[date % 16] & 0x7f; // dot
                ValHienThi[7] = MaLed7Doan[date / 16];
                HienThiVal_With_Delay(ValHienThi, TIME_DELAY);
                Convert_BCD_to_Decimal();
                if (!input(MOD))
                {
                    delay_ms(20);
                    if (!input(MOD))
                    {
                        while (input(MOD) == 0) {}
                    }
                    break;
                }
            }

            // Adjust month
            while (TRUE)
            {
                if (!input(INC)) month++;
                if (month > 12) month = 1;
                Convert_Decimal_to_BCD();
                ValHienThi[4] = 0xFF;
                ValHienThi[5] = 0xFF;
                HienThiVal_With_Delay(ValHienThi, TIME_DELAY);
                ValHienThi[4] = MaLed7Doan[month % 16] & 0x7f; // dot
                ValHienThi[5] = MaLed7Doan[month / 16];
                HienThiVal_With_Delay(ValHienThi, TIME_DELAY);
                Convert_BCD_to_Decimal();
                if (!input(MOD))
                {
                    delay_ms(20);
                    if (!input(MOD))
                    {
                        while (input(MOD) == 0) {}
                    }
                    break;
                }
            }

            // Adjust year
            while (TRUE)
            {
                if (!input(INC)) year++;
                if (year > 50) year = 0;
                Convert_Decimal_to_BCD();
                ValHienThi[0] = 0xFF;
                ValHienThi[1] = 0xFF;
                HienThiVal_With_Delay(ValHienThi, TIME_DELAY);
                ValHienThi[0] = MaLed7Doan[year % 16];
                ValHienThi[1] = MaLed7Doan[year / 16];
                HienThiVal_With_Delay(ValHienThi, TIME_DELAY);
                Convert_BCD_to_Decimal();
                if (!input(MOD))
                {
                    delay_ms(20);
                    if (!input(MOD))
                    {
                        while (input(MOD) == 0) {}
                    }
                    break;
                }
            }

            // Adjust day of week
            while (TRUE)
            {
                if (!input(INC)) day++;
                if (day > 7) day = 1;
                Convert_Decimal_to_BCD();
                ValHienThi[2] = 0xFF;
                ValHienThi[3] = 0xFF;
                HienThiVal_With_Delay(ValHienThi, TIME_DELAY);
                if (day == 0)
                {
                    ValHienThi[2] = 0xc8;
                    ValHienThi[3] = 0xc6;
                }
                else
                {
                    ValHienThi[3] = MaLed7Doan[0];
                    ValHienThi[2] = MaLed7Doan[day];
                }
                HienThiVal_With_Delay(ValHienThi, TIME_DELAY);
                Convert_BCD_to_Decimal();
                if (!input(MOD))
                {
                    delay_ms(20);
                    if (!input(MOD))
                    {
                        while (input(MOD) == 0) {}
                    }
                    break;
                }
            }

            Convert_Decimal_to_BCD();
            // Write time and calendar to DS1307
            ds1307_write(0x00, second);
            ds1307_write(0x01, minute);
            ds1307_write(0x02, hour);
            ds1307_write(0x03, day);
            ds1307_write(0x04, date);
            ds1307_write(0x05, month);
            ds1307_write(0x06, year);
            delay_ms(200);
        }
}

// Thiết lập thời gian và lịch ban đầu cho DS1307, thời gian bắt đầu từ 08:00:00, ngày 04/02/2026, Thứ 4
void setup_Time_Calendar()
{
    second = 0x00;
    minute = 0x00;
    hour   = 0x08;
    day    = 0x04; // Thứ 4
    date   = 0x04;
    month  = 0x02;
    year   = 0x26;

    ds1307_write(0x00, second);
    ds1307_write(0x01, minute);
    ds1307_write(0x02, hour);
    ds1307_write(0x03, day);
    ds1307_write(0x04, date);
    ds1307_write(0x05, month);
    ds1307_write(0x06, year);
}

void main()
{
    set_tris_a(0x00);
    set_tris_b(0xFF);
    set_tris_d(0x00);

    output_low(SER);
    output_low(SCK);
    output_high(RCK);  // mac dinh high

    output_high(LED0);
    output_low(LED1);
    output_high(LED2);
    output_low(LED3);
    output_high(LED4);
    output_low(LED5);
    output_high(LED6);
    output_high(LED7);
    output_low(S_COI);

    setup_Time_Calendar();
    // Chờ bộ dao động ổn định
    delay_ms(20000);
    setup_Time_Calendar();

    while (TRUE)
    {
        HienThiVal_With_Delay_number(TIME_DELAY, NUMBER);
        pos++;
        if (pos > (sizeof(vmsg_pattern) - 8))
        {
            pos = 0;
        }
    }
}
