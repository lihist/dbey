// Ported from CCS PIC C to MPLAB X + XC8 for PIC18F45K20
// Fosc = 16MHz (internal)

// ================== CONFIG BITS ==================
#include <xc.h>
#include <stdint.h>

#pragma config FOSC = INTIO67   // Internal oscillator block, port function on RA6 and RA7
#pragma config FCMEN = OFF      // Fail-Safe Clock Monitor
#pragma config IESO = OFF       // Oscillator Switchover mode
#pragma config PWRTEN = OFF     // Power-up Timer Enable
#pragma config BOREN = OFF      // Brown-out Reset disabled
#pragma config WDTEN = OFF      // Watchdog Timer disabled

#define _XTAL_FREQ 16000000UL

// ================== TYPE ALIASES ==================
typedef uint8_t  int8;
typedef uint16_t int16;
typedef uint32_t int32;
typedef uint8_t  int1;  // used as boolean

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

// ================== PIN DEFINITIONS ==================
// NOTE: Inputs use PORTx, outputs use LATx

// --- Port A ---
#define BRANCH4     PORTAbits.RA0
#define BRANCH5     PORTAbits.RA1
#define BRANCH6     PORTAbits.RA2
#define BRANCH7     PORTAbits.RA3
#define BRANCH8     PORTAbits.RA4
#define BRANCH9     PORTAbits.RA5
#define LED2        LATAbits.LATA6
#define LED1        LATAbits.LATA7

// --- Port B ---
#define COL3        PORTBbits.RB0
#define ROW1        LATBbits.LATB1
#define ROW2        LATBbits.LATB2
#define ROW3        LATBbits.LATB3
#define ROW4        LATBbits.LATB4
#define BRANCH2     PORTBbits.RB6
#define BRANCH1     PORTBbits.RB7

// --- Port C ---
#define LED3        LATCbits.LATC0
#define LED4        LATCbits.LATC1
#define LED5        LATCbits.LATC2
#define LED6        LATCbits.LATC3
#define RELAY       LATCbits.LATC7

// --- Port D ---
#define LED7        LATDbits.LATD0
#define LED8        LATDbits.LATD1
#define LED9        LATDbits.LATD2
#define LED10       LATDbits.LATD3

#define RED_LED     LATDbits.LATD4
#define GREEN_LED   LATDbits.LATD5

#define COL1        PORTDbits.RD6
#define COL2        PORTDbits.RD7

// --- Port E ---
#define BRANCH10    PORTEbits.RE0
#define BUZZER2     LATEbits.LATE1
#define BUZZER1     LATEbits.LATE2
#define BRANCH3     PORTEbits.RE3

#define BRANCH_COUNT 10

// ================== HELPER MACROS ==================
#define output_high(pin) do { (pin) = 1; } while(0)
#define output_low(pin)  do { (pin) = 0; } while(0)
#define input_pin(pin)   (pin)

#define delay_ms(x) __delay_ms(x)
#define delay_us(x) __delay_us(x)

// make8 / make32 equivalents from CCS
#define make8(var, byte)  ( (uint8_t)(((uint32_t)(var) >> (8U * (byte))) & 0xFFU) )

static inline uint32_t make32(uint8_t b3, uint8_t b2, uint8_t b1, uint8_t b0)
{
    uint32_t v = 0;
    v |= ((uint32_t)b0);
    v |= ((uint32_t)b1 << 8);
    v |= ((uint32_t)b2 << 16);
    v |= ((uint32_t)b3 << 24);
    return v;
}

// ================== EEPROM FUNCTIONS ==================
uint8_t read_eeprom(uint8_t addr)
{
    EEADR = addr;
    EECON1bits.EEPGD = 0; // Access Data EEPROM
    EECON1bits.CFGS  = 0; // Access EEPROM not config
    EECON1bits.RD    = 1; // Trigger read
    return EEDATA;
}

void write_eeprom(uint8_t addr, uint8_t value)
{
    uint8_t gie_state = INTCONbits.GIE;

    EEADR = addr;
    EEDATA = value;
    EECON1bits.EEPGD = 0;  // Data EEPROM
    EECON1bits.CFGS  = 0;  // Not config
    EECON1bits.WREN  = 1;  // Enable write

    INTCONbits.GIE = 0;    // Disable global interrupts during unlock
    EECON2 = 0x55;
    EECON2 = 0xAA;
    EECON1bits.WR = 1;     // Begin write
    while (EECON1bits.WR); // Wait until done
    EECON1bits.WREN = 0;   // Disable write

    INTCONbits.GIE = gie_state; // Restore GIE
}

// ================== GLOBAL VARIABLES ==================

// Password management
int32 master_password = 123456;     // Default master password
int32 multiplier       = 1;         // Position multiplier
int32 entered_password = 0;         // Entered so far
int32 final_password   = 0;         // Final 6-digit password

// System state
int8  password_digit_pos = 1;       // 1..6
int8  entered_digit      = 12;      // 12 = no key
int8  user_number;                  // user index (1..30)
int8  menu_state   = 0;
int8  buzzer_duration = 30;

// Loop counters / status
int8  i, a = 0;
int8  user_mismatch = 1;
int8  last_data;
int8  read_data[120];
int8  user_index;
int8  data_start;
int8  timeout_counter = 0;

// Flags
int1  relay_status       = 0; // 0/1
int1  star_pressed       = 0;
int1  master_pass_select = 0;
int1  delete_user        = 0;

// Users
int32 users[30];
int32 temp_password = 0;

// Keypad debounce
int8  prev_key_state = 0xFF;
int8  key_counter    = 0;
int8  confirmed_key  = 0xFF;
int1  key_processed  = 0;

// ================== FORWARD DECLARATIONS ==================
void return_main_menu(void);
void correct_feedback(void);
void wrong_feedback(void);
void eeprom_write_all(void);
void eeprom_read_all(void);
void memory_check(void);
void check_password(void);
void keypad(void);
void check_master_password(void);
void check_user_number(void);
void check_user_for_delete(void);
void set_new_password(void);
void confirm_new_password(void);

// Helper to set Timer0 preload value like CCS set_timer0(7710)
static inline void set_timer0(uint16_t value)
{
    TMR0H = (uint8_t)(value >> 8);
    TMR0L = (uint8_t)(value & 0xFF);
}

// ================== BEEP ==================
void beep_sound(void)
{
    entered_password = (entered_digit * multiplier) + entered_password;
    output_high(BUZZER2);
    delay_ms(buzzer_duration);
    output_low(BUZZER2);
    delay_ms(150);
}

// ================== FEEDBACKS ==================
void correct_feedback(void)
{
    for(i = 0; i < 4; i++)
    {
        output_low(RED_LED);
        output_low(GREEN_LED);
        output_high(BUZZER2);
        delay_ms(250);
        output_high(GREEN_LED);
        output_low(BUZZER2);
        delay_ms(250);
    }
    output_high(RED_LED);
    output_high(GREEN_LED);
    password_digit_pos = 1;
    entered_password   = 0;
    entered_digit      = 12;
}

void wrong_feedback(void)
{
    output_high(RED_LED);
    output_low(GREEN_LED);
    output_high(BUZZER2);
    delay_ms(1000);
    output_low(RED_LED);
    output_low(BUZZER2);

    password_digit_pos = 1;
    entered_password   = 0;
    entered_digit      = 12;
}

// ================== EEPROM ALL WRITE ==================
void eeprom_write_all(void)
{
    // up to 30 users
    data_start = 0;
    for (user_index = 0; user_index < 30; user_index++)
    {
        data_start = user_index * 4;

        read_data[data_start]     = make8(users[user_index], 0);
        write_eeprom(data_start,     read_data[data_start]);

        read_data[data_start + 1] = make8(users[user_index], 1);
        write_eeprom(data_start + 1, read_data[data_start + 1]);

        read_data[data_start + 2] = make8(users[user_index], 2);
        write_eeprom(data_start + 2, read_data[data_start + 2]);

        read_data[data_start + 3] = make8(users[user_index], 3);
        write_eeprom(data_start + 3, read_data[data_start + 3]);
    }

    // master password at 120..123
    read_data[0] = make8(master_password, 0);
    read_data[1] = make8(master_password, 1);
    read_data[2] = make8(master_password, 2);
    read_data[3] = make8(master_password, 3);
    write_eeprom(120, read_data[0]);
    write_eeprom(121, read_data[1]);
    write_eeprom(122, read_data[2]);
    write_eeprom(123, read_data[3]);
}

// ================== EEPROM ALL READ ==================
void eeprom_read_all(void)
{
    data_start = 0;
    // users
    for (user_index = 0; user_index < 30; user_index++)
    {
        data_start = user_index * 4;
        for (i = 0; i < 4; i++)
        {
            read_data[data_start + i] = read_eeprom(data_start + i);
        }

        users[user_index] = make32(read_data[data_start + 3],
                                   read_data[data_start + 2],
                                   read_data[data_start + 1],
                                   read_data[data_start + 0]);
    }

    // master password 120..123
    for (i = 0; i < 4; i++)
    {
        read_data[i] = read_eeprom(120 + i);
    }
    master_password = make32(read_data[3], read_data[2], read_data[1], read_data[0]);
}

// ================== MEMORY CHECK ==================
void memory_check(void)
{
    last_data = read_eeprom(255);
    if(last_data == 0)
    {
        // already initialized
        eeprom_read_all();
    }
    else
    {
        // first time
        write_eeprom(255, 0);

        read_data[0] = make8(master_password, 0);
        read_data[1] = make8(master_password, 1);
        read_data[2] = make8(master_password, 2);
        read_data[3] = make8(master_password, 3);
        write_eeprom(120, read_data[0]);
        write_eeprom(121, read_data[1]);
        write_eeprom(122, read_data[2]);
        write_eeprom(123, read_data[3]);
    }
}

// ================== PASSWORD CHECK ==================
void check_password(void)
{
    user_mismatch = 1;

    for(a = 0; a < 30; a++)
    {
        if(final_password == users[a] || final_password == master_password)
        {
            user_mismatch = 0;
            // success feedback
            for(i = 0; i < 4; i++)
            {
                output_low(RED_LED);
                output_low(GREEN_LED);
                output_high(BUZZER2);
                delay_ms(250);
                output_high(GREEN_LED);
                output_low(BUZZER2);
                delay_ms(250);
            }
            final_password = 0;
            // CCS'de int1 olduğu için ++ ile toggle oluyordu; burada XOR ile toggle
            relay_status ^= 1;
            break;
        }
    }

    if (user_mismatch)
    {
        output_high(RED_LED);
        output_high(BUZZER2);
        delay_ms(1000);
        output_low(RED_LED);
        output_low(BUZZER2);
    }

    password_digit_pos = 1;
    entered_password   = 0;
    entered_digit      = 12;
}

// ================== RETURN MAIN MENU ==================
void return_main_menu(void)
{
    output_low(GREEN_LED);
    output_low(RED_LED);
    user_number        = 99;
    star_pressed       = 0;
    entered_digit      = 12;
    master_pass_select = 0;
    menu_state         = 0;

    // Timer0 off
    T0CONbits.TMR0ON   = 0;
    INTCONbits.TMR0IE  = 0;
    INTCONbits.TMR0IF  = 0;
}

// ================== KEYPAD SCAN + DEBOUNCE ==================
void keypad(void)
{
    int8 current_key = 0xFF;

    // Row1
    output_low(ROW1);
    output_high(ROW2);
    output_high(ROW3);
    output_high(ROW4);
    delay_us(10);

    if (!input_pin(COL1))       current_key = 1;
    else if (!input_pin(COL2))  current_key = 2;
    else if (!input_pin(COL3))  current_key = 3;

    // Row2
    if (current_key == 0xFF)
    {
        output_high(ROW1);
        output_low(ROW2);
        output_high(ROW3);
        output_high(ROW4);
        delay_us(10);

        if (!input_pin(COL1))       current_key = 4;
        else if (!input_pin(COL2))  current_key = 5;
        else if (!input_pin(COL3))  current_key = 6;
    }

    // Row3
    if (current_key == 0xFF)
    {
        output_high(ROW1);
        output_high(ROW2);
        output_low(ROW3);
        output_high(ROW4);
        delay_us(10);

        if (!input_pin(COL1))       current_key = 7;
        else if (!input_pin(COL2))  current_key = 8;
        else if (!input_pin(COL3))  current_key = 9;
    }

    // Row4
    if (current_key == 0xFF)
    {
        output_high(ROW1);
        output_high(ROW2);
        output_high(ROW3);
        output_low(ROW4);
        delay_us(10);

        if (!input_pin(COL1))       current_key = 10; // *
        else if (!input_pin(COL2))  current_key = 0;
        else if (!input_pin(COL3))  current_key = 11; // #
    }

    // all rows high
    output_high(ROW1);
    output_high(ROW2);
    output_high(ROW3);
    output_high(ROW4);

    // Debounce
    if (current_key == prev_key_state)
    {
        if (current_key != 0xFF)
        {
            key_counter++;
            if (key_counter >= 3)
            {
                key_counter = 3;
                if (confirmed_key != current_key)
                {
                    confirmed_key = current_key;
                    key_processed = 0;
                }
            }
        }
        else
        {
            key_counter   = 0;
            confirmed_key = 0xFF;
            key_processed = 0;
        }
    }
    else
    {
        key_counter    = 0;
        prev_key_state = current_key;
    }

    if (confirmed_key != 0xFF && !key_processed)
    {
        key_processed   = 1;
        entered_digit   = confirmed_key;
        timeout_counter = 0;

        switch(confirmed_key)
        {
            case 10: // *
                final_password    = 0;
                entered_password  = 0;
                password_digit_pos = 1;
                output_high(BUZZER2);
                delay_ms(buzzer_duration);
                output_low(BUZZER2);
                break;

            case 11: // #
                // işlem main loop'ta
                break;

            default: // 0..9
                password_digit_pos++;
                beep_sound();
                break;
        }
    }
}

// ================== MASTER PASSWORD CHECK ==================
void check_master_password(void)
{
    if(entered_digit == 11)
    {
        if(master_password == final_password)
        {
            correct_feedback();
            user_number     = 90;
            final_password  = 0;
            entered_password = 0;
            menu_state      = 2;
        }
        else
        {
            wrong_feedback();
            star_pressed       = 0;
            menu_state         = 0;
            master_pass_select = 0;
        }
    }

    if(entered_digit == 10)
        return_main_menu();
}

// ================== USER NUMBER CHECK ==================
void check_user_number(void)
{
    if(password_digit_pos == 3)
    {
        final_password   = entered_password;
        password_digit_pos = 1;
        entered_password = 0;
    }

    if(entered_digit == 11)
    {
        int hundred_thousands = (final_password / 100000L) % 10L;
        int ten_thousands     = (final_password / 10000L)  % 10L;
        user_number = (int8)(hundred_thousands * 10 + ten_thousands);

        if(user_number > 0 && user_number <= 30)
        {
            correct_feedback();
            final_password = 0;
            menu_state     = 3;
        }
        else if(user_number == 98)
        {
            correct_feedback();
            menu_state = 6;
        }
        else if(user_number == 99)
        {
            correct_feedback();
            master_pass_select = 1;
            final_password     = 0;
            menu_state         = 3;
        }
        else
        {
            wrong_feedback();
            star_pressed       = 0;
            user_number        = 98;
            menu_state         = 0;
            master_pass_select = 0;
            final_password     = 0;
        }
    }

    if(entered_digit == 10)
        return_main_menu();
}

// ================== USER NUMBER CHECK FOR DELETE ==================
void check_user_for_delete(void)
{
    if(password_digit_pos == 3)
    {
        final_password   = entered_password;
        password_digit_pos = 1;
        entered_password = 0;
    }

    if(entered_digit == 11)
    {
        int hundred_thousands = (final_password / 100000L) % 10L;
        int ten_thousands     = (final_password / 10000L)  % 10L;
        user_number = (int8)(hundred_thousands * 10 + ten_thousands);

        if(user_number > 0 && user_number <= 30)
        {
            correct_feedback();
            delete_user = 1;
        }
        else
        {
            wrong_feedback();
            star_pressed       = 0;
            user_number        = 90;
            menu_state         = 0;
            master_pass_select = 0;
        }
    }

    if(entered_digit == 10)
        return_main_menu();
}

// ================== NEW PASSWORD FIRST ENTRY ==================
void set_new_password(void)
{
    if(entered_digit == 11 && final_password != 0)
    {
        temp_password      = final_password;
        star_pressed       = 0;
        correct_feedback();
        final_password     = 0;
        menu_state         = 4;
    }
    else
    {
        wrong_feedback();
        star_pressed       = 0;
        user_number        = 98;
        menu_state         = 0;
        master_pass_select = 0;
        final_password     = 0;
    }

    if(entered_digit == 10)
        return_main_menu();
}

// ================== NEW PASSWORD CONFIRM ==================
void confirm_new_password(void)
{
    if(entered_digit == 11)
    {
        if(temp_password == final_password)
        {
            if(master_pass_select == 0)
            {
                // user password
                users[user_number - 1] = final_password;
                star_pressed           = 0;
                menu_state             = 0;
                eeprom_write_all();
                correct_feedback();
                final_password         = 0;
                output_low(RED_LED);
                output_low(GREEN_LED);
            }
            else
            {
                // master password
                master_password    = final_password;
                master_pass_select = 0;
            }
        }
        else
        {
            final_password = 0;
            menu_state     = 3;
            wrong_feedback();
        }
    }

    if(entered_digit == 10)
        return_main_menu();
}

// ================== TIMER0 INTERRUPT ==================
void __interrupt() isr(void)
{
    if (INTCONbits.TMR0IF && INTCONbits.TMR0IE)
    {
        timeout_counter++;
        if(timeout_counter == 17)  // ~ 1 minutes
        {
            timeout_counter = 0;
            return_main_menu();
        }
        set_timer0(7710);
        INTCONbits.TMR0IF = 0;
    }
}

// ================== MAIN ==================
void main(void)
{
    // OSCCON: 16MHz internal
    OSCCONbits.IRCF = 0b111;  // 16 MHz
    OSCCONbits.SCS  = 0b10;   // Internal oscillator block

    ANSEL = 0x00; ANSELH = 0x00;/* Disable all analog, use digital I/O
    ANSELA = 0x00;
    ANSELB = 0x00;
    ANSELC = 0x00;
    ANSELD = 0x00;
    ANSELE = 0x00;
    //*/
    // Timer0 configuration (16-bit, Fosc/4, prescaler 1:256)
    T0CONbits.T08BIT = 0;     // 16-bit
    T0CONbits.T0CS   = 0;     // Internal clock
    T0CONbits.PSA    = 0;     // Prescaler on
    T0CONbits.T0PS   = 0b111; // 1:256
    set_timer0(7710);
    INTCONbits.TMR0IF = 0;
    INTCONbits.TMR0IE = 0;    // Initially disabled
    
    // Global interrupts
    INTCONbits.GIE   = 1;
    INTCONbits.PEIE  = 1;

    // TRIS configuration (copied from CCS)
    TRISA = 0b00111111;  // A0-A5 inputs, A6-A7 outputs
    TRISB = 0b11100001;  // B0,B6,B7 inputs; B1-B5 outputs
    TRISC = 0b01110000;  // C4-C6 inputs; C0-C3,C7 outputs
    TRISD = 0b11000000;  // D6-D7 inputs; D0-D5 outputs
    TRISE = 0b11111001;  // E0,E3-E7 inputs; E1-E2 outputs

    // Clear outputs
    output_low(LED1);
    output_low(LED2);
    output_low(LED3);
    output_low(LED4);
    output_low(LED5);
    output_low(LED6);
    output_low(LED7);
    output_low(LED8);
    output_low(LED9);
    output_low(LED10);

    output_low(BUZZER1);
    output_low(BUZZER2);

    output_low(RELAY);

    output_low(RED_LED);
    output_low(GREEN_LED);

    output_low(ROW1);
    output_low(ROW2);
    output_low(ROW3);
    output_low(ROW4);

    // EEPROM init / load
    memory_check();
    // Eğer memory_check sırasında users[] dolmadıysa, varsayılanları burada temizleyebilirsin.

    // Main loop
    while(1)//for(;;)
    {
        // Relay control
        if(relay_status == 1)
        {
            output_high(GREEN_LED);
            output_high(RELAY);
        }
        else
        {
            output_low(GREEN_LED);
            output_low(RELAY);
        }
        GREEN_LED = 1;
        // Keypad
        keypad();
        delay_ms(100);
        GREEN_LED = 0;
        // Normal mode: # (11) ile şifre kontrolü, * (10) yokken
        if (entered_digit == 11 && star_pressed == 0)
        {
            password_digit_pos = 1;
            check_password();
        }

        // Admin menü (*) algılama
        if (entered_digit == 10)
        {
            star_pressed = 1;
            INTCONbits.TMR0IF = 0;
            set_timer0(7710);
            T0CONbits.TMR0ON  = 1;   // Start Timer0
            INTCONbits.TMR0IE = 1;   // Enable Timer0 interrupt
            entered_digit = 12;
        }

        // Admin menu
        while(star_pressed == 1)
        {
            output_high(RED_LED);
            output_high(GREEN_LED);

            menu_state = 1;

            // 1: Master password
            while(menu_state == 1)
            {
                keypad();
                check_master_password();
            }

            // 2: User number select
            while(menu_state == 2)
            {
                keypad();
                check_user_number();
            }

            // 3: New password enter
            while(menu_state == 3)
            {
                keypad();
                if(entered_digit == 11)
                    set_new_password();
            }

            // 4: Confirm new password
            while(menu_state == 4)
            {
                keypad();
                confirm_new_password();
            }

            // 5: Additional confirm (kept for compatibility)
            while(menu_state == 5)
            {
                keypad();
                confirm_new_password();
            }

            // 6: Delete user
            while(menu_state == 6)
            {
                keypad();
                check_user_for_delete();
                if(delete_user == 1)
                {
                    users[user_number - 1] = 0xFFFFFFFFUL; // Mark deleted
                    star_pressed           = 0;
                    menu_state             = 0;
                    eeprom_write_all();
                    final_password         = 0;
                    output_low(RED_LED);
                    output_low(GREEN_LED);
                    delete_user            = 0;
                }
            }

            star_pressed = 0;
        }

        // Password digit handling
        if(password_digit_pos == 7)
        {
            password_digit_pos = 1;
            final_password     = entered_password;
            entered_password   = 0;
        }

        switch(password_digit_pos)
        {
            case 1: multiplier = 100000; break;
            case 2: multiplier = 10000;  break;
            case 3: multiplier = 1000;   break;
            case 4: multiplier = 100;    break;
            case 5: multiplier = 10;     break;
            case 6: multiplier = 1;      break;
            default: break;
        }
    }
}
