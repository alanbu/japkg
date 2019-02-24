/*
 * Log.cc
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

#include "Log.h"
#include <ctime>

Log::Log() : _unchanged(0) {
}

Log::~Log()
{
}

/**
 * Start the log, creating and opening the log file
 *
 * @param log_dir directory to save the log/summary to
 * @param msg what's being logged
 */
void Log::start(const std::string &log_dir, const std::string &msg)
{
	std::time_t now = std::time(nullptr);
	char fname[64];
	std::tm *tm_now = std::localtime(&now);
	std::strftime(fname, 64, "%Y%m%d%H%M%S", tm_now );
	_file_prefix = log_dir + "." + fname;
	_log_file.open((_file_prefix + "log").c_str());

	std::strftime(fname, 64, "%c", tm_now);
	_log_file << "Logging of " << msg << " started at " << fname << std::endl;
}

/**
 * Put time of day in the log
 */
void Log::log_time()
{
	std::time_t now = std::time(nullptr);
	char fname[64];
	std::tm *tm_now = std::localtime(&now);
	std::strftime(fname, 64, "%H:%M:%S", tm_now );
	_log_file << fname;
}


/**
 * End of log
 */
void Log::end(const std::string &msg)
{
	log_time();
	_log_file << ":" << msg;

	// Create summary
	std::ofstream sum(_file_prefix + "summary");

	char fname[64];
	std::time_t now = std::time(nullptr);
	std::tm *tm_now = std::localtime(&now);
	std::strftime(fname, 64, "%c", tm_now);
	sum << "Summary of packaging on " << fname << std::endl << std::endl;

	sum << "New packages         " << _new_packages.size() << std::endl;
	sum << "Upgraded packages    " << _upgrade_packages.size() << std::endl;
	sum << "Packages with errors " << _error_packages.size() << std::endl;
	sum << "Unchanged packages   " << _unchanged << std::endl;
	sum << std::endl;
	sum << "Total                " << (_new_packages.size() + _upgrade_packages.size()
			+ _error_packages.size() + _unchanged) << std::endl;
	sum << std::endl;
    if (!_new_packages.empty())
    {
    	sum << std::endl;
    	sum << "New packages" << std::endl;
    	sum << "------------" << std::endl;
    	for(std::string const &title : _new_packages)
    	{
    		sum << title << std::endl;
    	}
    }

    if (!_upgrade_packages.empty())
    {
    	sum << std::endl;
    	sum << "Upgrade packages" << std::endl;
    	sum << "----------------" << std::endl;
    	for(std::string const &title : _upgrade_packages)
    	{
    		sum << title << std::endl;
    	}
    }

    if (!_error_packages.empty())
    {
    	sum << std::endl;
    	sum << "Error packages" << std::endl;
    	sum << "--------------" << std::endl;
    	for(std::string const &title : _error_packages)
    	{
    		sum << title << std::endl;
    	}
    }
}

/**
 * Log a message
 */
void Log::message(const std::string &msg)
{
	log_time();
	_log_file << ":INFO:" << msg << std::endl;
}

void Log::message(size_t count, const std::string &msg)
{
	log_time();
	_log_file << ":INFO:" << count << " " << msg << std::endl;
}

void Log::error(const std::string &msg)
{
	log_time();
	_log_file << ":ERROR:" << msg << std::endl;
}

void Log::fatal_error(const std::string &msg)
{
	log_time();
	_log_file << ":FATAL:" << msg << std::endl;
}


void Log::new_package(const std::string &full_title)
{
	_new_packages.push_back(full_title);
}

void Log::upgrade_package(const std::string &full_title)
{
	_upgrade_packages.push_back(full_title);
}

void Log::error_package(const std::string &full_title)
{
	_error_packages.push_back(full_title);
}

Log::PackageContext::PackageContext(Log &log, const std::string &id, const std::string &title) :
	_log(log),
	_id(id),
	_title(title),
	_package(true),
	_new(false),
	_upgrade(false),
	_error(false)
{
	_log.message("Processing package " + id + " " + title);
}

Log::PackageContext::~PackageContext()
{
	std::string full_name(_id + " " + _title);
	_log.message("Finished processing " + full_name);
	if (_error) _log.error_package(full_name);
	else if (_new) _log.new_package(full_name);
	else if (_upgrade)_log.upgrade_package(full_name);
	else if (_package) _log.inc_unchanged();
}

void Log::PackageContext::do_not_package()
{
	_package = false;
}

void Log::PackageContext::new_package(bool value)
{
	_new = value;
}
void Log::PackageContext::upgrade_package(bool value)
{
	_upgrade= value;
}

void Log::PackageContext::message(const std::string &msg)
{
	_log.message(_id + ":" + msg);
}

void Log::PackageContext::error(const std::string &msg)
{
	_log.error(_id + ":" + msg);
	_error = true;
}
