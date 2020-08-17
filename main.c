#include "main.h"
#include <stdio.h>
#include "romfunctions.h"
#include <z80.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

const uint16_t midifreqlut[] = {
0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	
0,	0,	0,	0,	0,	0,	0,	0,	0,	3954,	3732,	3523,	3325,	
3138,	2962,	2796,	2639,	2491,	2351,	2219,	2095,	1977,	1866,	1761,	1662,	1569,	
1481,	1398,	1319,	1245,	1176,	1110,	1047,	988,	933,	881,	831,	785,	741,	
699,	660,	623,	588,	555,	524,	494,	467,	440,	416,	392,	370,	349,	
330,	311,	294,	277,	262,	247,	233,	220,	208,	196,	185,	175,	165,	
156,	147,	139,	131,	124,	117,	110,	104,	98,	93,	87,	82,	78,	
73,	69,	65,	62,	58,	55,	52,	49,	46,	44,	41,	39,	37,	
35,	33,	31,	29,	28,	26,	25,	23,	22,	21,	19,	18,	17,	
16,	15,	15,	14,	13,	12,	12,	11,	10,	10,	9,	};

#define YM2149_FREQA_LSB	0x00
#define YM2149_FREQA_MSB	0x01
#define YM2149_FREQB_LSB	0x02
#define YM2149_FREQB_MSB	0x03
#define YM2149_FREQC_LSB	0x04
#define YM2149_FREQC_MSB	0x05
#define YM2149_FREQ_NOISE	0x06
#define YM2149_MIXER		0x07
#define YM2149_LEVELA		0x08
#define YM2149_LEVELB		0x09
#define YM2149_LEVELC		0x0A
#define YM2149_FREQ_ENV_LSB	0x0B
#define YM2149_FREQ_ENV_MSB	0x0C
#define YM2149_SHAPE_ENV	0x0D
#define YM2149_IODATA_A		0x0E
#define YM2149_IODATA_B		0x0F

__sfr __at 0xD8 YM2149_REGISTER;
__sfr __at 0xD0 YM2149_DATA;

uint32_t ticks;

void print_memory(const void *addr, uint16_t size)
{
    uint16_t printed = 0;
    uint16_t i;
    const unsigned char* pc = addr;
    for (i=0; i<size; ++i)
    {
        int  g;
        g = (*(pc+i) >> 4) & 0xf;
        g += g >= 10 ? 'a'-10 : '0';
        rom_putchar_uart(g);
        printed++;

        g = *(pc+i) & 0xf;
        g += g >= 10 ? 'a'-10 : '0';
        rom_putchar_uart(g);
        printed++;
        if (printed % 32 == 0) rom_putchar_uart('\n');
        else if (printed % 4 == 0) rom_putchar_uart(' ');
    }
}

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))

struct channel
{
	uint8_t reg_level;
	uint8_t freq_lsb;
	uint8_t freq_msb;
	uint8_t note;
	uint32_t started_tick;
};

struct channel channels[] = {
	{	/* Channel B is mixed into the stereo center, use it as the primary note */
		.reg_level = YM2149_LEVELB,
		.freq_lsb  = YM2149_FREQB_LSB,
		.freq_msb  = YM2149_FREQB_MSB,
		.note 	   = 0x00,
		.started_tick   = 0
	},
	{
		.reg_level = YM2149_LEVELA,
		.freq_lsb  = YM2149_FREQA_LSB,
		.freq_msb  = YM2149_FREQA_MSB,
		.note 	   = 0x00,
		.started_tick   = 0
	},
	{
		.reg_level = YM2149_LEVELC,
		.freq_lsb  = YM2149_FREQC_LSB,
		.freq_msb  = YM2149_FREQC_MSB,
		.note 	   = 0x00,
		.started_tick   = 0
	}
};

static inline void set_ym_register(uint8_t reg, uint8_t value)
{
	YM2149_REGISTER = reg;
	YM2149_DATA = value; 
}

void note_off(uint8_t note)
{
	uint8_t channel; 

	for (channel = 0; channel < ARRAY_SIZE(channels); ++channel)
	{
		if (channels[channel].note == note)
		{
			set_ym_register(channels[channel].reg_level, 0x00); // volume off, mode 0
			channels[channel].note = 0x00; 
			channels[channel].started_tick = 0x00;
			return;
		}
	}
}

void note_on(uint8_t note, uint8_t velocity)
{
	uint8_t channel; 
	uint16_t tonereg; 

	if (velocity == 0)
	{
		note_off(note);
		return;
	}

	for (channel = 0; channel < ARRAY_SIZE(channels); ++channel)
	{
		if (channels[channel].note == 0x00 || channels[channel].note == note)
		{
			tonereg = midifreqlut[note];

			//set_ym_register(channels[channel].reg_level, (velocity >> 3) & 0x0F); // dynamic volume
			set_ym_register(channels[channel].reg_level, 0x08); // max volume, mode 0

			set_ym_register(channels[channel].freq_lsb, tonereg & 0x00FF);
			set_ym_register(channels[channel].freq_msb, (tonereg & 0x0F00) >> 8);
			channels[channel].note = note; 
			channels[channel].started_tick  = ticks;
			return;
		}
	}
}

void dump_channels()
{
	uint8_t channel; 

	for (channel = 0; channel < ARRAY_SIZE(channels); ++channel)
	{
		rom_putstring_uart("channel: 0x");
		print_memory(&channel, 1);
		rom_putstring_uart(" note: 0x");
		print_memory(&channels[channel].note, 1);
		rom_putstring_uart("\n");

	}
}

void handle_midi_command(uint8_t inputcommand, uint8_t note, uint8_t velocity)
{
	uint8_t command; 
	uint8_t channel; 

	command = inputcommand & 0xF0; 
	channel = inputcommand & 0x0F; 

	/*rom_putstring_uart("handle_midi_command: command ");
	print_memory(&command, 1);
	rom_putstring_uart(" note ");
	print_memory(&note, 1);
	rom_putstring_uart(" velocity ");
	print_memory(&velocity, 1);
	rom_putstring_uart("\n");*/

	set_ym_register(YM2149_MIXER, 0x38); // Tone Channel A,B,C 

	if (command == 0x90)
	{
		note_on(note, velocity);
		//dump_channels();
	}

	if (command == 0x80)
	{
		note_off(note);
		//dump_channels();
	}

	if (command == 0x85)
	{
		set_ym_register(note, velocity);
	}
}

void main()
{
	uint8_t input = 0; 
	uint8_t input_byte = 0; 
	uint8_t command = 0; 
	uint8_t note = 0; 
	uint8_t velocity = 0; 

	rom_putstring_uart("hello from midisynth!\n");

	ticks = 0;

	while (1)
	{
		while (rom_uart_available())
		{
			input = rom_uart_read();

			if (input > 0x7F)
			{
				// New command
				command = input; 
				input_byte = 0;
				note = 0; 
				velocity = 0; 
			}
			else
			{
				if (command != 0x00)
				{
					// Data byte
					input_byte++; 
					if (input_byte == 1)
					{
						note = input; 
					}
					if (input_byte == 2)
					{
						velocity = input; 
						handle_midi_command(command, note, velocity);
						input_byte = 0; 
						command = 0; 
					}
				}
			}

			/*rom_putstring_uart("read: ");
			print_memory(&input, 1);
			rom_putstring_uart(" #: ");
			print_memory(&input_byte, 1);

			rom_putstring_uart(" command: ");
			print_memory(&command, 1);

			rom_putstring_uart(" note: ");
			print_memory(&note, 1);

			rom_putstring_uart(" velocity: ");
			print_memory(&velocity, 1);

			rom_putstring_uart("\n");*/
		}

		ticks++;

		if (ticks % 1000 == 0)
		{
			// unstick hung notes
			uint8_t channel; 

			for (channel = 0; channel < ARRAY_SIZE(channels); ++channel)
			{
				if (channels[channel].note != 0x00 && channels[channel].started_tick + 4000 <= ticks)
				{
					rom_putstring_uart("hung note on channel ");
					print_memory(&channel, 1);
					rom_putstring_uart(" note ");
					print_memory(&channels[channel].note, 1);
					rom_putstring_uart("\n");

					set_ym_register(channels[channel].reg_level, 0x00); // volume off, mode 0
					channels[channel].note = 0x00; 
					channels[channel].started_tick = 0x00;
				}
			}
		}
	}
}
