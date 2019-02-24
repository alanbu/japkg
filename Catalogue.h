/*
 * Catalogue.h
 *
 *  Created on: 22 Sep 2018
 *      Author: alanb
 */
/*********************************************************************
* Copyright 2018 Alan Buckley
*
* This file is part of japkg.
*
* japkg is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* japkg is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with PackIt. If not, see <http://www.gnu.org/licenses/>.
*
*****************************************************************************/

#ifndef CATALOGUE_H_
#define CATALOGUE_H_


#include <map>
#include <string>
#include <vector>

typedef std::map<std::string, std::string> CatEntry;

class Catalogue {
public:
	Catalogue();
	virtual ~Catalogue();

	bool load(const std::string &filename);

	typedef std::vector<CatEntry>::const_iterator const_iterator;

	const_iterator begin() const {return _entries.cbegin();}
	const_iterator end() const {return _entries.cend();}

	size_t size() const {return _entries.size();}

private:
	bool skipline(std::istream &in);
	bool readline(std::istream &in, std::vector<std::string> &values);

private:
	std::vector<CatEntry> _entries;
};

#endif /* CATALOGUE_H_ */
