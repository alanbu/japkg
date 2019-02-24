/*********************************************************************
* Copyright 2009-2014 Alan Buckley
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



#include "Packager.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <memory>

#include "tbx/reporterror.h"
#include "tbx/path.h"
#include "tbx/stringutils.h"

#define _ZIP_SYSTEM_LINUX
#include "ziparchive/ZipArchive.h"
#include "ziparchive/ZipException.h"
#include "RISCOSZipExtra.h"

/**
 * Name of package items, must be matched with PackageItem enum
 */
const char *Packager::_item_names[NUM_ITEMS] = {
  "Package name",
  "Version",
  "Package version",
  "Section",
  "Priority",
  "Maintainer",
  "Standards version",
  "Summary",
  "Description",
  "Licence",
  "Copyright",
  "Item to install",
  "Install to",
  "Depends",
  "Recommends",
  "Suggests",
  "Conflicts",
  "Components"
};

/**
 * Special directories found in the package
 */
const int NumSpecialDirs = 11;
enum SpecialDirId {SD_RISCPKG, SD_SYSVARS, SD_SPRITES,
      SD_APPS, SD_MANUALS, SD_RESOURCES, SD_BOOT, SD_PLING_BOOT, SD_SYSTEM,
      SD_TOBELOADED, SD_TOBETASKS,
      SD_NONE=99};

const char *SpecialDirs[NumSpecialDirs] =
{
	"RiscPkg",
	"SysVars",
	"Sprites",
	"Apps",
	"Manuals",
	"Resources",
	"Boot",
	"!Boot",
	"System",
	"ToBeLoaded",
	"ToBeTasks"
};

/** Buffer for file copy **/
static int copy_buffer_size = 640 * 1024;
static char *copy_buffer = new char[copy_buffer_size];

/** Exception thrown if there is a problem creating a packages */
class PackageCreateException
{
	std::string message;
public:
	PackageCreateException(const std::string &msg) : message(msg) {};

	const std::string &what() const {return message;}
};


Packager::Packager() :
	_modified(false),
	_error_count(0)
{
    package_name("");
    version("");
    package_version("1");
    section("");
    priority("Optional");
    maintainer("");
    standards_version("0.4.0");
    summary("");
    licence("");
    copyright("");
    set_error(ITEM_TO_PACKAGE, "must be entered");

    // Reset modified flag as nothing has really changed yet
    _modified = false;
}

Packager::~Packager()
{
}

/**
 * Return next error number or -1 if no more errors
 *
 * Returns to start of list when it gets to the end.
 */
int Packager::next_error(int i) const
{
    if (_error_count == 0) return -1;
    if (i >= NUM_ITEMS) i = NUM_ITEMS -1;

    do
    {
       if (++i == NUM_ITEMS) i = 0;
    } while (_errors[i].empty());

    return i;
}

void Packager::package_name(std::string value)
{
	_package_name = value;
	if (value.empty())
	{
	  set_error(PACKAGE_NAME, "must be entered");
	} else
	  clear_error(PACKAGE_NAME);

	modified(true);
}

void Packager::version(std::string value)
{
	_version = value;
   if (value.empty())
   {
      set_error(VERSION, "must be entered");
   } else
      clear_error(VERSION);

   modified(true);
}


void Packager::package_version(std::string value)
{
   _package_version = value;
   if (value.empty())
   {
      set_error(PACKAGE_VERSION, "must be entered");
   } else
      clear_error(PACKAGE_VERSION);

   modified(true);
}

void Packager::section(std::string value)
{
   _section = value;
   if (value.empty())
   {
      set_error(SECTION, "must be entered");
   } else
      clear_error(SECTION);
   modified(true);
}

void Packager::priority(std::string value)
{
   _priority = value;
   if (value.empty())
   {
      set_error(PRIORITY, "must be entered");
   } else
      clear_error(PRIORITY);
   modified(true);
}

void Packager::maintainer (std::string value)
{
   _maintainer = value;
   if (value.empty())
   {
      set_error(MAINTAINER, "must be entered");
   } else
   {
	   std::string::size_type ltpos = value.find('<');
	   std::string::size_type gtpos = value.find('>');

	   if (ltpos == std::string::npos || gtpos == std::string::npos)
		   set_error(MAINTAINER, "Email address must be included and enclosed in '<' and '>'");
	   else if (ltpos > gtpos)
		   set_error(MAINTAINER, "The '<' must appear before the '>' surrounding the email address");
	   else
		   clear_error(MAINTAINER);
   }

   modified(true);
}

void Packager::standards_version(std::string value)
{
   _standards_version = value;
   if (value.empty())
   {
      set_error(STANDARDS_VERSION, "must be entered");
   } else
   {
	   bool format_ok = true;
	   bool last_dot = true; // Causes required error if first char is a dot.
	   int dot_count = 0;

	   for (std::string::const_iterator i = value.begin();
	        i != value.end() && format_ok; ++i)
	   {
		   if ((*i) == '.')
		   {
			   if (last_dot) format_ok = false;
			   dot_count++;
			   last_dot = true;
		   } else
			   last_dot = false;
	   }

	   if (last_dot) format_ok = false;

	   if (dot_count > 3)
		   set_error(STANDARDS_VERSION, "maximum of 4 components separated by dots ('.')");
	   else if (dot_count < 2)
		   set_error(STANDARDS_VERSION, "must contain at least 3 components separated by dots ('.')");
	   else if (!format_ok)
		   set_error(STANDARDS_VERSION, "must be up to 4 numbers separated by dots ('.')");
	   else if (standards_version_lt("0.4.0"))
		   set_error(STANDARDS_VERSION, "must be at least 0.4.0");
	   else
		   clear_error(STANDARDS_VERSION);
   }

   modified(true);
}

/**
 * Check it the standards version is less than the given value
 *
 * @param value version to check against
 * @returns true if version is less than the given value
 */
bool Packager::standards_version_lt(std::string value)
{
	if (_standards_version.empty()) return true;
	std::string::iterator svi = _standards_version.begin();
	std::string::iterator vi = value.begin();
	int check_val = 0;
	while (svi != _standards_version.end())
	{
		if (*svi == '.')
		{
			svi++;
			int value_part = 0;
			while (vi != value.end() && *vi != '.') value_part = value_part * 10 + (*vi++ - '0');
			if (vi != value.end()) ++vi;
			if (check_val < value_part) return true;
			else if (check_val > value_part) return false;
			check_val = 0;
		} else if (*svi >= '0' && *svi <= '9')
		{
			check_val = check_val * 10 + (*svi++ - '0');
		} else
		{
			// All invalid entries are considered less than
			return true;
		}
	}

	int value_part = 0;
	while (vi != value.end() && *vi != '.') value_part = value_part * 10 + (*vi++ - '0');
	if (vi != value.end()) ++vi;

	if (check_val < value_part) return true;

	// Got to end so its equal
	return false;
}

void Packager::summary(std::string value)
{
	_summary = value;
	if (value.empty())
	{
	   set_error(SUMMARY, "must be entered");
	} else
	   clear_error(SUMMARY);
	modified(true);
}

void Packager::description(std::string description)
{
	_description = description;
	modified(true);
}

void Packager::licence(std::string licence)
{
	_licence = licence;
	if (licence.empty())
	{
	   set_error(LICENCE, "must be entered");
	} else
	   clear_error(LICENCE);

	modified(true);
}

void Packager::copyright(std::string value)
{
	_copyright = value;

	if (value.empty())
	{
	   set_error(COPYRIGHT, "must be entered");
	} else
	   clear_error(COPYRIGHT);

	modified(true);
}


/**
 * Set item to package.
 *
 * @param item - item to add/modify to package
 */
void Packager::set_item_to_package(const ItemToPackage &item)
{
	clear_error(ITEM_TO_PACKAGE);
	validate_install_to(item.install_to());
	for (ItemToPackage &check : _items_to_package)
	{
		if (check.source() == item.source())
		{
			check = item;
		    modified(true);
			return;
		}
	}
	_items_to_package.push_back(item);
    modified(true);
}

/**
 * Remove the item from the package list by source location
 */
void Packager::remove_item_to_package(const std::string &source)
{
	for(std::vector<ItemToPackage>::iterator it = _items_to_package.begin();
			it != _items_to_package.end(); ++it)
	{
		if (it->source() == source)
		{
			_items_to_package.erase(it);
			break;
		}
	}
	if (_items_to_package.empty())
	{
		set_error(ITEM_TO_PACKAGE, "You must have a least one item to package");
	}
	modified(true);
}


/**
 * Location to install package to
 *
 */
void Packager::validate_install_to(std::string where)
{
    if (where.empty())
    {
        set_error(INSTALL_TO, "must be entered");
    } else if (*(where.rbegin()) == '.')
    {
    	set_error(INSTALL_TO, "must not end with a full stop");
    } else
    {
    	std::string::size_type dot_pos = where.find('.');
    	std::string root_dir = (dot_pos == std::string::npos) ? where : where.substr(0, dot_pos);
    	std::string opts;
    	bool bad = true;
    	for (int id = SD_APPS; id <= SD_SYSTEM && bad; id++)
    	{
    		if (!opts.empty()) opts += ",";
    		opts += " ";
    		opts += SpecialDirs[id];
    		if (tbx::equals_ignore_case(root_dir, SpecialDirs[id]))
    		{
    			bad = false;
    		}
    	}

    	if (bad)
    	{
    		set_error(INSTALL_TO, "must start with one of"+opts);
    	} else
    	{
    		bool two_dots = false;
    		while (dot_pos != std::string::npos && !two_dots)
    		{
    			std::string::size_type next_dot = where.find('.', dot_pos+1);
    			if (next_dot == dot_pos+1) two_dots = true;
    			dot_pos = next_dot;
    		}

    		if (two_dots)
    		{
    			set_error(INSTALL_TO, "should not have two dots ('.') together");
    		} else
    		{
    			clear_error(INSTALL_TO);
    		}
    	}
    }
}

void Packager::depends(std::string value)
{
   _depends = value;
   check_depends(DEPENDS, value);
   modified(true);
}

void Packager::recommends(std::string value)
{
   _recommends = value;
   check_depends(RECOMMENDS, value);
   modified(true);
}

void Packager::suggests(std::string value)
{
   _suggests = value;
   check_depends(SUGGESTS, value);
   modified(true);
}

void Packager::conflicts(std::string value)
{
   _conflicts = value;
   check_depends(CONFLICTS, value);
   modified(true);
}

/**
 * Check dependency format
 */
void Packager::check_depends(PackageItem where, std::string depends)
{
	if (depends.empty())
	{
		clear_error(where);
		return; // Don't need to have a dependency
	}
	std::string err;
	std::string::size_type comma_pos = depends.find(',');
	std::string::size_type start = 0;
	while (comma_pos != std::string::npos && err.empty())
	{
		err = check_one_dependency(depends.substr(start, comma_pos - start));
		start = comma_pos + 1;
		comma_pos = depends.find(',', start);
	}
	if (err.empty()) err = check_one_dependency(depends.substr(start));

	if (err.empty()) clear_error(where);
	else set_error(where, err);
}

std::string Packager::check_one_dependency(std::string dep) const
{
	std::string::const_iterator i = dep.begin();
	std::string err;

	while (i != dep.end() && (*i) == ' ') i++;
	if (i == dep.end())
	{
		err = "empty dependency, have you got too many commas";
	} else if ((*i) == ')')
	{
		err = "Extra ')' in a dependency";
	} else
	{
		while (i != dep.end() && (*i) != '(' && (*i) != ' ') i++;
		while (i != dep.end() && (*i) == ' ') i++;
		if (i != dep.end())
		{
			if (*i != '(')
			{
				err = "dependency package name must end with a comma or a '('";
			} else
			{
				i++;
				while (i != dep.end() && (*i) == ' ') i++;
				if (i == dep.end() || !((*i) == '=' || (*i) == '<' || (*i) == '>'))
				{
					err = "version operator '=', '<<', '>>', '<=' or '>=' missing";
				} else
				{
					if ((*i) == '<')
					{
						i++;
						if (i == dep.end() || ((*i) != '<' && (*i) != '='))
						{
							err = "'<' must be followed by another '<' or and '='";
						}
					} else if ((*i) == '>')
					{
						i++;
						if (i == dep.end() || ((*i) != '>' && (*i) != '='))
						{
							err = "'>' must be followed by another '>' or and '='";
						}
					}
					if (err.empty())
					{
						if (i != dep.end()) i++;
						while (i != dep.end() && (*i) == ' ') i++;
						if (i == dep.end() || (*i) == ')' || (*i) == ',')
							err = "version number missing";
						else if ((*i) == '>' || (*i) == '<' || (*i) == '=')
							err = "extra symbol in version operator";
					}
					if (err.empty())
					{
						while (i != dep.end() && *i != ')' && (*i) != ' ') i++;
						while (i != dep.end() && *i == ' ') i++;
						if (i == dep.end() || *i != ')') err = "missing ')' or a space in the version number";
					}
				}
			}
		}
	}

	return err;
}

/**
 * Package has been modified
 */
void Packager::modified(bool modified)
{
	if (_modified != modified)
	{
		_modified = modified;
	}
}

/**
 * Set the error message for a package item
 */
void Packager::set_error(PackageItem where, const std::string message)
{
    if (_errors[where].empty()) _error_count++;
    _errors[where] = message;
}

/**
 * Clear the error message for a package item
*/
void Packager::clear_error(PackageItem where)
{
    if (!_errors[where].empty())
    {
       _error_count--;
       _errors[where] = "";
    }
}

/**
 * Read control file from disc
 *
 * @param filename filename to read
 */
void Packager::read_control(const std::string &filename)
{
	std::ifstream cf(filename.c_str());
	read_control(cf);
}

/**
 * Read items from control record.
 *
 * This is based on read routine from LibPkg
 *
 * Throws an PackageFormatException if there is a syntax error or
 * a field type this program doesn't understand
 */
void Packager::read_control(std::istream &in)
{
	std::string name, value;
	bool done=false;

	while (in&&!done)
	{
		// Get line from input stream.
		std::string line;
		getline(in,line);

		// Convert to sequence.
		std::string::const_iterator first=line.begin();
		std::string::const_iterator last=line.end();

		// Strip trailing spaces.
		while ((last!=first)&&isspace(*(last-1))) --last;


		if ((first==last)||isspace(*first))
		{
			// Line is blank or begins with a space:
			// Skip leading spaces.
			std::string::const_iterator p=first;
			while ((p!=last)&&isspace(*p)) ++p;
			if (p==last)
			{
				// Line is blank (or contains only spaces):
				done=true;
			}
			else
			{
				// Line is a continuation line:
				// Check whether there is a field to continue.
				if (name.empty())
					throw PackageFormatException(
						"Continuation line not allowed here in RiscPkg/Control");

				// If line contains nothing but a period
				// then skip that character.
				if ((p+1==last)&&(*p=='.')) ++p;

				// Append continuation line to field.
				value+=std::string("\n");
				value+=std::string(p,last);
			}
		}
		else
		{
			if (!name.empty()) set_control_field(name, value);

			// Line does not begin with a space:
			// Parse fieldname.
			std::string::const_iterator p=first;
			while ((p!=last)&&(*p!=':'))
			{
				if (isspace(*p))
					throw PackageFormatException("Syntax error in RiscPkg/Control");
				++p;
			}
			name=std::string(first,p);

			// Parse colon at end of fieldname.
			if ((p!=last)&&(*p==':')) ++p;
			else throw PackageFormatException("':' expected in RiscPkg/Control");

			// Skip spaces.
			while ((p!=last)&&isspace(*p)) ++p;

			// Set beginning of value
			value = std::string(p, last);

		}
	}

	if (!name.empty()) set_control_field(name, value);
}

/**
 * Set control field with it's value.
 *
 * throws PackageFormatException if this program doesn't understand the field name.
 */
void Packager::set_control_field(std::string name, std::string value)
{
	if (name.compare("Package") == 0)
	{
		package_name(value);
	} else if (name.compare("Version") == 0)
	{
		std::string::size_type rpos = value.rfind('-');
		if (rpos == std::string::npos)
		{
			version(value);
			package_version("");
		} else
		{
			version(value.substr(0, rpos));
			package_version(value.substr(rpos+1));
		}
	} else if (name.compare("Section") == 0)
	{
		section(value);
	} else if (name.compare("Priority") == 0)
	{
		priority(value);
	} else if (name.compare("Maintainer") == 0)
	{
		maintainer(value);
	} else if (name.compare("Standards-Version") == 0)
	{
		standards_version(value);
	} else if (name.compare("Description") == 0)
	{
		std::string::size_type eolpos = value.find('\n');
		if (eolpos == std::string::npos)
		{
			summary(value);
		} else
		{
			summary(value.substr(0, eolpos));
			description(value.substr(eolpos+1));
		}
	} else if (name.compare("Licence") == 0)
	{
		licence(value);
	} else if (name.compare("Depends") == 0)
	{
	    depends(value);
	} else if (name.compare("Recommends") == 0)
	{
	    recommends(value);
	} else if (name.compare("Suggests") == 0)
	{
	   suggests(value);
	} else if (name.compare("Conflicts") == 0)
	{
	   conflicts(value);
	} else
	{
		throw PackageFormatException("Unable to process field '" + name + "' in RiscPkg/Control");
	}
}

/**
 * Compare the passed path with the item name and update it to be the
 * item that is being installed.
 *
 * @param install_item item name to update
 * @param item_name name of item to compare with current install_item
 * @param can_grow starts as true and is changed to false when the install_item
 *        is stopped from getting any larger.
 */
void Packager::set_install_item(std::string &install_item, const std::string &item_name, bool &can_grow)
{
	std::string::size_type install_item_size = install_item.size();
	std::string::size_type item_size = item_name.size();
	std::string::size_type match_pos;

	match_pos = std::string::npos;
	if (install_item_size == 0)
	{
		install_item = item_name;
	}
	else if (item_size <= install_item_size)
	{
		if (item_name == install_item.substr(0, item_size))
		{
			if (item_size != install_item.size()
				&& install_item[item_size] != '/')
			{
				can_grow = false;
				std::string::size_type pos = item_name.rfind('/');
				install_item = item_name.substr(0, pos);
			}
		} else
		{
			match_pos = item_name.rfind('/');
		}
	} else if (can_grow)
	{
		if (item_name.substr(0, install_item_size) == install_item)
		{
			if (item_name[install_item_size] == '/')
			{
				install_item = item_name;
			} else
			{
				match_pos = install_item.rfind('/');
			}
		} else
		{
			match_pos = install_item.rfind('/');
		}
	}

	if (match_pos != std::string::npos)
	{
		while (match_pos != std::string::npos
				&& item_name.substr(0, match_pos) != install_item.substr(0, match_pos)
			   )
		{
			match_pos = item_name.rfind('/',match_pos-1);
		}

		// Should at least match at base dir level
		if (match_pos == std::string::npos)
		{
			std::string msg;
			msg = "Only Install of one item (file or folder) supported. Found '";
			msg += install_item;
			msg += "' and '" + item_name + "'";
			throw PackageFormatException(msg);
		} else
		{
			install_item = item_name.substr(0, match_pos);
			can_grow = false;
		}
	}
}


/**
 * Convert filename from within a zip to a RISC OS filename
 */
std::string Packager::zip_to_riscos_name(const std::string &zipname) const
{
	std::string roname(zipname);
	std::string::size_type pos = 0;
	while ((pos = roname.find_first_of("./", pos + 1)) != std::string::npos)
	{
		if (roname[pos] == '.') roname[pos] = '/';
		else roname[pos] = '.';
	}

	return roname;
}

/**
 * Convert a RISC OS filename  to filename within a zip
 */
std::string Packager::riscos_to_zip_name(const std::string &riscosname) const
{
	// Currently the transformation is identical, but it
	// may change in future.
	return zip_to_riscos_name(riscosname);
}


/**
 * Save packager
 *
 * @param filename file name to save package as
 * @param error option string to be updated with any error message
 *
 * returns true if successful
 */
bool Packager::save(std::string filename, std::string *error /*=nullptr*/)
{
	CZipArchive zip;
	bool ok = false;

	if (error) error->clear();

	try
	{
		zip.Open(filename.c_str(), CZipArchive::zipCreate);

		write_control(zip);
		write_copyright(zip);

		for (ItemToPackage &item_to_package : _items_to_package)
		{
			tbx::Path files(item_to_package.source());

			// Set size of base file name so it can be removed from zip filenames
			_base_dir_size = files.parent().name().length() + 1;
			std::vector<std::pair<tbx::Path, tbx::PathInfo> > file_list;

			tbx::PathInfo root_info;
			if (!files.path_info(root_info))
			{
				std::string msg("Unable to read file/directory ");
				msg += item_to_package.source();
				throw PackageCreateException(msg);
			}

			if (root_info.directory())
			{
				get_file_list(files, file_list);
			} else
			{
			    if (root_info.image_file())
			    {
			       // Image file systems don't by default give a file type
			       // so re-read it and calculate
			       files.raw_path_info(root_info, true);
			    }
				file_list.push_back(std::pair<tbx::Path, tbx::PathInfo>(files, root_info));
			}

			unsigned int last_percent = 2;
			unsigned int total_files = file_list.size();

			for (unsigned int i = 0; i != total_files; i++)
			{
				copy_file(zip, file_list[i].first, file_list[i].second, item_to_package.install_to());
				unsigned int pc = i * 100 / total_files;
				if (pc > last_percent)
				{
					last_percent = pc;
				}
			}
		}

		zip.Close();

	    ok = true;

	} catch(CZipException &e)
	{
		std::string errmsg("Failed to create zip file: ");
		std::string desc =e.GetErrorDescription();
		errmsg += desc;
		if (error) *error = errmsg;tbx::report_error(errmsg.c_str());
	} catch(PackageCreateException &e)
	{
		if (error) *error = e.what();
	} catch(std::bad_alloc &bae)
	{
		if (error) *error = "Unable to allocate enough memory to create package";
	} catch(...)
	{
		if (error) *error = "Unexpected exception thrown during package creation";
	}

	return ok;
}

/**
 * Return text of the control file
 *
 * @return control text
 */
std::string Packager::control_as_text() const
{
	std::ostringstream os;

	if (!_package_name.empty())
	    os << "Package: " << _package_name << std::endl;
	if (!_version.empty())
	    os << "Version: " << _version << "-" << _package_version << std::endl;
	if (!_section.empty())
	    os << "Section: " << _section << std::endl;
	if (!_priority.empty())
	     os << "Priority: " << _priority << std::endl;
	if (!_maintainer.empty())
	   os << "Maintainer: " << _maintainer << std::endl;
	if (!_standards_version.empty())
	   os << "Standards-Version: " << _standards_version << std::endl;
	if (!_licence.empty())
	   os << "Licence: " << _licence << std::endl;
	if (!_summary.empty())
		os << "Description: " << _summary << std::endl;
	if (!_description.empty())
	{
	    if (_summary.empty()) os << "Description: ";
  		std::string::size_type solpos = 0, eolpos, wspos;
  		int blank_line = 0;
		while (solpos < _description.size()
			&& (eolpos = _description.find('\n', solpos))!=std::string::npos
			)
		{
			wspos = solpos;
			while (wspos < eolpos && _description[wspos] == ' ') wspos++;
			if (eolpos == wspos) blank_line++;
			else
			{
				while (blank_line > 0) { blank_line--; os << " ." << std::endl;}
			    os << " " << _description.substr(solpos, eolpos - solpos) << std::endl;
			}
			solpos = eolpos+1;
		}
		if (solpos < _description.size())
		{
			while (blank_line > 0) { blank_line--; os << " ." << std::endl;}
			os << " "  << _description.substr(solpos) << std::endl;
		}
	}

	bool write_comps = true;
	for (const ItemToPackage &item_to_package : _items_to_package)
	{
		if (item_to_package.component_flags() != CF_None)
		{
			if (write_comps)
			{
				os << "Components: ";
				write_comps = false;
			} else
			{
				os << ",";
			}

			os << item_to_package.component() << " (Movable)";
		}
	}
	if (!write_comps) os << std::endl;

    if (!_depends.empty())
      os << "Depends: " << _depends << std::endl;
    if (!_recommends.empty())
      os << "Recommends: " << _recommends << std::endl;
    if (!_suggests.empty())
      os << "Suggests: " << _suggests << std::endl;
    if (!_conflicts.empty())
      os << "Conflicts: " << _conflicts << std::endl;

    return os.str();
}

/**
 * Write control record to given stream
 */
void Packager::write_control(CZipArchive &zip) const
{

	write_text_file(zip, "RiscPkg/Control", control_as_text());
}


/**
 * Write the copyright file
 */
void Packager::write_copyright(CZipArchive &zip) const
{
	write_text_file(zip, "RiscPkg/Copyright", _copyright.c_str());
}

/**
 * Write a text file with the given text to the zip file
 */
void Packager::write_text_file(CZipArchive &zip, const char *filename, std::string text) const
{
	CZipFileHeader fhead;
	fhead.SetFileName(filename);
	fhead.SetModificationTime(time(NULL));

	RISCOSZipExtra textextra(0xFFF);

    // Local entry
	CZipExtraData *extra = fhead.m_aLocalExtraData.CreateNew(textextra.tag());
    extra->m_data.Allocate(textextra.size());
	memcpy(extra->m_data, textextra.buffer(), textextra.size());
	// Central directory entry
	extra = fhead.m_aCentralExtraData.CreateNew(textextra.tag());
    extra->m_data.Allocate(textextra.size());
	memcpy(extra->m_data, textextra.buffer(), textextra.size());

	zip.OpenNewFile(fhead);
	zip.WriteNewFile(text.c_str(), text.size());
	zip.CloseNewFile();
}

/**
 * Get list of files to copy
 *
 * Recurses down directories
 */
void Packager::get_file_list(const tbx::Path &dirname, std::vector<std::pair<tbx::Path, tbx::PathInfo> > &file_list) const
{
	std::vector<std::string> dirnames;

	for (tbx::PathInfo::Iterator i = tbx::PathInfo::begin(dirname, "*");
	        i != tbx::PathInfo::end(); ++i)
	{
		tbx::PathInfo entry(*i);

		if (entry.directory())
		{
			// Go down directories after processing all files
			dirnames.push_back(entry.name());
		} else
		{
			tbx::Path filename(dirname , entry.name());
			file_list.push_back(std::pair<tbx::Path, tbx::PathInfo>(filename, entry));
		}
	}

	std::vector<std::string>::iterator i;
	for (i = dirnames.begin(); i != dirnames.end(); ++i)
	{
		tbx::Path subdirname(dirname, (*i));
		get_file_list(subdirname, file_list);
	}
}

/**
 * Copy files from specified directory to zip file.
 */
void Packager::copy_files(CZipArchive &zip, const tbx::Path &dirname, const std::string &install_to) const
{
	std::vector<std::string> dirnames;

	for (tbx::PathInfo::Iterator i = tbx::PathInfo::begin(dirname, "*");
	        i != tbx::PathInfo::end(); ++i)
	{
		tbx::PathInfo entry(*i);
		if (entry.directory())
		{
			// Go down directories after processing all files
			dirnames.push_back(entry.name());
		} else
		{
			tbx::Path filename(dirname , entry.name());
			copy_file(zip, filename, entry, install_to);
		}
	}

	std::vector<std::string>::iterator i;
	for (i = dirnames.begin(); i != dirnames.end(); ++i)
	{
		tbx::Path subdirname(dirname, (*i));
		copy_files(zip, subdirname, install_to);
	}
}

/**
 * Copy a single file to the archive
 */
void Packager::copy_file(CZipArchive &zip, const tbx::Path &filename, const std::string &install_to) const
{
	tbx::PathInfo entry;
	filename.path_info(entry);

	copy_file(zip, filename, entry, install_to);
}

/**
 * Copy a single file and its attribute to the archive
 */
void Packager::copy_file(CZipArchive &zip, const tbx::Path &filename, tbx::PathInfo &entry, const std::string &install_to) const
{
	// swap "." and slashes in name to put in zip file
	std::string nameinzip = install_to + "." + filename.name().substr(_base_dir_size);
	nameinzip = riscos_to_zip_name(nameinzip);

	CZipFileHeader fhead;
	fhead.SetFileName(nameinzip.c_str());

	if (entry.has_file_type())
	{
		long long csecs_since_1900 = entry.modified_time().centiseconds();
		long long secs_between = 25567; // days
		secs_between *= 24 * 60 * 60; // seconds
		time_t secs_since_1970 = (time_t)(csecs_since_1900/100 - secs_between);

		fhead.SetModificationTime(secs_since_1970);
	} else
	{
	    fhead.SetModificationTime(time(NULL));
	}

	RISCOSZipExtra extra(entry);

    // Local filetype extra data
	CZipExtraData *extra_data = fhead.m_aLocalExtraData.CreateNew(extra.tag());
    extra_data->m_data.Allocate(extra.size());
	memcpy(extra_data->m_data, extra.buffer(), extra.size());
	// Central Directory filetype extra data
	extra_data = fhead.m_aCentralExtraData.CreateNew(extra.tag());
    extra_data->m_data.Allocate(extra.size());
	memcpy(extra_data->m_data, extra.buffer(), extra.size());

	zip.OpenNewFile(fhead);

	/* Copy file data */
	int filesize = entry.length();
	if (filesize > 0)
	{
		std::ifstream from_file(filename.name().c_str());
		while (filesize > copy_buffer_size)
		{
			from_file.read(copy_buffer, copy_buffer_size);
			zip.WriteNewFile(copy_buffer, copy_buffer_size);
			filesize -= copy_buffer_size;
		}
		if (filesize > 0)
		{
			from_file.read(copy_buffer, filesize);
			zip.WriteNewFile(copy_buffer, filesize);
		}
	}

	zip.CloseNewFile();
}


/**
 * Read item from zip file into a string.
 *
 * @param zip CZipArchive to read item from
 * @param index index of item to read
 * @param item string to be updated with data extracted from zipfile
 *
 * @returns true if successful and data is updated
 */
bool Packager::read_zip_item(CZipArchive &zip, int index, std::string &item)
{
	CZipMemFile mf;
	if (zip.ExtractFile(index, mf))
	{
		int len = mf.GetLength();

		char *data = (char *)mf.Detach();
		item.assign(data, len);
		free(data);

		return true;
	}

	return false;
}

/**
 * Read zip item to a string.
 *
 * @param zipfile to read
 * @param zipname item to read in zip file (case insensitive)
 * @param resulting data
 * @returns true if data is read
 */
bool Packager::read_zip_item(const std::string &zipfile, const std::string &zipname, std::string &data)
{
	CZipArchive zip;

	try
	{
		zip.Open(zipfile.c_str());
		zip.EnableFindFast(true);
		int index = zip.FindFile(zipname.c_str(), CZipArchive::ffNoCaseSens);
		if (index == ZIP_FILE_INDEX_NOT_FOUND) return false;

		return read_zip_item(zip, index, data);

	} catch(CZipException &e)
	{
		// std::cout << "Not a valid zip file " << e.GetErrorDescription() << std::endl;
		// Not a valid zip file so assume its a file
		// to be packaged
		return false;
	}
}

/**
 * Return the standard file leaf name for saving to disc.
 *
 * This is the package name "_" version "-" package version with invalid
 * filename characters converted
 */
std::string Packager::standard_leafname() const
{
	std::string leafname(_package_name + "_" + _version + "-" + _package_version);
	// Just "." to "/" conversion for now
	std::string::size_type dot_pos;
	while ((dot_pos = leafname.find('.'))!= std::string::npos) leafname[dot_pos] = '/';
	return leafname;
}

/**
 * Compare the files for this package with an existing package.
 *
 * @param pkgfilename full path to package to compare to
 * @param diff optional string to give reason packages were different
 * @returns true if the packages are the same
 */
bool Packager::same_as(const std::string &pkgfilename, std::string *diff /* = nullptr */) const
{
	CZipArchive zip_compare;
	if (!zip_compare.Open(pkgfilename.c_str(), CZipArchive::OpenMode::zipOpenReadOnly))
	{
		// Not the same if the old file doesn't exist
		if (diff) *diff = pkgfilename + " does not exist";
		return false;
	}

    std::map<std::string, int> zip_contents;
    int num_objects = zip_compare.GetCount();
    for (int i = 0; i < num_objects; ++i)
    {
    	CZipFileHeader *fileInfo = zip_compare.GetFileInfo(i);
    	if (!fileInfo->IsDirectory())
    	{
    		zip_contents[fileInfo->GetFileName()] = fileInfo->m_uUncomprSize;
    	}
    }

    // First check control/copyright content size changes
    if (!compare_file_text_size(zip_contents, "RiscPkg/Copyright", _copyright, diff))
    {
    	return false;
    }

    std::string control = control_as_text();
    if (!compare_file_text_size(zip_contents, "RiscPkg/Control", control, diff))
    {
    	return false;
    }

    // Now check control/copyright for content changes
    if (!file_text_is_same(zip_compare, "RiscPkg/Copyright", _copyright, diff))
    {
    	return false;
    }

    if (!file_text_is_same(zip_compare, "RiscPkg/Control", control, diff))
    {
    	return false;
    }

    // No longer interested in these two files
    zip_contents.erase("RiscPkg/Copyright");
    zip_contents.erase("RiscPkg/Control");


    // Map of disc file to package file
    std::map<std::string, std::string> disc_file_list;


    // Quick check for existence/file sizes - building list of files on disc
    for (ItemToPackage const &item : _items_to_package)
    {
    	std::string zip_install_to = item.install_to() + "." + tbx::Path(item.source()).leaf_name();
    	if (!build_disc_list(disc_file_list, zip_contents, item.source(), riscos_to_zip_name(zip_install_to), diff))
    	{
    		return false;
    	}
    }

    // zip contents should be empty now if all files in zip are in new package
    if (!zip_contents.empty())
    {
    	if (diff)
		{
    		*diff = tbx::to_string((int)zip_contents.size())
    		+ " files removed, first is "
			+ zip_contents.begin()->first;
		}
    	return false;
    }

    // Check disc file contents against zip file
    for (auto &disc_entry : disc_file_list)
    {
    	if (!file_is_same(zip_compare, disc_entry.first, disc_entry.second, diff))
    	{
    		return false;
    	}
    }

   return true;
}


/**
 * Compare size of zip file entry to size of give text
 * @param zip_contents map of zip file contents to size
 * @param zip_filename name of file to check
 * @param text to check against
 * @param diff pointer to description of mismatch (if any)
 * @returns true if text size and zip file are the same size
 */
bool Packager::compare_file_text_size(std::map<std::string, int> &zip_contents, const std::string &zip_filename, const std::string &text, std::string *diff) const
{
	if (zip_contents[zip_filename] == (int)text.size())
	{
		return true;
	} else
	{
		if (diff) *diff = zip_filename + " size changed";
		return false;
	}
}

/**
 * Compare the contents of a file in the archive to a string
 *
 * @param zip_compare archive containing file to compare
 * @param zip_filename file name in archinve
 * @param text text to compare
 * @param diff pointer to description of mismatch (if any)
 * @return true if zip file contents and text match.
 */
bool Packager::file_text_is_same(CZipArchive &zip_compare, const std::string &zip_filename, const std::string &text, std::string *diff) const
{
	ZIP_INDEX_TYPE index = zip_compare.FindFile(zip_filename.c_str());
	if (index == ZIP_FILE_INDEX_NOT_FOUND)
	{
		if (diff) *diff = zip_filename + " does not exist";
		return false;
	}
	if (!zip_compare.OpenFile(index))
	{
		if (diff) *diff = zip_filename + " could not be opened";
		return false;
	}

	std::unique_ptr<char> buf(new char[text.size() + 2]);
	int num_read = zip_compare.ReadFile((void *)buf.get(), text.size()+2);
	zip_compare.CloseFile();
	if (num_read != (int)text.size())
	{
		if (diff) *diff = zip_filename + " different size in zip";
		return false;
	}

	if (text.compare(0, text.size(), buf.get(), text.size()) == 0)
	{
		return true;
	} else
	{
		char *bc = buf.get();
		int i = 0;
		for (char ch : text)
		{
			i++;
			if (*bc != ch) std::cout << "cd " << i << " " << *bc << " != " << ch << std::endl;
			bc++;
		}
		if (diff) *diff = zip_filename + " contents changed";
		return false;
	}
}

/**
 * Build list of files to package (matched to name in zipfile)
 *
 * This routine exits before the list is build if a file is not
 * found in the zip or the file lengths do not match.
 *
 * @param disc_file_list list to update
 * @param zip_contents list of zip file contents (found files are removed)
 * @param disc_dirname name of directory on disc
 * @param zip_dirname name of directory in the zip
 * @param diff pointer to description of missing file (if any)
 * return true if full list is built
 */
bool Packager::build_disc_list(
		std::map<std::string, std::string> &disc_file_list,
		std::map<std::string, int> &zip_contents,
		const std::string &disc_dirname,
		const std::string &zip_dirname,
		std::string *diff) const
{
	std::vector<std::string> subdirs;
	std::string disc_filename;
	std::string zip_filename;
//	std::cout << "Comparing " << disc_dirname << " with " << zip_dirname << std::endl;
	tbx::PathInfo dir(tbx::Path(disc_dirname));
	for (tbx::PathInfo::Iterator it = tbx::PathInfo::begin(disc_dirname); it != tbx::PathInfo::end(); ++it)
	{
		if (it->directory())
		{
			subdirs.push_back(it->name());
		} else
		{
			disc_filename = disc_dirname + "." + it->name();
			zip_filename = zip_dirname + "/" + riscos_to_zip_name(it->name());
			auto found_in_zip = zip_contents.find(zip_filename);
			if (found_in_zip == zip_contents.end())
			{
				if (diff) *diff = "new file " + disc_filename;
				return false;
			} else
			{
				if (it->length() != found_in_zip->second)
				{
					if (diff) *diff = "file size changed " + disc_filename;
					return false;
				}
				// Erase file from contents list we can see if any files have
				// been deleted
				zip_contents.erase(found_in_zip);
				disc_file_list[disc_filename] = zip_filename;
			}
		}
	}

	for(std::string &subdir : subdirs)
	{
		if (!build_disc_list(disc_file_list, zip_contents,
				disc_dirname + "." + subdir,
				zip_dirname + "/" + riscos_to_zip_name(subdir),
				diff))
		{
			return false;
		}
	}

    return true;
}

/**
 * Check if the file contents are the same as in a zip file
 *
 * @param zip_compare archive with file to compare
 * @param disc_filename name on disc
 * @param zip_filename name in zip archive
 * @param diff string update with message if file is not the same
 * @param true if file contents are the same
 */
bool Packager::file_is_same(CZipArchive &zip_compare, const std::string &disc_filename, const std::string &zip_filename, std::string *diff) const
{
	const int BUFFER_SIZE = 16384;
	static char disc_buffer[BUFFER_SIZE];
	static char zip_buffer[BUFFER_SIZE];

	ZIP_INDEX_TYPE index = zip_compare.FindFile(zip_filename.c_str());
	if (index == ZIP_FILE_INDEX_NOT_FOUND)
	{
		if (diff) *diff = zip_filename + " does not exist";
		return false;
	}

	if (!zip_compare.OpenFile(index))
	{
		if (diff) *diff = zip_filename + " could not be opened";
		return false;
	}

	std::ifstream check(disc_filename, std::ios::binary);
	if (!check)
	{
		zip_compare.Close();
		if (diff) *diff = zip_filename + " could not be opened";
		return false;
	}

	int zip_read;
	bool same = true;

	while (check && same)
	{
		check.read(disc_buffer, BUFFER_SIZE);
		zip_read = zip_compare.ReadFile((void *)zip_buffer, BUFFER_SIZE);
		if (zip_read != check.gcount())
		{
			same = false;
			if (diff) *diff = disc_filename + " read bytes size mismatch";
		} else
		{
			if (std::memcmp(disc_buffer, zip_buffer, zip_read) != 0)
			{
				same = false;
				if (diff) *diff = disc_filename + " contents changed";
			}
		}
	}

	if (same && zip_compare.ReadFile((void *)zip_buffer, 1) != 0)
	{
		if (diff) *diff = disc_filename + " read bytes size mismatch";
		same = false;
	}
	zip_compare.CloseFile();

    return same;
}
