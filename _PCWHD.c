/*******************************************************************************
* Security System with Keypad and Multiple User Support
* Microcontroller: PIC18F45K20
* Features:
* - Multiple user password management (up to 30 users)
* - Master password control
* - EEPROM storage for passwords
* - 4x3 Keypad input with debounce
* - LED and buzzer feedback
* - Relay control for door/lock
*******************************************************************************/

#include <18F45K20.h>

#device ADC=16

#FUSES NOWDT                    // No Watch Dog Timer
#FUSES WDT128                   // Watch Dog Timer uses 1:128 Postscale
#FUSES NOBROWNOUT               // No brownout reset
#FUSES NOMCLR                   // Master Clear pin used for I/O
#FUSES NOLVP                    // No low voltage programming
#FUSES NOXINST                  // Extended instruction set disabled
#FUSES PROTECT                  // Code protection enabled

#use delay(internal=16MHz)

// Pin definitions - Branch inputs
#define BRANCH4      PIN_A0
#define BRANCH5      PIN_A1
#define BRANCH6      PIN_A2
#define BRANCH7      PIN_A3
#define BRANCH8      PIN_A4
#define BRANCH9      PIN_A5
#define LED2         PIN_A6
#define LED1         PIN_A7

// Keypad pins
#define COL3         PIN_B0
#define ROW1         PIN_B1
#define ROW2         PIN_B2
#define ROW3         PIN_B3
#define ROW4         PIN_B4
#define BRANCH2      PIN_B6
#define BRANCH1      PIN_B7

// LED outputs
#define LED3         PIN_C0
#define LED4         PIN_C1
#define LED5         PIN_C2
#define LED6         PIN_C3
#define RELAY        PIN_C7
#define LED7         PIN_D0
#define LED8         PIN_D1
#define LED9         PIN_D2
#define LED10        PIN_D3

// Status LEDs
#define RED_LED      PIN_D4
#define GREEN_LED    PIN_D5

// More keypad pins
#define COL1         PIN_D6
#define COL2         PIN_D7

// Additional pins
#define BRANCH10     PIN_E0
#define BUZZER1      PIN_E2
#define BUZZER2      PIN_E1
#define BRANCH3      PIN_E3

#define BRANCH_COUNT 10

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <internal_eeprom.c>

// Global variables for password management
int32 master_password = 123456;          // Default master password
int32 multiplier = 1;                    // Position multiplier for digit entry
int32 entered_password = 0;              // Currently entered password
int32 final_password = 0;                // Final password after 6 digits

// System state variables
int8 password_digit_pos = 1;             // Current digit position (1-6)
int8 entered_digit = 12;                 // Last entered digit (12 = no key)
int8 user_number;                        // Current user number being edited
int8 menu_state = 0;                     // Current menu state
int8 buzzer_duration = 30;               // Buzzer beep duration in ms

// Loop counters and status variables
int8 i, a = 0;
int8 user_mismatch = 1;                  // 1 = password doesn't match any user
int8 last_data;                          // EEPROM initialization check
int8 read_data[120];                     // Buffer for EEPROM data
int8 user_index;                         // Current user index for EEPROM ops
int8 data_start;                         // EEPROM data start address
int8 timeout_counter = 0;                // Menu timeout counter

// System flags
int1 relay_status = 0;                   // 0 = relay off, 1 = relay on
int1 star_pressed = 0;                   // Star key (* key) pressed flag
int1 master_pass_select = 0;             // Master password selection mode
int1 delete_user = 0;                    // Delete user flag

// User password array (30 users max)
int32 users[30];
int32 temp_password = 0;                 // Temporary password for verification

// Debounce variables for improved keypad reading
int8 prev_key_state = 0xFF;              // Previous key state (0xFF = no key)
int8 key_counter = 0;                    // Debounce counter
int8 confirmed_key = 0xFF;               // Confirmed key value
int1 key_processed = 0;                  // Key has been processed flag

/*******************************************************************************
* Function: beep_sound
* Description: Generate beep sound and add digit to password
*******************************************************************************/
void beep_sound()
{
   entered_password = (entered_digit * multiplier) + entered_password;
   output_high(BUZZER2);
   delay_ms(buzzer_duration);
   output_low(BUZZER2);
   delay_ms(150);
}

/*******************************************************************************
* Function: correct_feedback
* Description: Visual and audio feedback for correct password
*******************************************************************************/
void correct_feedback()
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
   entered_password = 0;
   entered_digit = 12;
}

/*******************************************************************************
* Function: wrong_feedback
* Description: Visual and audio feedback for wrong password
*******************************************************************************/
void wrong_feedback()
{
   output_high(RED_LED); 
   output_low(GREEN_LED);
   output_high(BUZZER2);
   delay_ms(1000);
   output_low(RED_LED);
   output_low(BUZZER2);

   password_digit_pos = 1;
   entered_password = 0;
   entered_digit = 12;
}

/*******************************************************************************
* Function: eeprom_write
* Description: Write all user passwords and master password to EEPROM
*******************************************************************************/
void eeprom_write()
{
   // Write up to 30 user passwords
   data_start = 0;
   for (user_index = 0; user_index < 30; user_index++) 
   {
      data_start = user_index * 4;

      read_data[data_start] = make8(users[user_index], 0);
      write_eeprom(data_start, read_data[data_start]);
      
      read_data[data_start + 1] = make8(users[user_index], 1);
      write_eeprom(data_start + 1, read_data[data_start + 1]);
      
      read_data[data_start + 2] = make8(users[user_index], 2);
      write_eeprom(data_start + 2, read_data[data_start + 2]);
      
      read_data[data_start + 3] = make8(users[user_index], 3);
      write_eeprom(data_start + 3, read_data[data_start + 3]);
   }
   
   // Write master password at address 120-123
   read_data[0] = make8(master_password, 0);
   read_data[1] = make8(master_password, 1);
   read_data[2] = make8(master_password, 2);
   read_data[3] = make8(master_password, 3);
   write_eeprom(120, read_data[0]);
   write_eeprom(121, read_data[1]);
   write_eeprom(122, read_data[2]);
   write_eeprom(123, read_data[3]);
}

/*******************************************************************************
* Function: eeprom_read
* Description: Read all user passwords and master password from EEPROM
*******************************************************************************/
void eeprom_read()
{
   data_start = 0;
   // Read user passwords
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
                                  read_data[data_start]);
   }
   
   // Read master password from address 120-123
   for (i = 0; i < 4; i++) 
   { 
      read_data[i] = read_eeprom(120 + i); 
   }
   master_password = make32(read_data[3], read_data[2], read_data[1], read_data[0]);
}

/*******************************************************************************
* Function: memory_check
* Description: Check if EEPROM is initialized, if not initialize it
*******************************************************************************/
void memory_check()
{ 
   last_data = read_eeprom(255);
   if(last_data == 0)
   {
      eeprom_read();  // EEPROM already initialized
   }
   else
   { 
      // First time initialization
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

/*******************************************************************************
* Function: check_password
* Description: Verify entered password against all users and master
*******************************************************************************/
void check_password() 
{ 
   user_mismatch = 1; // Assume no match initially
   
   for(a = 0; a < 30; a++) 
   {
      if(final_password == users[a] || final_password == master_password) 
      {
         user_mismatch = 0; // Password matched
         // Success feedback
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
         relay_status++;  // Toggle relay
         break;
      }
   }

   if (user_mismatch) 
   {
      // Wrong password feedback
      output_high(RED_LED);
      output_high(BUZZER2);
      delay_ms(1000);
      output_low(RED_LED);
      output_low(BUZZER2);
   }
   password_digit_pos = 1;
   entered_password = 0;
   entered_digit = 12;
}

/*******************************************************************************
* Function: return_main_menu
* Description: Return to main menu and reset all flags
*******************************************************************************/
void return_main_menu()
{
   output_low(GREEN_LED);
   output_low(RED_LED);
   user_number = 99;
   star_pressed = 0; 
   entered_digit = 12; 
   master_pass_select = 0;
   menu_state = 0;
   disable_interrupts(INT_TIMER0);
}

/*******************************************************************************
* Function: keypad
* Description: Read keypad with debounce algorithm
*******************************************************************************/
void keypad()
{
   int8 current_key = 0xFF;  // Current key value (0xFF = no key)
   
   // Scan row 1
   output_low(ROW1);
   output_high(ROW2);
   output_high(ROW3);
   output_high(ROW4);
   delay_us(10);  // Small delay for row switching
   
   if (!input(COL1)) current_key = 1;
   else if (!input(COL2)) current_key = 2;
   else if (!input(COL3)) current_key = 3;
   
   // Scan row 2
   if (current_key == 0xFF) {
      output_high(ROW1);
      output_low(ROW2);
      output_high(ROW3);
      output_high(ROW4);
      delay_us(10);
      
      if (!input(COL1)) current_key = 4;
      else if (!input(COL2)) current_key = 5;
      else if (!input(COL3)) current_key = 6;
   }
   
   // Scan row 3
   if (current_key == 0xFF) {
      output_high(ROW1);
      output_high(ROW2);
      output_low(ROW3);
      output_high(ROW4);
      delay_us(10);
      
      if (!input(COL1)) current_key = 7;
      else if (!input(COL2)) current_key = 8;
      else if (!input(COL3)) current_key = 9;
   }
   
   // Scan row 4
   if (current_key == 0xFF) {
      output_high(ROW1);
      output_high(ROW2);
      output_high(ROW3);
      output_low(ROW4);
      delay_us(10);
      
      if (!input(COL1)) current_key = 10;  // * key (cancel)
      else if (!input(COL2)) current_key = 0;
      else if (!input(COL3)) current_key = 11; // # key (confirm)
   }
   
   // Set all rows high
   output_high(ROW1);
   output_high(ROW2);
   output_high(ROW3);
   output_high(ROW4);
   
   // Debounce algorithm
   if (current_key == prev_key_state) {
      // Same key state continues
      if (current_key != 0xFF) {
         // A key is pressed
         key_counter++;
         if (key_counter >= 3) {  // Same key read 3 times (~30ms)
            key_counter = 3;  // Limit counter
            if (confirmed_key != current_key) {
               // New key detected
               confirmed_key = current_key;
               key_processed = 0;  // Not processed yet
            }
         }
      } else {
         // No key pressed
         key_counter = 0;
         confirmed_key = 0xFF;
         key_processed = 0;
      }
   } else {
      // Key state changed, reset counter
      key_counter = 0;
      prev_key_state = current_key;
   }
   
   // Process confirmed key if not yet processed
   if (confirmed_key != 0xFF && !key_processed) {
      key_processed = 1;  // Mark as processed
      entered_digit = confirmed_key;
      timeout_counter = 0;  // Reset timeout counter
      
      // Process based on key value
      switch(confirmed_key) {
         case 10:  // * key (cancel/clear)
            final_password = 0;
            entered_password = 0;
            password_digit_pos = 1;
            output_high(BUZZER2);
            delay_ms(buzzer_duration);
            output_low(BUZZER2);
            break;
            
         case 11:  // # key (confirm)
            // Just assign value, processing done in main loop
            break;
            
         default:  // Digits 0-9
            password_digit_pos++;
            beep_sound();
            break;
      }
   }
}

/*******************************************************************************
* Function: check_master_password
* Description: Verify master password for admin menu access
*******************************************************************************/
void check_master_password()
{
   if(entered_digit == 11)
   {
      if(master_password == final_password)
      { 
         correct_feedback(); 
         user_number = 90;
         final_password = 0;
         entered_password = 0;
         menu_state = 2;
      }
      else
      {
         wrong_feedback();
         star_pressed = 0;
         menu_state = 0;
         master_pass_select = 0;
      }
   }
   
   if(entered_digit == 10)
      return_main_menu();
}

/*******************************************************************************
* Function: check_user_number
* Description: Check and validate user number entry
*******************************************************************************/
void check_user_number()
{
   if(password_digit_pos == 3)
   { 
      final_password = entered_password; 
      password_digit_pos = 1; 
      entered_password = 0; 
   }
   
   if(entered_digit == 11)
   {
      int hundred_thousands = (final_password / 100000) % 10;
      int ten_thousands = (final_password / 10000) % 10;
      user_number = hundred_thousands * 10 + ten_thousands;
      
      if(user_number > 0 && user_number <= 30)
      {
         correct_feedback();
         final_password = 0;
         menu_state = 3;
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
         final_password = 0;
         menu_state = 3;
      }
      else
      { 
         wrong_feedback();
         star_pressed = 0;
         user_number = 98;
         menu_state = 0;
         master_pass_select = 0;
         final_password = 0;
      }        
   }
   
   if(entered_digit == 10)
      return_main_menu();
}

/*******************************************************************************
* Function: check_user_for_delete
* Description: Check user number for deletion
*******************************************************************************/
void check_user_for_delete()
{
   if(password_digit_pos == 3)
   { 
      final_password = entered_password; 
      password_digit_pos = 1; 
      entered_password = 0; 
   }
   
   if(entered_digit == 11)
   {
      int hundred_thousands = (final_password / 100000) % 10;
      int ten_thousands = (final_password / 10000) % 10;
      user_number = hundred_thousands * 10 + ten_thousands;
      
      if(user_number > 0 && user_number <= 30)
      {
         correct_feedback();
         delete_user = 1;
      }
      else
      { 
         wrong_feedback();
         star_pressed = 0;
         user_number = 90;
         menu_state = 0;
         master_pass_select = 0;
      }        
   }
   
   if(entered_digit == 10)
      return_main_menu();
}

/*******************************************************************************
* Function: set_new_password
* Description: Get first password entry for new user
*******************************************************************************/
void set_new_password()
{
   if(entered_digit == 11 && final_password != 0)
   {
      temp_password = final_password;
      star_pressed = 0;
      correct_feedback();
      final_password = 0;
      menu_state = 4;
   }
   else
   {
      wrong_feedback();
      star_pressed = 0;
      user_number = 98;
      menu_state = 0;
      master_pass_select = 0;
      final_password = 0;
   }
   
   if(entered_digit == 10)
      return_main_menu();
}

/*******************************************************************************
* Function: confirm_new_password
* Description: Confirm new password by entering it twice
*******************************************************************************/
void confirm_new_password()
{
   if(entered_digit == 11)
   {
      if(temp_password == final_password)
      {
         if(master_pass_select == 0)
         {
            // Save user password
            users[user_number - 1] = final_password;
            star_pressed = 0;
            menu_state = 0;
            eeprom_write();
            correct_feedback();
            final_password = 0;
            output_low(RED_LED);
            output_low(GREEN_LED);
         }
         else
         {
            // Save master password
            master_password = final_password;
            master_pass_select = 0;
         }
      }
      else
      {
         // Passwords don't match
         final_password = 0;
         menu_state = 3;
         wrong_feedback();
      }
   }
   
   if(entered_digit == 10)
      return_main_menu();
}

/*******************************************************************************
* Timer0 ISR - Menu timeout handler
*******************************************************************************/
#INT_TIMER0
void Timer0_isr(void)
{
   timeout_counter++;
   if(timeout_counter == 17)  // ~17 seconds timeout
   {
      timeout_counter = 0;
      return_main_menu();
   }
   set_timer0(7710);
   clear_interrupt(INT_TIMER0);
}

/*******************************************************************************
* Main function
*******************************************************************************/
void main()
{
   // Initialize oscillator
   setup_oscillator(OSC_16MHZ);   
   
   // Timer0 setup for timeout
   clear_interrupt(INT_TIMER0);
   setup_timer_0(RTCC_INTERNAL|RTCC_DIV_256);  // 3.4s overflow
   enable_interrupts(GLOBAL);
   disable_interrupts(INT_TIMER0);
   
   // Configure I/O ports
   set_tris_a(0b00111111);  // A0-A5 inputs, A6-A7 outputs
   set_tris_b(0b11100001);  // B0,B5-B7 inputs, others outputs
   set_tris_c(0b01110000);  // C4-C6 inputs, others outputs
   set_tris_d(0b11000000);  // D6-D7 inputs, others outputs
   set_tris_e(0b11111001);  // E0,E3-E7 inputs, E1-E2 outputs
   
   // Initialize all outputs to low
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

   // Check and initialize EEPROM
   memory_check();
   
   // Main program loop
   while(TRUE)
   {
      // Control relay based on status
      if(relay_status == 1)
      { 
         output_high(GREEN_LED); 
         output_high(RELAY); 
      }
      else if(relay_status == 0)
      { 
         output_low(GREEN_LED); 
         output_low(RELAY); 
      }
      
      // Read keypad
      keypad();
      
      // Check for password entry in normal mode
      if (entered_digit == 11 && star_pressed == 0) 
      { 
         password_digit_pos = 1; 
         check_password(); 
      }
      
      // Check for admin menu entry (* key)
      if (entered_digit == 10)
      { 
         star_pressed = 1;
         enable_interrupts(INT_TIMER0);
         enable_interrupts(GLOBAL);
         set_timer0(7710);
         entered_digit = 12;
      }
      
      // Admin menu handling
      while(star_pressed == 1)
      {
         output_high(RED_LED);
         output_high(GREEN_LED);
         
         menu_state = 1;
         
         // State 1: Master password entry
         while(menu_state == 1)
         { 
            keypad(); 
            check_master_password(); 
         }
         
         // State 2: User number selection
         while(menu_state == 2)
         { 
            keypad(); 
            check_user_number(); 
         }
         
         // State 3: New password entry
         while(menu_state == 3)
         { 
            keypad();
            if(entered_digit == 11)
               set_new_password(); 
         }
         
         // State 4: Confirm password
         while(menu_state == 4)
         { 
            keypad(); 
            confirm_new_password(); 
         }
         
         // State 5: Additional password confirmation (if needed)
         while(menu_state == 5)
         { 
            keypad(); 
            confirm_new_password(); 
         }
         
         // State 6: Delete user
         while(menu_state == 6)
         {
            keypad();
            check_user_for_delete();
            if(delete_user == 1)
            {
               users[user_number - 1] = 0xFFFFFFFF;  // Mark as deleted
               star_pressed = 0;
               menu_state = 0;
               eeprom_write();
               final_password = 0;
               output_low(RED_LED);
               output_low(GREEN_LED);
               delete_user = 0;
            }
         }
         
         star_pressed = 0;
      }
      
      // Update password position multiplier
      if(password_digit_pos == 7)
      { 
         password_digit_pos = 1;
         final_password = entered_password; 
         entered_password = 0;
      }
      
      switch(password_digit_pos)
      {
         case 1: multiplier = 100000; break;
         case 2: multiplier = 10000; break;
         case 3: multiplier = 1000; break;
         case 4: multiplier = 100; break;
         case 5: multiplier = 10; break;
         case 6: multiplier = 1; break;
      }
   }
}
