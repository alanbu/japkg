/*********************************************************************
* Copyright 2009 Alan Buckley
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


#ifndef RISCOSZIPEXTRA_H_
#define RISCOSZIPEXTRA_H_

namespace tbx
{
	class PathInfo;
}

class RISCOSZipExtra
{
public:
	RISCOSZipExtra();
	RISCOSZipExtra(int file_type);
	RISCOSZipExtra(const tbx::PathInfo &entry);
	RISCOSZipExtra(void *buffer);

	void *buffer() {return &signature;}
	int size() const {return 20;}

	unsigned int signature;
	unsigned int loadaddress;
	unsigned int execaddress;
	unsigned int attributes;
	unsigned int reserved;

	bool check_signature() const {return (signature == 0x30435241);} // "ARC0"

	static unsigned short tag()	{ return 0x4341; }
};

#endif
