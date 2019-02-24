/*
 * Catalogue.cc
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

#include "Catalogue.h"
#include <fstream>
#include <iostream>
#include <string>

const int HEADER_LINES = 5;

Catalogue::Catalogue() {
	// TODO Auto-generated constructor stub

}

Catalogue::~Catalogue() {
	// TODO Auto-generated destructor stub
}

/**
 * Load the catalogue from the csv file
 *
 * @param filename the name of the file to load
 * @returns true if load successful
 */
bool Catalogue::load(const std::string &filename)
{
	std::ifstream in(filename);
	if (!in)
	{
		std::cerr << "Unable to load catalogue file " << filename << std::endl;
		return false;
	}

	for (int j = 0; j < HEADER_LINES; ++j)
	{
		skipline(in);
	}

	std::vector<std::string> labels;
	if (!readline(in, labels))
	{
		std::cerr << "Unable to read header row" << std::endl;
		return false;
	} else
	{
		// Tidy the labels up
		for (auto &label : labels)
		{
			// Start by erasing ">" and linefeeds from anywhere in the label
			std::string::size_type pos;
			while ((pos = label.find_first_of("\r\n>"))!=std::string::npos) label.erase(pos,1);
			// Strip spaces at end
			while (!label.empty() && label.back() == ' ') label.erase(label.size() - 1);
			// Reduce double spaces to a single space
			while ((pos = label.find("  "))!= std::string::npos) label.erase(pos,1);

			// Uncomment following line to get a list of labels for checking
			// std::cout << label  << std::endl;
		}

		std::vector<std::string> data;
		while (readline(in, data))
		{
			if (data.size() > 10 && !data[0].empty())
			{
				CatEntry entry;
				for (size_t i = 0; i < std::min(labels.size(), data.size()); ++i)
				{
					entry[labels[i]] = data[i];
				}
				_entries.push_back(entry);
			}
		}
	}

	if (_entries.empty())
	{
		std::cerr << "No data found in catalogue" << std::endl;
		return false;
	}
    return true;
}

/**
 * Skip a line of the catalogue input
 *
 * @param in input stream catalogue is being read from
 * @return true if more data in stream
 */
bool Catalogue::skipline(std::istream &in)
{
	char c;
	while (in.get(c) && c != '\n');
	return in;
}

/**
 * Read a line from the catalogue into an array
 * @param in the input stream to read the line from
 * @param values vector updated with the values found
 * @returns true if line successfully read
 */
bool Catalogue::readline(std::istream &in, std::vector<std::string> &values)
{
	values.clear();
	bool more = true;
	std::string cell;
	char c;
	bool in_quotes = false;

	while(more)
	{
		if (in.get(c))
		{
			if (in_quotes)
			{
				if (c == '"')
				{
					in_quotes = false;
				} else
				{
					cell += c;
				}
			} else
			{
				switch(c)
				{
				case ',':
					values.push_back(cell);
					cell.clear();
					break;
				case '\r':
					// Ignore extra CR in DOS format text files
					break;

				case '\n':
					values.push_back(cell);
					more = false;
					break;

				case '"':
					in_quotes = true;
					break;

				default:
					cell += c;
					break;
				}
			}
		} else
		{
			more = false;
		}
	}

	// True if input was successful - shouldn't hit end of file
	return in;
}
