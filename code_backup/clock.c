#include <16f887.h>
#fuses INTRC_IO
#use delay(clock=8000000)   // 8 MHz internal oscillator
#use i2c(master, sda=PIN_C4, scl=PIN_C3, slow)

#define SER PIN_D0
#define SCK PIN_D2
#define RCK PIN_D4
#define MOD PIN_B0
#define INC PIN_B2
#define LED0 PIN_A0
#define LED1 PIN_A2
#define LED2 PIN_A4
#define LED3 PIN_A6

unsigned int8 second, minute, hour, date, month, year, day;
// Ma led 7 doan common anode tu 0 den 9
const unsigned int8 MaLed7Doan[10] = {0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82, 0xF8, 0x80, 0x90};

const unsigned int16 TIME_DELAY = 40; // ~0.3 seconds
const unsigned int8 NUMBER = 1; // ~0.3 seconds
const unsigned int8 DichCot[16] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};
unsigned int8 ValHienThi[16] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
unsigned int8 vmsg_pattern[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                0x87, 0x8b, 0xe3, 0xff, 0xc6, 0xc8, 0xff, 0xff,
                                0x87, 0xaf, 0xa3, 0xab, 0xff, 0xa1, 0xfb, 0xab, 0x8b, 0xff,
                                0x87, 0x8b, 0xe3, 0xa3, 0xab, 0x90,
                                0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
unsigned int8 pos = 0;
void xuat_1byte(unsigned int8 bytexuat);
void xuat_4byte(unsigned int32 xuat4byte);
void HienThiVal(unsigned int8 Val[16]);
void HienThiVal_With_Delay(unsigned int8 Val[16], unsigned int16 delay_time);
void HienThiVal_With_Delay_number(unsigned int16 delay_time, unsigned int8 number);
void get_data();
void Convert_BCD_to_Decimal();
void Convert_Decimal_to_BCD();
void ds1307_write(unsigned int8 address, unsigned int8 data);
void Adjust_Time_Calendar();

void xuat_1byte(unsigned int8 bytexuat)
{

    unsigned int8 i;

    for (i = 0; i < 8; i++)
    {
        output_bit(SER, (bytexuat & 0x80) ? 1 : 0);
        output_low(SCK);
        delay_us(1); // delay nho de dam bao xung du dai tren phan cung, co the bo phan nay
        output_high(SCK);
        bytexuat = bytexuat << 1;
    }
}

void xuat_4byte(unsigned int32 xuat4byte)
{
    unsigned int8 byte3, byte2, byte1, byte0;
    
    byte0 = (unsigned int8)(xuat4byte & 0x000000FF);
    byte1 = (unsigned int8)((xuat4byte >> 8) & 0x000000FF);
    byte2 = (unsigned int8)((xuat4byte >> 16) & 0x000000FF);
    byte3 = (unsigned int8)((xuat4byte >> 24) & 0x000000FF);

    xuat_1byte(byte3);
    xuat_1byte(byte2);
    xuat_1byte(byte1);
    xuat_1byte(byte0);
    output_low(RCK); // Chốt dữ liệu
    delay_us(1);
    output_high(RCK); // Chốt dữ liệu
}

void HienThiVal(unsigned int8 Val[16])
{
    unsigned int32 value;
    for (int8 i = 0; i < 16; i++)
    {
        if (i < 8)
            value = (unsigned int32)Val[i] << 24 | (unsigned int32)DichCot[i] << 16 & (unsigned int32)0xffffff00;
        else
            value = (unsigned int32)Val[i] << 8 | (unsigned int32)DichCot[i] & (unsigned int32)0xff00ffff;
        xuat_4byte(value);
        delay_us(500);
    }
}

void HienThiVal_With_Delay(unsigned int8 Val[16], unsigned int16 delay_time)
////////////////////////////////////////  USE  ////////////////////////////////////////
//        unsigned int8  delay_time;                                                 //
//        unsigned int16 cal_delay_time;                                             //
//        delay_time = 0.5; // Seconds to display                                    //
//        cal_delay_time = (unsigned int16)(delay_time * 1000 / 8); // 8 = 16 * 0.5  //
//        HienThiVal_With_Delay(ValHienThi, cal_delay_time);                         //
//        HienThiVal_With_Delay(ValHienThiOff, 2); // 1 ms                           //
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
        Adjust_Time_Calendar();
        number--;
    } 
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
    day    = i2c_read(1);
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
    if (day == 0)
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

void main()
{
    set_tris_a(0x00);
    set_tris_b(0xFF);
    set_tris_d(0x00);

    output_low(SER);
    output_low(SCK);
    output_high(RCK);  // mac dinh high

    output_high(LED0);
    output_high(LED1);
    output_high(LED2);
    output_high(LED3);

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
