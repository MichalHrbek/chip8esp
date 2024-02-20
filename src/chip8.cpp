#include "chip8.h"

void Chip8::cycle()
{
	uint16_t opcode = memory[pc] << 8 | memory[pc + 1]; // Loading the next instruction from memory
	
	ulong current = micros();
	if (current - last_tick > 16666) { // Ticking down the timers
		if (delay_timer > 0) --delay_timer;
		if (sound_timer > 0)
		{
			playing_sound = true;
			sound_timer--;
		} else {
			playing_sound = false;
		}
		last_tick = current; // Accurate enough
	}

	uint16_t NNN = opcode & 0x0FFF;
	uint8_t NN = opcode & 0x00FF;
	uint8_t N = opcode & 0x000F;
	uint8_t X = (opcode & 0x0F00) >> 8;
	uint8_t Y = (opcode & 0x00F0) >> 4;

	switch (opcode & 0xF000)
	{
		case 0x0000:
			switch (opcode & 0x00FF)
			{
				case 0x00E0: // Clears the screen
					memset(screen,0,64*32);
					screen_drawn = true;
					pc += 2;
					break;

				case 0x00EE: // Returns from a subroutine
					pc = stack[--sp];
					pc += 2;
					break;

				case 0x0000:
					pc += 2;
					break;

				default:
					unknown_opcode(opcode);
					break;
			}
			break;

		case 0x1000: // Jumps to address NNN
			pc = NNN;
			break;

		case 0x2000: // Calls subroutine at NNN
			stack[sp++] = pc;
			pc = NNN;
			break;

		case 0x3000: // Skips the next instruction if VX equals NN
			if (V[X] == NN) pc += 4;
			else pc += 2;
			break;

		case 0x4000: // Skips the next instruction if VX does not equal NN
			if (V[X] != NN) pc += 4;
			else pc += 2;
			break;

		case 0x5000: // Skips the next instruction if VX equals VY
			if (V[X] == V[Y]) pc += 4;
			else pc += 2;
			break;

		case 0x6000: // Sets VX to NN
			V[X] = NN;
			pc += 2;
			break;

		case 0x7000: // Adds NN to VX
			V[X] += NN;
			pc += 2;
			break;

		case 0x8000: // Math
			switch (opcode & 0x000F)
			{
				case 0x0000: // Sets VX to the value of VY
					V[X] = V[Y];
					pc += 2;
					break;

				case 0x0001: // Sets VX to VX or VY
					V[X] |= V[Y];
					pc += 2;
					break;

				case 0x0002: // Sets VX to VX and VY
					V[X] &= V[Y];
					pc += 2;
					break;

				case 0x0003: // Sets VX to VX xor VY
					V[X] ^= V[Y];
					pc += 2;
					break;

				case 0x0004: // VY is added to VX. VF is to 0 when there's an overflow, and 1 if there is not
					V[X] += V[Y];
					V[0xF] = ((uint16_t)V[X] + (uint16_t)V[Y] > 0xFF ? 1 : 0);
					pc += 2;
					break;

				case 0x0005: // VY is subtracted from VX. VF is set to 0 when there's a borrow, and 1 when there is not
					V[X] -= V[Y];
					V[0xF] = (V[X] < V[Y] ? 1 : 0);
					pc += 2;
					break;

				case 0x0006: // Stores the least significant bit of VX in VF and then shifts VX to the right by 1
					V[X] >>= 1;
					V[0xF] = ((V[X] & 0x1) != 0 ? 1 : 0);
					pc += 2;
					break;

				case 0x0007: // Sets VX to VY minus VX. VF is set to 0 when there's a borrow, and 1 when there is not
					V[X] = (uint8_t)(V[Y] - V[X]);
					V[0xF] = (V[Y] > V[X] ? 1 : 0);
					pc += 2;
					break;

				case 0x000E: // Stores the most significant bit of VX in VF and then shifts VX to the left by 1
					V[0xF] = (V[X] >> 7);
					V[X] <<= 1;
					pc += 2;
					break;

				default:
					unknown_opcode(opcode);
					break;
			}
			break;

		case 0x9000: // Skips the next instruction if VX does not equal VY
			if (V[X] != V[Y]) pc += 4;
			else pc += 2;
			break;

		case 0xA000: // Sets I to the address NNN
			I = NNN;
			pc += 2;
			break;

		case 0xB000: // Jumps to the address NNN plus V0
			pc = (uint16_t)(NNN + V[0]);
			break;

		case 0xC000: // Sets VX to the result of a bitwise and operation on a random number and NN
			V[X] = ((uint8_t)random(256)) & NN;
			pc += 2;
			break;

		case 0xD000: // Draws a sprite
			{
				uint16_t x = V[X];
				uint16_t y = V[Y];
				uint16_t height = opcode & 0x000F;
				uint16_t pixel;

				V[0xF] = 0;
				for (int yline = 0; yline < height; yline++)
				{
					pixel = memory[I + yline];
					for (int xline = 0; xline < 8; xline++)
					{
						if ((pixel & (0x80 >> xline)) != 0)
						{
							uint8_t posX = (x + xline) % 64;
							uint8_t posY = (y + yline) % 32;

							uint16_t posPixel = posX + ((posY) * 64);

							if (screen[posPixel] == true) V[0xF] = 1;
							screen[posPixel] ^= 1;
							screen_drawn = true;
						}
					}
				}

				pc += 2;
				break;
			}

		case 0xE000: // Keypress
			switch (opcode & 0x00FF)
			{
				case 0x009E: // Skips the next instruction if the key stored in VX is pressed
					if (keys[key_map[V[X]]]) pc += 4;
					else pc += 2;
					break;

				case 0x00A1: // Skips the next instruction if the key stored in VX is not pressed
					if (!keys[key_map[V[X]]]) pc += 4;
					else pc += 2;
					break;

				default:
					unknown_opcode(opcode);
					break;
			}
			break;

		case 0xF000:
			switch (opcode & 0x00FF)
			{
				case 0x0007: // Sets VX to the value of the delay timer.
					V[X] = delay_timer;
					pc += 2;
					break;

				case 0x000A: // A key press is awaited, and then stored in VX
					for (uint8_t i = 0; i < 16; i++)
						if (keys[key_map[i]]) { V[X] = i; pc += 2; }
					break;

				case 0x0015: // Sets the delay timer to VX
					delay_timer = V[X];
					pc += 2;
					break;

				case 0x0018: // Sets the sound timer to VX
					sound_timer = V[X];
					pc += 2;
					break;

				case 0x001E: // Adds VX to I
					I += V[X];
					pc += 2;
					break;

				case 0x0029: // Sets I to the location of the sprite for the character in VX
					I = V[X] * 5;
					pc += 2;
					break;

				case 0x0033: // FX33: Stores the Binary-coded decimal representation of VX at the addresses I, I + 1, and I + 2
					memory[I] = (uint8_t)(V[(X)] / 100);
					memory[I + 1] = (uint8_t)(V[(X)] / 10 % 10);
					memory[I + 2] = (uint8_t)(V[X] % 100 % 10);
					pc += 2;
					break;

				case 0x0055: // Stores from V0 to VX (including VX) in memory, starting at address I
					for (int i = 0; i <= X; i++)
						memory[I + i] = V[i];
					pc += 2;
					break;

				case 0x0065: // Fills from V0 to VX (including VX) with values from memory, starting at address I
					for (int i = 0; i <= X; i++)
						V[i] = memory[I + i];
					pc += 2;
					break;
				default:
					unknown_opcode(opcode);
					break;
			}
			break;

		default:
			unknown_opcode(opcode);
			break;
	}
}