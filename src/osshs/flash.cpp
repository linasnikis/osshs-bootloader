/*
 * MIT License
 *
 * Copyright (c) 2019 Linas Nikiperavicius
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <osshs/log/logger.hpp>
#include <osshs/flash.hpp>
#include <modm/platform.hpp>

namespace osshs
{
		bool
		Flash::initialize()
		{
			// Enable CRC peripheral clock
			RCC->AHBENR |= RCC_AHBENR_CRCEN;

			// Unlock flash if locked
			if (isLocked() && !unlock())
			{
				OSSHS_LOG_ERROR("Initializing flash failed. Could not unlock flash.");
				return false;
			}

			OSSHS_LOG_INFO("Initializing flash succeeded.");
			return true;
		}

		void
		Flash::deinitialize()
		{
			// Disable CRC peripheral clock
			RCC->AHBENR &= !RCC_AHBENR_CRCEN;

			OSSHS_LOG_INFO("Deinitializing flash succeeded.");
		}

		bool
		Flash::isLocked()
		{
			return FLASH->CR & FLASH_CR_LOCK;
		}

		bool
		Flash::unlock()
		{
			// Unlock flash
			FLASH->KEYR = OSSHS_FLASH_KEY1;
			FLASH->KEYR = OSSHS_FLASH_KEY2;

			// Verify that flash was unlocked
			if (isLocked())
			{
				OSSHS_LOG_ERROR("Unlocking flash failed.");
				return false;
			}

			OSSHS_LOG_INFO("Unlocking flash succeeded.");
			return true;
		}

		void
		Flash::lock()
		{
			// Lock flash
			FLASH->CR |= FLASH_CR_LOCK;

			OSSHS_LOG_INFO("Locking flash succeeded.");
		}

		uint16_t
		Flash::readHalfWord(uint32_t address)
		{
			if (address & 0b1)
			{
				OSSHS_LOG_ERROR("Reading half word from flash failed. Address not half word aligned(address = `0x%08x`).", address);
				return false;
			}

			// Read from flash
			uint16_t value = *reinterpret_cast<uint16_t *>(address);

			OSSHS_LOG_DEBUG("Reading half word from flash succeeded(address = `0x%08x`, value = `0x%04x`).", address, value);
			return value;
		}

		bool
		Flash::writeHalfWord(uint32_t address, uint16_t value)
		{
			if (address & 0b1)
			{
				OSSHS_LOG_ERROR("Writing half word to flash failed. Address not half word aligned(address = `0x%08x`, value = `0x%04x`).",
					address, value);
				return false;
			}

			// Wait until flash is not busy
			while(FLASH->SR & FLASH_SR_BSY);

			// Enable flash programming
			FLASH->CR |= FLASH_CR_PG;

			// Write to flash
			*reinterpret_cast<uint16_t *>(address) = value;

			// Wait until flash is not busy
			while(FLASH->SR & FLASH_SR_BSY);

			// Disable flash programming
			FLASH->CR &= ~FLASH_CR_PG;

			// Verify written value
			if (*reinterpret_cast<uint16_t *>(address) == value)
			{
				OSSHS_LOG_DEBUG("Writing half word to flash succeeded(address = `0x%08x`, value = `0x%04x`).", address, value);
				return true;
			}

			OSSHS_LOG_ERROR("Writing half word to flash failed(address = `0x%08x`, value = `0x%04x`).", address, value);
			return false;
		}

		bool
		Flash::erasePage(uint32_t address)
		{
			if (address % OSSHS_FLASH_PAGE_SIZE)
			{
				OSSHS_LOG_ERROR("Erasing flash page failed. Address not page aligned(address = `0x%08x`, page = `%d`).",
					address, (address - OSSHS_FLASH_ORIGIN) / OSSHS_FLASH_PAGE_SIZE);
				return false;
			}

			// Wait until flash is not busy
			while(FLASH->SR & FLASH_SR_BSY);

			// Enable page erasing
			FLASH->CR |= FLASH_CR_PER;

			// Specify which address to erase
			FLASH->AR = address;

			// Start erasing
			FLASH->CR |= FLASH_CR_STRT;

			// Wait until flash is not busy
			while(FLASH->SR & FLASH_SR_BSY);

			// Disable page erasing
			FLASH->CR &= ~FLASH_CR_PER;

			// Verify that the page was erased
			for (uint32_t i = address; i < address + OSSHS_FLASH_PAGE_SIZE; i += 2)
				if (*reinterpret_cast<uint16_t *>(i) != 0xffff)
				{
					OSSHS_LOG_ERROR("Erasing flash page failed(address = `0x%08x`, page = `%d`).",
						address, (address - OSSHS_FLASH_ORIGIN) / OSSHS_FLASH_PAGE_SIZE);
					return false;
				}

			OSSHS_LOG_DEBUG("Erasing flash page succeeded(address = `0x%08x`, page = `%d`).",
				address, (address - OSSHS_FLASH_ORIGIN) / OSSHS_FLASH_PAGE_SIZE);
			return true;
		}

		bool
		Flash::readPage(uint32_t address, std::unique_ptr<uint8_t[]> &buffer)
		{
			if (address % OSSHS_FLASH_PAGE_SIZE)
			{
				OSSHS_LOG_ERROR("Reading flash page failed. Address not page aligned(address = `0x%08x`, page = `%d`).",
					address, (address - OSSHS_FLASH_ORIGIN) / OSSHS_FLASH_PAGE_SIZE);
				return false;
			}

			// Read a whole page from flash
			for (uint32_t i = 0; i < OSSHS_FLASH_PAGE_SIZE; i += 2)
			{
				// Read a value from flash
				uint16_t value = *reinterpret_cast<uint16_t *>(address + i);

				// Little endian is the default memory format for ARM processors
				buffer[i + 0] = value & 0xff;
				buffer[i + 1] = value >> 8;
			}

			OSSHS_LOG_DEBUG("Reading flash page succeeded(address = `0x%08x`, page = `%d`).",
				address, (address - OSSHS_FLASH_ORIGIN) / OSSHS_FLASH_PAGE_SIZE);
			return true;
		}

		bool
		Flash::writePage(uint32_t address, std::unique_ptr<uint8_t[]> &buffer)
		{
			if (address % OSSHS_FLASH_PAGE_SIZE)
			{
				OSSHS_LOG_ERROR("Writing flash page failed. Address not page aligned(address = `0x%08x`, page = `%d`).",
					address, (address - OSSHS_FLASH_ORIGIN) / OSSHS_FLASH_PAGE_SIZE);
				return false;
			}

			if (!erasePage(address))
			{
				OSSHS_LOG_ERROR("Writing flash page failed. Page was not erased(address = `0x%08x`, page = `%d`).",
					address, (address - OSSHS_FLASH_ORIGIN) / OSSHS_FLASH_PAGE_SIZE);
				return false;
			}

			// Enable flash programming
			FLASH->CR |= FLASH_CR_PG;

			// Wait until flash is not busy
			while(FLASH->SR & FLASH_SR_BSY);

			// Write a whole page to flash
			for (uint32_t i = 0; i < OSSHS_FLASH_PAGE_SIZE; i += 2)
			{
				// Little endian is the default memory format for ARM processors
				uint16_t value = buffer[i] | (buffer[i + 1] << 8);
				
				// Write to flash
				*reinterpret_cast<uint16_t *>(address + i) = value;

				// Wait until flash is not busy
				while(FLASH->SR & FLASH_SR_BSY);

				// Verify written value
				if (*reinterpret_cast<uint16_t *>(address + i) != value)
				{
					OSSHS_LOG_ERROR("Writing flash page failed. Value could not be written(address = `0x%08x`, value = `0x%04x`).",
						address, buffer[i]);
					return true;
				}
			}
			
			// Disable flash programming
			FLASH->CR &= ~FLASH_CR_PG;

			OSSHS_LOG_DEBUG("Writing flash page succeeded(address = `0x%08x`, page = `%d`).",
				address, (address - OSSHS_FLASH_ORIGIN) / OSSHS_FLASH_PAGE_SIZE);
			return true;
		}

		bool
		Flash::calculatePageCRC(uint32_t address, std::unique_ptr<uint32_t> &crc)
		{
			if (address % OSSHS_FLASH_PAGE_SIZE)
			{
				OSSHS_LOG_ERROR("Calculating flash page CRC failed. Address not page aligned(address = `0x%08x`, page = `%d`).",
					address, (address - OSSHS_FLASH_ORIGIN) / OSSHS_FLASH_PAGE_SIZE);
				return false;
			}

			// Reset CRC peripheral
			CRC->CR |= CRC_CR_RESET;

			// Calculate CRC of a flash page
			uint32_t i;
			for (i = 0; i < OSSHS_FLASH_PAGE_SIZE; i += 4)
			{
#if OSSHS_FLASH_CRC_REFLECT_INPUT
				// Read a full word from flash
				uint16_t majorHalfWord = *reinterpret_cast<uint16_t *>(address + i + 0);
				uint16_t minorHalfWord = *reinterpret_cast<uint16_t *>(address + i + 2);

				// Little endian is the default memory format for ARM processors
				uint32_t value = reflectWord(majorHalfWord | (minorHalfWord << 16));
#else
				// Read a full word from flash
				uint16_t majorHalfWord = *reinterpret_cast<uint16_t *>(address + i + 0);
				uint16_t minorHalfWord = *reinterpret_cast<uint16_t *>(address + i + 2);

				// Swap order of bytes in both half words
				majorHalfWord = ((majorHalfWord & 0x00ff) << 8) |
												((majorHalfWord & 0xff00) >> 8);

				minorHalfWord = ((minorHalfWord & 0x00ff) << 8) |
												((minorHalfWord & 0xff00) >> 8);

				// Little endian is the default memory format for ARM processors
				uint32_t value = minorHalfWord | (majorHalfWord << 16);
#endif

				// Calculate CRC of the full word
				CRC->DR = value;
			}

			// Retrieve calculated CRC from peripheral
#if OSSHS_FLASH_CRC_REFLECT_RESULT
			*crc = reflectWord(CRC->DR);
#else
			*crc = CRC->DR;
#endif

			// Apply final XOR value to CRC
#if OSSHS_FLASH_CRC_FINAL_XOR != 0
			(*crc) ^= OSSHS_FLASH_CRC_FINAL_XOR;
#endif

			// Reset CRC peripheral
			CRC->CR |= CRC_CR_RESET;

			OSSHS_LOG_DEBUG("Calculating flash page CRC succeeded(address = `0x%08x`, page = `%d`, crc = `0x%08x`).",
				address, (address - OSSHS_FLASH_ORIGIN) / OSSHS_FLASH_PAGE_SIZE, *crc);
			return true;
		}
		
		uint32_t
		Flash::reflectWord(uint32_t value)
		{
			value = ((value >>  1) & 0x55555555) | ((value <<  1) & 0xaaaaaaaa);
			value = ((value >>  2) & 0x33333333) | ((value <<  2) & 0xcccccccc);
			value = ((value >>  4) & 0x0f0f0f0f) | ((value <<  4) & 0xf0f0f0f0);
			value = ((value >>  8) & 0x00ff00ff) | ((value <<  8) & 0xff00ff00);
			value = ((value >> 16) & 0x0000ffff) | ((value << 16) & 0xffff0000);
			return value;
		}
}
