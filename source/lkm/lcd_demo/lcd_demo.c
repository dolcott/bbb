//////////////////////////////////////////////////////////
//Loadable Kernel Module Using the BeagleBoneBlack
//
//LCD Demo Module
//Text LCD Display
//EA Displays
//4bit parallel display
//Data Lines: GPIO 66, 67, 68, 69
//Control Lines: GPIO 44, 45, 46
//
//Equivalent Pins:
//Data Lines:
//Pin 28-31 on the display, coorespond to D0 - D3 (ie, reversed order)
//GPIO66 - D0
//GPIO67 - D1
//GPIO68 - D2
//GPIO69 - D3
//
//GPIO44 - Pin 36 - E - latch pin
//GPIO45 - Pin 37 - RW - read/write 0=write
//GPIO46 - Pin 39 - RS - cmd/data, 0 = command, 1 = data
//GPIO47 - Pin 40 - RESET - active low
//
#include <linux/init.h>		//__init and __exit macros
#include <linux/module.h>	//required for any module
#include <linux/kernel.h>	//required for any module
#include <linux/io.h>		//access to hardware registers

#include <linux/kthread.h>    //threads
#include <linux/delay.h>      //sleep command



//////////////////////////////////////////
//Register definitions
//GPIO Base Addresses
#define GPIO0_ADDR 0x44E07000		//GPIO0 base address
#define GPIO1_ADDR 0x4804C000		//GPIO1 base address
#define GPIO2_ADDR 0x481AC000		//GPIO2 base address
#define GPIO3_ADDR 0x481AF000		//GPIO3 base address

////////////////////////////////////////////////////
//GPIO Offsets - Apply to base for a specific IO
//This is a partial list, not sure if DATAIN is corrrect
//or what it's used for.
//use DATAOUT, SET, and CLEAR offsets for capturing the 
//status of GPIOX, set pin, and clearing pins.
//
#define GPIO_OE_ADDR 			0x134	//output enable - 1 in, 0 out
#define GPIO_DATAOUT 			0x13C	//pin states for output pins
#define GPIO_DATAIN 			0x138	//??
#define GPIO_CLEARDATAOUT		0x190	//write to clear a pin
#define GPIO_SETDATAOUT			0x194	//write to set a pin
//
//////////////////////////////////////////
//Control Registers
//
#define GPIO1_OUTPUT_REG		(GPIO1_ADDR | GPIO_DATAOUT)			//read
#define GPIO1_INPUT_REG			(GPIO1_ADDR | GPIO_DATAIN)			//read
#define GPIO1_SET_REG			(GPIO1_ADDR | GPIO_SETDATAOUT)		//write
#define GPIO1_CLEAR_REG			(GPIO1_ADDR | GPIO_CLEARDATAOUT)	//write
#define GPIO1_OE_REG			(GPIO1_ADDR | GPIO_OE_ADDR)			//read/write?

#define GPIO2_OUTPUT_REG		(GPIO2_ADDR | GPIO_DATAOUT)			//read
#define GPIO2_INPUT_REG			(GPIO2_ADDR | GPIO_DATAIN)			//read
#define GPIO2_SET_REG			(GPIO2_ADDR | GPIO_SETDATAOUT)		//write
#define GPIO2_CLEAR_REG			(GPIO2_ADDR | GPIO_CLEARDATAOUT)	//write
#define GPIO2_OE_REG			(GPIO2_ADDR | GPIO_OE_ADDR)			//read/write?

/////////////////////////////////////////
//Data Line Bits - 66-69
//GPIO2
#define DATA0_BIT			(1 << 2)	//pin66 - GPIO2, 66-64
#define DATA1_BIT			(1 << 3)	//pin67 - GPIO2
#define DATA2_BIT			(1 << 4)	//pin68 - GPIO2
#define DATA3_BIT			(1 << 5)	//pin69 - GPIO2

/////////////////////////////////////////
//Control Line Bits - 44-46
//GPIO1
#define E_BIT			(1 << 12)	//pin44 - GPIO1
#define RW_BIT			(1 << 13)	//pin45 - GPIO1
#define RS_BIT			(1 << 14)	//pin46 - GPIO1
#define RESET_BIT		(1 << 15)	//pin47 - GPIO1


//LCD Defaults
#define LCD_CONTRAST_DEFAULT		(11)		//0x0B


/////////////////////////////////////////////
//kernel memory locations
//Control lines - pins 44, 45, 46
void __iomem* ioOE_ctl_Reg;			//output enable reg
void __iomem* ioSet_ctl_Reg;		//write - set
void __iomem* ioClear_ctl_Reg;		//write - clear

//Data lines - pins 66, 67, 68, 69
void __iomem* ioOE_data_Reg;		//output enable reg
void __iomem* ioSet_data_Reg;		//write - set
void __iomem* ioClear_data_Reg;		//write - clear


/////////////////////////////////////////
//Module Information
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dana Olcott");
MODULE_DESCRIPTION("WDOG 3 Line Display");
MODULE_VERSION("0.1");

//line control
static void lcd_pinConfig(void);			//config pins
static void lcd_pinUnConfig(void);			//reset pin config to default
static void lcd_setDataLines(uint8_t);		//write high/low to data reg
static void lcd_writeEnable(void);			//RW = 0
static void lcd_readEnable(void);			//RW = 1
static void lcd_commandEnable(void);		//RS = 0
static void lcd_dataEnable(void);			//RS = 1
static void lcd_pulseEPin(void);			//pulse E line
static void lcd_reset(void);				//pulse RESET line

//write command/data
static void lcd_writeCommand(uint8_t cmd);
static void lcd_writeData(uint8_t data);


//next level up, init, write string, etc
static void lcd_init(void);
static void lcd_clear(void);
static void lcd_cursorOn(void);
static void lcd_cursorOff(void);

static void lcd_setPosition(uint8_t, uint8_t);		//line and offset
static void lcd_writeString(char* buffer, uint8_t line, uint8_t offset);
static void lcd_writeStringBytes(char* buffer, uint8_t length, uint8_t line, uint8_t offset);
static void lcd_setContrast(uint8_t);





//task for toggle pins
static unsigned int toggleValue = 0;

static struct task_struct *toggleTask;     //changes a value

static int toggleTaskFunction(void *arg)
{
	char buffer[16];
	int length;
	static int counter = 0x00;

	while(!kthread_should_stop())
	{
		//similar to an "enter task critical?"  
		set_current_state(TASK_RUNNING);          //set to running

		if (!toggleValue)
		{
			lcd_cursorOff();
			lcd_clear();
			length = sprintf(buffer, "Count:    %-4d", counter);
			lcd_writeStringBytes(buffer, length, 0, 0);
			lcd_writeString("Hello 1", 1, 2);
			lcd_writeString("Hello 2", 2, 4);


			toggleValue = 1;
			counter++;
		}

		else
		{
			//do something else
			lcd_cursorOn();
			lcd_clear();
			
			lcd_writeString("Goodby 0", 0, 6);
			lcd_writeString("Goodby 1", 1, 4);
			lcd_writeString("Goodby 2", 2, 2);

			toggleValue = 0;
		}

		set_current_state(TASK_INTERRUPTIBLE); //set to interruptable
		msleep(1000);                           //sleep 
	}

	return 0;
}

//write data/command


/////////////////////////////////////////////
//lcd_demo_init()
//
static int __init lcd_demo_init(void)
{
	printk(KERN_EMERG "lcd_demo_init()\n");
	lcd_pinConfig();	
	lcd_init();

	toggleTask = kthread_run(toggleTaskFunction, NULL, "Toggle_Task");

	return 0;
}

/////////////////////////////////////////////
//lcd_exit()
//
static void __exit lcd_demo_exit(void)
{
	printk(KERN_EMERG "lcd_demo_exit()\n");

	//stop the task
	kthread_stop(toggleTask);

	//restore pins to default states
	lcd_pinUnConfig();
}




//config pins
void lcd_pinConfig(void)
{
	int oeDataValue, oeControlValue;

	printk(KERN_EMERG "lcd_pinConfig()\n");

	//remap the control and data line registers
	//control - gpio1 44-47
	ioOE_ctl_Reg = ioremap(GPIO1_OE_REG, 4);		//output enable reg
	ioSet_ctl_Reg = ioremap(GPIO1_SET_REG, 4);		//write - set
	ioClear_ctl_Reg = ioremap(GPIO1_CLEAR_REG, 4);	//write - clear

	//Data lines - pins 66, 67, 68, 69
	ioOE_data_Reg = ioremap(GPIO2_OE_REG, 4);		//output enable reg
	ioSet_data_Reg = ioremap(GPIO2_SET_REG, 4);		//write - set
	ioClear_data_Reg = ioremap(GPIO2_CLEAR_REG, 4);	//write - clear

	//current OE values
	oeControlValue = ioread32(ioOE_ctl_Reg);
	oeDataValue = ioread32(ioOE_data_Reg);

	//control - clear bits for output
	oeControlValue &=~ (E_BIT | RS_BIT | RW_BIT | RESET_BIT);

	//data - clear bits for output
	oeDataValue &=~ (DATA0_BIT | DATA1_BIT | DATA2_BIT | DATA3_BIT);

	iowrite32(oeControlValue, ioOE_ctl_Reg);
	iowrite32(oeDataValue, ioOE_data_Reg);

	//set initial values - data - all off
	iowrite32((DATA0_BIT | DATA1_BIT | DATA2_BIT | DATA3_BIT), ioClear_data_Reg);

	//set initial values - control line
	//e - low, RS = low, RW = low
	iowrite32((E_BIT | RW_BIT | RS_BIT), ioClear_ctl_Reg);

	//reset - set and pulse to reset
	iowrite32(RESET_BIT, ioSet_ctl_Reg);
	msleep(10);
	iowrite32(RESET_BIT, ioClear_ctl_Reg);
	msleep(10);
	iowrite32(RESET_BIT, ioSet_ctl_Reg);

}

void lcd_pinUnConfig(void)
{
	int oeDataValue, oeControlValue;

	printk(KERN_EMERG "lcd_pinConfig()\n");

	//set initial values - data - all off
	iowrite32((DATA0_BIT | DATA1_BIT | DATA2_BIT | DATA3_BIT), ioClear_data_Reg);

	//set initial values - control line
	//e - low, RS = low, RW = low, RESET - low
	iowrite32((E_BIT | RW_BIT | RS_BIT | RESET_BIT), ioClear_ctl_Reg);

	//restore input state to data and control lines
	oeControlValue = ioread32(ioOE_ctl_Reg);
	oeDataValue = ioread32(ioOE_data_Reg);

	//control - set bits for input
	oeControlValue |= (E_BIT | RS_BIT | RW_BIT | RESET_BIT);

	//data - set bits for input
	oeDataValue |= (DATA0_BIT | DATA1_BIT | DATA2_BIT | DATA3_BIT);

	iowrite32(oeControlValue, ioOE_ctl_Reg);
	iowrite32(oeDataValue, ioOE_data_Reg);

}

/////////////////////////////////////
//Use the lower 4 bits, writing to D0,
//D1, D2, and D3
void lcd_setDataLines(uint8_t data)
{
	uint8_t bit0, bit1, bit2, bit3;

	bit0 = (data >> 0) & 0x01;
	bit1 = (data >> 1) & 0x01;
	bit2 = (data >> 2) & 0x01;
	bit3 = (data >> 3) & 0x01;

	if (!bit0)
		iowrite32(DATA0_BIT, ioClear_data_Reg);
	else
		iowrite32(DATA0_BIT, ioSet_data_Reg);

	if (!bit1)
		iowrite32(DATA1_BIT, ioClear_data_Reg);
	else
		iowrite32(DATA1_BIT, ioSet_data_Reg);

	if (!bit2)
		iowrite32(DATA2_BIT, ioClear_data_Reg);
	else
		iowrite32(DATA2_BIT, ioSet_data_Reg);

	if (!bit3)
		iowrite32(DATA3_BIT, ioClear_data_Reg);
	else
		iowrite32(DATA3_BIT, ioSet_data_Reg);
}

//RW = 0
void lcd_writeEnable(void)
{
	iowrite32(RW_BIT, ioClear_ctl_Reg);
}

//RW = 1
void lcd_readEnable(void)
{
	iowrite32(RW_BIT, ioSet_ctl_Reg);
}

//RS = 0
void lcd_commandEnable(void)
{
	iowrite32(RS_BIT, ioClear_ctl_Reg);
}

//RS = 1
void lcd_dataEnable(void)
{
	iowrite32(RS_BIT, ioSet_ctl_Reg);
}

void lcd_pulseEPin(void)
{
	iowrite32(E_BIT, ioSet_ctl_Reg);
	msleep(1);
	iowrite32(E_BIT, ioClear_ctl_Reg);
}


void lcd_reset(void)
{
	iowrite32(RESET_BIT, ioSet_ctl_Reg);
	msleep(10);
	iowrite32(RESET_BIT, ioClear_ctl_Reg);
	msleep(10);
	iowrite32(RESET_BIT, ioSet_ctl_Reg);
}


//write command
//write enable, command enable
//load high, pulse e, load low, pulse e
void lcd_writeCommand(uint8_t cmd)
{
	lcd_writeEnable();
	lcd_commandEnable();

	lcd_setDataLines((cmd >> 4) & 0x0F);	//high
	lcd_pulseEPin();						//pulse
	lcd_setDataLines(cmd & 0x0F);			//low
	lcd_pulseEPin();						//pulse

}

void lcd_writeData(uint8_t data)
{
	lcd_writeEnable();
	lcd_dataEnable();

	lcd_setDataLines((data >> 4) & 0x0F);	//high
	lcd_pulseEPin();						//pulse
	lcd_setDataLines(data & 0x0F);			//low
	lcd_pulseEPin();						//pulse
}


void lcd_init(void)
{
	lcd_reset();		//reset

	//lcd init - set to 4 bit mode.  3 pulses of 
	//of the e pin.  First one is interpreted in 
	//8 bit mode, setting it to 4 bit mode.  Second
	//two are interpreted as 4 bit mode, setting it to 4 bit mode
	//ie, we have an odd number of pulses since the first is
	//8 bit and second 2 are 4 bit

	//set data lines - 0x02
	lcd_setDataLines(0x02);
	msleep(5);

	lcd_pulseEPin();	//once
	msleep(5);

	lcd_pulseEPin();	//twice
	msleep(5);

	lcd_pulseEPin();	//three times
	msleep(5);

	//check busy flag
	//reading and data
	lcd_readEnable();
	lcd_dataEnable();

	//load data lines with 0x00
	lcd_setDataLines(0x00);

	//pulse e 2 x
	lcd_pulseEPin();
	msleep(5);
	lcd_pulseEPin();
	msleep(5);

	//set write command
	lcd_writeEnable();
	lcd_commandEnable();

	msleep(10);

	//Setup the LCD based on table in the datasheet.  Note
	//change in the funtion register 4 bit vs 8 bit.  Can't
	//figure out how to get the contrast set all the way
	//down.
	lcd_writeCommand(0x29);	//4 bit mode, 2 lines, instruction table 1
	lcd_writeCommand(0x15);	//BS 1/5 3 line LCD
	lcd_writeCommand(0x54);	//booster on, contrast bits C5 and C4 low
	lcd_writeCommand(0x6E); //voltage follower and gain
	lcd_writeCommand(0x7B); //set contrast C3, c2, c1 - 72 original
	lcd_writeCommand(0x28); //4 bit, 2 lines, instruction set 0
	lcd_writeCommand(0x0F); //display on, cursor on, cursor blink
	lcd_writeCommand(0x01); //delete display
	lcd_writeCommand(0x06); //cursor auto increment	

	lcd_setContrast(LCD_CONTRAST_DEFAULT);

}

void lcd_clear(void)
{
	lcd_writeCommand(0x01);
}

void lcd_cursorOn(void)
{
	lcd_writeCommand(0x0F);
}

void lcd_cursorOff(void)
{
	lcd_writeCommand(0x0C);	
}




//////////////////////////////////
//Set the cursor position as a function
//of line and offset.  Similar to lcd_setLine
//but with or'd offset
//line = 0, 1, 2...  offset = 0 to 15
void lcd_setPosition(uint8_t line, uint8_t offset)
{
	uint8_t address = 0x80;

	if (line < 3)
	{
		address |= (line << 4);

		if (offset < 16)
		{
			address |= offset;
			lcd_writeCommand(address);			
		}
	}
}

/////////////////////////////////
//Write string to the current line
//and offset.  Buffer should be null
//terminated
//for now, assume string starts at the
//x = 0, no wrapping

void lcd_writeString(char* buffer, uint8_t line, uint8_t offset)
{
	int i = 0;

	if ((line < 3) && (offset < 16))
	{
		lcd_setPosition(line, offset);

		while((buffer[i] != '\0') && (i < 16))
		{
			lcd_writeData(buffer[i]);
			i++;
		}
	}
}

///////////////////////////////////////
//write string to current line and offset
//buffer and length

void lcd_writeStringBytes(char* buffer, uint8_t len, uint8_t line, uint8_t offset)
{
	int i = 0;

	if ((line < 3) && (offset < 16))
	{
		lcd_setPosition(line, offset);

		for (i = 0 ; i < len ; i++)
		{
			lcd_writeData(buffer[i]);
		}
	}
}


/////////////////////////////////////
//Set Contrast - 0 to 15
void lcd_setContrast(uint8_t contrast)
{
	//contrast base = 0x70
	uint8_t value = 0x70;

	if (contrast < 16)
	{
		lcd_writeCommand(0x29);	//4 bit mode, 2 lines, instruction table 1

		value |= contrast;

		lcd_writeCommand(value);
		lcd_writeCommand(0x28); 	//return to instruction set 0		
	}
}



module_init(lcd_demo_init);
module_exit(lcd_demo_exit);
