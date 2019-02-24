/*********************************************************************
* Copyright 2009-2011 Alan Buckley
*
* This file is part of PackIt.
*
* PackIt is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* PackIt is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with PackIt. If not, see <http://www.gnu.org/licenses/>.
*
*****************************************************************************/


#include "RISCOSZipExtra.h"
#include "tbx/path.h"


#include <iostream>
#include <iomanip>

RISCOSZipExtra::RISCOSZipExtra()
{
  signature = 0x30435241;
  loadaddress= execaddress = 0;
  attributes = 1 /*ATTR_OWNER_READ */
               | 2 /* OWNER_WRITE */
               | 0X10 /* WORLD_READ */
               | 0x20 /* WORLD_WRITE */;
  reserved = 0;
}

RISCOSZipExtra::RISCOSZipExtra(int file_type)
{
  signature = 0x30435241;

   // Set time to current time
  tbx::UTCTime time = tbx::UTCTime::now();

   execaddress = time.low_word();
   loadaddress = 0xFFF00000; // Marks as file type
   loadaddress |= file_type << 8; // File type
   loadaddress |= time.high_byte();

   attributes = 1 /*ATTR_OWNER_READ */
                | 2 /* OWNER_WRITE */
                | 0X10 /* WORLD_READ */
                | 0x20 /* WORLD_WRITE */;

   reserved = 0;
}

RISCOSZipExtra::RISCOSZipExtra(const tbx::PathInfo &entry)
{
  signature = 0x30435241;

  execaddress = entry.exec_address();
  loadaddress = entry.load_address();
  attributes = entry.attributes();
  reserved = 0;

  if (entry.has_file_type())
  {
	  // Ensure file type is in top of the load address
	  loadaddress = 0xFFF00000
//		   | (entry.file_type() << 8)
// Use raw file type as we copy Image FS entries as they are
// and don't go into them.
		   | (entry.raw_file_type() << 8)
		   | (loadaddress & 0xFF);
  }
}


RISCOSZipExtra::RISCOSZipExtra(void *buffer)
{
	unsigned int *ibuf = (unsigned int *)buffer;

	signature = *ibuf++;
	loadaddress = *ibuf++;
	execaddress = *ibuf++;
	attributes = *ibuf++;
	reserved = 0;
}
