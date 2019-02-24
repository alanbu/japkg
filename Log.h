/*
 * Log.h
 *
 *  Created on: 3 Oct 2018
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

#ifndef LOG_H_
#define LOG_H_

#include <fstream>
#include <vector>
#include <string>

/**
 * Class to log message and create summary
 */
class Log {
public:
	Log();
	virtual ~Log();

	void start(const std::string &log_dir, const std::string &msg);
	void end(const std::string &msg);
	void message(const std::string &msg);
	void message(size_t count, const std::string &msg);
	void error(const std::string &msg);
	void fatal_error(const std::string &msg);

    class PackageContext
    {
    private:
		Log &_log;
		std::string _id;
		std::string _title;
		bool _package;
		bool _new;
		bool _upgrade;
		bool _error;
    public:
		PackageContext(Log &log, const std::string &id, const std::string &title);
		~PackageContext();
		void new_package(bool value);
		void upgrade_package(bool value);
		void do_not_package();

		void message(const std::string &msg);
		void error(const std::string &msg);
    };

    void new_package(const std::string &full_title);
    void upgrade_package(const std::string &full_title);
    void error_package(const std::string &full_title);
    void inc_unchanged() {_unchanged++;}

private:
    void log_time();

private:
    std::ofstream _log_file;
    std::string _file_prefix;

    std::vector<std::string> _new_packages;
    std::vector<std::string> _upgrade_packages;
    std::vector<std::string> _error_packages;
    size_t _unchanged;
};

#endif /* LOG_H_ */
