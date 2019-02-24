/*
 * main.cc
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

#include <iostream>
#include <string>
#include <fstream>
#include <map>
#include <set>
#include "Catalogue.h"
#include "Packager.h"
#include "version.h"
#include "Log.h"
#include <tbx/path.h>
#include <tbx/stringutils.h>
#include <unixlib/local.h>

// Program control strings
std::string s_cat_filename("catalogue/csv"); // Path added below
std::string s_games_dir("$.Games");
std::string s_extras_dir("$.Games.Extras");
std::string s_logs_dir("Logs"); // Path added below
std::string s_copyright_filename("$.Games.Copyright");
std::string s_packages_dir("$.Packages");
std::string s_release_packages = "release";
std::string s_beta_packages = "beta";
std::string s_maintainer = "Jonathan Abbott<jon@jaspp.org.uk>";
std::string s_base_install("Apps.Games");
/** List of characters that should not be in the package name */
const char *s_pkgname_invalid_chars = " :'<>*?";

// RISC OS filer doesn't like spaces, so they will be replaced with hard spaces
const char HARD_SPACE = '\xA0';

// Work variables
/** Standard copyright text for games */
std::string s_standard_copyright;
/** Map of currently packages to their version */
std::map<std::string, std::string> s_current_packages;
/** Lookup from catalogue ID to game directory */
std::map<std::string, std::string> s_dir_lookup;
/** Check to ensure package names are unique */
std::set<std::string> s_used_pkgnames;
/** Check to ensure default install directories do not clash */
std::set<std::string> s_used_components;

/** Logging */
Log s_log;

// Functions in this file
static void package_extras();
static void package_extra(const std::string &extra_dir);
static void package_game(const CatEntry &entry);
static void check_and_save_package(Packager &pkg, Log::PackageContext &log_context, bool released);
static void current_package_list(const std::string &from_dirname);
static void create_dir_lookup();
static bool validate_pkgname(const std::string &pkgname, std::string *errmsg = nullptr);
static bool calc_version(const std::string &pkg_dir, std::string &version);

/**
 * Main program entry point
 */
int main(int argc, char *argv[0])
{
	std::string app_dir;
	{
		// Full paths is passed to unixlib programs in args[0] in unix format
		// so convert to RISC OS format and strip the app name.
		char ropath[256];
		int filetype;
		__riscosify_std(argv[0], 0, ropath, 256, &filetype);
		app_dir = ropath;
		app_dir.erase(app_dir.rfind('.'));
	}
	s_logs_dir = app_dir + "." + s_logs_dir;
	s_cat_filename = app_dir + "." + s_cat_filename;

	tbx::Path(s_logs_dir).create_directory();
	std::cout << "Logs directory " << s_logs_dir << std::endl;
	s_log.start(s_logs_dir, "Packaging run");

	std::cout << "Reading standard copyright text..." << std::flush;
	s_log.message("Reading copyright text from " + s_copyright_filename);
	int cp_length = 0;
	char *cp_text = tbx::Path(s_copyright_filename).load_file(&cp_length);
	if (!cp_text)
	{
		s_log.fatal_error("failed to load copyright text");
		std::cout << "failed to load" << std::endl;
		return -1;
	}
	s_standard_copyright.assign(cp_text, cp_length);
	delete [] cp_text;
	std::cout << "loaded" << std::endl;
	s_log.message("Copyright text loaded");

	s_log.message("Reading catalogue " + s_cat_filename);
	std::cout << "Reading catalogue from " << s_cat_filename << "..."  << std::flush;
	Catalogue cat;
	if (!cat.load(s_cat_filename))
	{
		std::cout << "load failed" << std::endl;
		s_log.fatal_error("Failed to load catalogue");
		return -2;
	}
	s_log.message("Catalogue loaded");
	std::cout << "loaded" << std::endl;

	s_log.message("Creating list of current packages");
	std::cout << "Creating list of current packages..." << std::flush;
	current_package_list(s_packages_dir + "." + s_release_packages);
	current_package_list(s_packages_dir + "." + s_beta_packages);
	std::cout << "done" << std::endl;
	s_log.message(s_current_packages.size(),"current packages found");

	s_log.message("Creating game to directory mapping");
	std::cout << "Creating game to directory mapping..." << std::flush;
	create_dir_lookup();
	std::cout << s_dir_lookup.size() << " game directories found." << std::endl;
	s_log.message(s_dir_lookup.size(),"game directories found.");

	/*** Debug code if you want to check directories found
	for (auto x : s_dir_lookup)
	{
		std::cout << x.first << " is at " << x.second << std::endl;
	}
	***/

	package_extras();

	// Ensure package directories are created
	tbx::Path(s_packages_dir).create_directory();
	tbx::Path(s_packages_dir, s_release_packages).create_directory();
	tbx::Path(s_packages_dir, s_beta_packages).create_directory();

	s_log.message(cat.size(), "packages to check/create");
	std::cout << "Creating " << cat.size() << " packages" << std::endl;
	int row = 0;
	for (const CatEntry &entry : cat)
	{
		std::cout << ++row << " ";
		package_game(entry);
	}

	s_log.end("End of packaging");

	return 0;
}

/**
 * Package ADFFS
 */
void package_extras()
{
	tbx::Path extras_dir(s_extras_dir);
	std::cout << "Create extra packages from " << s_extras_dir << std::endl;
	for (std::string &fsobject : extras_dir)
	{
		package_extra(s_extras_dir + "." + fsobject);
	}
}

void package_extra(const std::string &extra_dir)
{
	if (!tbx::Path(extra_dir + ".Control").exists())
	{
		// Not a package directory
		return;
	}

	std::string pkgname(tbx::Path(extra_dir).leaf_name());

    Log::PackageContext log_context(s_log, pkgname, "Extra Package");

	std::cout << "Packaging " << pkgname << "...";

	int len;
	char *data = tbx::Path(extra_dir +".Copyright").load_file(&len);
	if (data == nullptr)
	{
		std::cout << "Missing 'Copyright' file" << std::endl;
		log_context.error("Missing 'Copyright' file");
		return;
	}

	std::string copyright(data, len);
	delete [] data;

	Packager pkg;
	pkg.package_name(pkgname);
	try
	{
		pkg.read_control(extra_dir + ".Control");
	} catch(std::exception &rcerr)
	{
		std::string err("Error reading Control file ");
		err += rcerr.what();
		std::cout << err << std::endl;
		log_context.error(err);
		return;
	}


	pkg.copyright(copyright);
	std::string version = pkg.version();
	if (!version.empty() && version[0] == '{')
	{
		if (!calc_version(extra_dir, version))
		{
			std::cout << "Invalid version in Control " << version << std::endl;
			log_context.error("Invalid version in Control " + version);
			return;
		}
	}

	if (version.empty()) version = "0";
	pkg.version(version);
	if (pkg.package_version().empty()) pkg.package_version("1");

	if (pkg.section().empty()) pkg.section("Games");
	if (pkg.maintainer().empty()) pkg.maintainer(s_maintainer);
	if (pkg.licence().empty()) pkg.licence("Non free");

	tbx::Path extra_path(extra_dir);
	std::vector<std::string> special_dir_list;
	for (std::string &fsobject : extra_path)
	{
		if (fsobject == "Copyright" || fsobject == "Control")
		{
			// Ignore
		} else if (fsobject == "!Boot" || fsobject == "Boot")
		{
			special_dir_list.push_back(fsobject);
		} else
		{
			pkg.set_item_to_package(ItemToPackage(extra_dir + "." + fsobject, s_base_install, CF_Movable));
		}
	}
	for (std::string &spec_dir : special_dir_list)
	{
		tbx::Path spec_path(extra_path, spec_dir);
		for (std::string &fsobject : spec_path)
		{
			pkg.set_item_to_package(ItemToPackage(spec_path.child(fsobject), spec_dir, CF_None));
		}
	}

	// Saving all extras as beta for now
	check_and_save_package(pkg, log_context, false);
}

/**
 * Package a single game
 *
 * @param entry catalogue entry with details
 */
void package_game(const CatEntry &entry)
{
	std::string pkgname(entry.at("Package name (max 31 chars)"));
    std::string id(entry.at("ID") + ("00" + entry.at("Sub ID")).substr(0,2));
	std::string title(entry.at("Title"));

    std::cout << id << " " << title << "..." << std::flush;

    std::string full_name;
    full_name += title;
    full_name += " (" + entry.at("Date") + ")";
    full_name += " (" + entry.at("Publisher") + ")";

    Log::PackageContext log_context(s_log, id, full_name);

    if (pkgname.empty())
    {
    	std::cout << "not packaged as no package name" << std::endl;
    	log_context.message("not packaged as no package name");
    	log_context.do_not_package();
    	return;
    }
    std::string name_err;
    if (!validate_pkgname(pkgname, &name_err))
    {
    	std::ostringstream os;
    	os << "Invalid package name '" << pkgname << "'. " << name_err;
    	std::cout << os.str() << std::endl;
    	log_context.error(os.str());
    	return;
    }

    full_name += " F" + id;


    auto found_dir = s_dir_lookup.find(id);
    if (found_dir == s_dir_lookup.end())
    {
    	log_context.error("Unable to find game directory");
    	std::cout << "Unable to find game directory" << std::endl;
    	return;
    }

    std::string game_dir_name = found_dir->second;

    tbx::Path game_dir(s_games_dir, game_dir_name);
    if (!game_dir.directory())
    {
    	log_context.error("Invalid directory");
    	std::cout << "Invalid directory " << game_dir << std::endl;
    	return;
    }

    // Build list of files to package
    std::vector<std::string> game_dir_list;
	bool has_control = false;
	bool released = (entry.at("Released") == "Y");

	for (std::string &fsobject : game_dir)
	{
		if (fsobject == "Control")
		{
			has_control = true;
		} else
		{
			game_dir_list.push_back(fsobject);
//			std::cout << std::endl << " *" << fsobject << std::endl;
		}
	}

	Packager pkg;

	if (has_control)
	{
		pkg.read_control(game_dir.child("Control").name().c_str());
	}
	if (pkg.package_name().empty())
	{
		pkg.package_name(pkgname);
	}

	if (pkg.depends().empty())
	{
		if (entry.at("RiscOS 5.x") == "F")
		{
			pkg.depends("ADFFS");
		}
	}

	std::string ver(entry.at("Version"));
	if (ver.empty()) ver = "0";
	pkg.version(ver);
	pkg.package_version("1");

	pkg.summary(full_name);

	if (pkg.description().empty())
	{
		pkg.description("Game description to follow");
	}

	pkg.section("Games");
	pkg.maintainer(s_maintainer);
	pkg.licence("Non free");

	std::string copyright(full_name);
	copyright += "\n\n" + s_standard_copyright;
	pkg.copyright(copyright);

	for (const std::string &fsobject : game_dir_list)
	{
		if (fsobject == "!Boot" || fsobject == "Boot")
		{
			tbx::Path spec_path(game_dir, fsobject);
			for (std::string &sdobject : spec_path)
			{
				pkg.set_item_to_package(ItemToPackage(spec_path.child(sdobject), fsobject, CF_None));
			}
		} else
		{
			ItemToPackage item(game_dir.child(fsobject), s_base_install, CF_Movable);
			if (s_used_components.count(item.component()))
			{
				log_context.message("Default install location already used, using ID to disambiguate");
				item.install_to(s_base_install + "." + id);
			} else
			{
				s_used_components.insert(item.component());
			}
			pkg.set_item_to_package(item);
		}
	}

	check_and_save_package(pkg, log_context, released);
}

/**
 * Final package validity checks and save it if ok
 *
 * @param pkg package to package
 * @param log_context package context
 * @param released - released build
 */
void check_and_save_package(Packager &pkg, Log::PackageContext &log_context, bool released)
{
	std::string pkgname(pkg.package_name());
	// Check package for validity
    if (pkg.error_count())
    {
    	std::cout << "Invalid package" << std::endl;
    	int start = pkg.first_error();
    	int next = start;
    	std::string msg;

    	do
    	{
    		std::cout << "  " << pkg.item_name(next) << " " << pkg.error_text(next) << std::endl;
    		if (!msg.empty()) msg += ", ";
    		msg += pkg.item_name(next) + " " + pkg.error_text(next);
    		next = pkg.next_error(next);
    	} while (next != start);
    	log_context.error("Invalid package - " + msg);
    } else
    {
    	bool save_package = true;
    	auto current = s_current_packages.find(pkgname);
    	if (current == s_current_packages.end())
    	{
    		log_context.message("Creating new package");
    		std::cout << "new";
    		log_context.new_package(true);
    	} else
    	{
    		try
    		{
				pkg::version v(pkg.version() + "-" + pkg.package_version());
				pkg::version old_v(current->second);
				if (v > old_v)
				{
					std::cout << "upgrade (new version)";
					log_context.message("Upgrading due to new version");
				} else
				{
					// Use old version - will increase later
					pkg.version(old_v.upstream_version());
					pkg.package_version(old_v.package_version());
    				std::string lastpkgfile(s_packages_dir + "." + s_release_packages + "." + pkg.standard_leafname());
    			    save_package = false;
    				if (!tbx::Path(lastpkgfile).exists())
    				{
    					lastpkgfile = s_packages_dir + "." + s_beta_packages + "." + pkg.standard_leafname();
    					if (released)
    					{
    						log_context.message("Upgrading beta to release");
    						std::cout << "upgrade (beta to release)";
    						save_package = true;
    					}
    				}

    				if (!save_package)
					{
    					log_context.message("Comparing files with last package");
    					std::string diff;
						save_package = !pkg.same_as(lastpkgfile, &diff);
						if (save_package)
						{
							std::cout << "upgrade (" << diff << ")";
							log_context.message("Upgrading " + diff);
						}
					}
    				if (save_package)
    				{
    					// Update version for new release
    					int new_pv = tbx::from_string<int>(pkg.package_version())+1;
    					pkg.package_version(tbx::to_string(new_pv));
    					log_context.upgrade_package(true);
    				}
				}
    		} catch(pkg::version::parse_error &ve)
    		{
    			std::string msg("Invalid package version format error. Old version ");
    			msg += current->second;
    			msg += ", new version " + pkg.version() + "-" + pkg.package_version();
    			msg += ", error ";
    			msg += ve.what();
    			log_context.error(msg);
    			std::cout << msg << std::endl;
				return; // Bail out
			} catch(std::exception &e)
			{
				log_context.error(std::string("Compare failed ") + e.what());
				std::cout << "Compare failed " << e.what() << std::endl;
				return; // Bail out
			}
    	}

    	std::cout << "..." << std::flush;

    	if (save_package)
    	{
			std::string pkgdir(s_packages_dir);
			std::string type(released ? s_release_packages : s_beta_packages);
			pkgdir += "." + type + ".";
			std::string pkgfile(pkgdir + pkg.standard_leafname());

			log_context.message("Creating/saving package to " + pkgfile);
			std::string errmsg;
			if (pkg.save(pkgfile, &errmsg))
			{
				log_context.message("Created/saved");
				std::cout << "created ";
			} else
			{
				log_context.error("Failed to save/create - " + errmsg);
				std::cout << "failed to create ";
			}
			std::cout << type << " package " << pkgfile << std::endl;
    	} else
    	{
    		std::cout << "is up to date" << std::endl;
    		log_context.message("Package is up to date");
    	}
    }
}

/**
 * Create list of current packages and the latest packaged version
 *
 * @param from_dir - packages from the given directory are added
 *                   to the list
 */
void current_package_list(const std::string &from_dirname)
{
	tbx::Path package_dir(from_dirname);
	// Missing directory
	if (!package_dir.directory()) return;

	for (std::string &fsobject : package_dir)
	{
		std::string::size_type us_pos = fsobject.rfind('_');
		if (us_pos != std::string::npos)
		{
			std::string pkgname = fsobject.substr(0,us_pos);
			std::string ver = fsobject.substr(us_pos+1);
			// Convert slashes back to dots for version
			std::string::size_type slash_pos;
			while ((slash_pos = ver.find("/")) != std::string::npos) ver[slash_pos] = '.';
			auto found = s_current_packages.find(pkgname);
			if (found == s_current_packages.end()
				|| pkg::version(ver) > pkg::version(found->second) )
			{
//				std::cout << "Found " << pkgname << " " << ver << std::endl;
				s_current_packages[pkgname] = ver;
			}
		}
	}
}

/**
 * Create lookup from game ID to game directory name
 */
void create_dir_lookup()
{
	tbx::Path games_dir(s_games_dir);
	for (std::string &fsobject : games_dir)
	{
		std::string::size_type cv_pos = fsobject.rfind(HARD_SPACE);
		if (cv_pos != std::string::npos)
		{
			std::string cat_id(fsobject, cv_pos+1);
			if (cat_id.size() == 8 && cat_id[0] == 'F')
			{
				s_dir_lookup[cat_id.substr(1)] = fsobject;
			}
		}
	}
}

/**
 * Validate a package name
 *
 * @param pkgname name of package
 * @param errmsg pointer to optional failure reason
 * @return true if name is ok
 */
bool validate_pkgname(const std::string &pkgname, std::string *errmsg /*= nullptr*/)
{
    if (pkgname.empty())
    {
    	if (errmsg) *errmsg = "No package name";
    	return false;
    }

    if (pkgname.size() > 31)
    {
    	if (errmsg) *errmsg = "Package name is longer than 31 chars";
    	return false;
    }

	// Package name can't have spaces
	std::string::size_type pos = pkgname.find_first_of(s_pkgname_invalid_chars);
	if (pos != std::string::npos)
	{
    	if (errmsg) *errmsg = std::string("Package name contains invalid characters (") + s_pkgname_invalid_chars + ")";
    	return false;
    }

    if (s_used_pkgnames.count(pkgname))
    {
    	if (errmsg) *errmsg = "Package name has already been used";
    	return false;
    }
    s_used_pkgnames.insert(pkgname);

    return true;
}

/**
 * Calculate version from expression.
 *
 * Functions are:
 *  FromFile(<file_name>,<regex>) - finds first line that matches regex and return the capture
 *
 * @param pkg_dir directory containing the package
 * @param version with expression, updated to calculated version or error message
 * @returns true if successful and version is updated
 */
bool calc_version(const std::string &pkg_dir, std::string &version)
{
	std::string::size_type eofpos = version.rfind('}');
	if (eofpos == std::string::npos)
	{
		version = "'" + version + "' missing closing '}'";
		return false;
	}
	std::string suffix(version, eofpos+1);

	std::string::size_type fbpos = version.find('(');
	std::string::size_type lbpos = version.rfind(')');
	if (fbpos == std::string::npos || lbpos == std::string::npos)
	{
		version = "'" + version + "' missing bracket '(' and/or ')'";
		return false;
	}

	std::string func_name(version,1,fbpos - 1);
	if (func_name != "FromFile")
	{
		version = "'" + version + "' invalid function '" + func_name + "'";
		return false;
	}

	std::string::size_type comma_pos = version.find(",");
	if (comma_pos == std::string::npos)
	{
		version = "'" + version + "' missing comma";
		return false;
	}

	std::string file_name(version, fbpos+1, comma_pos - fbpos - 1);
	std::string pattern(version, comma_pos+1, lbpos - comma_pos - 1);

	std::ifstream file(pkg_dir + "." + file_name);
	if (!file)
	{
		version = "'" + version + "' could not open file '" + pkg_dir + "." + file_name + "'";
		return false;
	}

	/* Did want to use std::regex or boost::regex, but std::regex didn't
	 * seem to want to take any pattern and the current build of
	 * boost::regex include an arm "swp" instruction that doesn't work on ARMv8
	 *
	try
	{
		boost::regex re(pattern);
		boost::smatch match;
		std::string line;
		while (std::getline(file, line))
		{
			if (boost::regex_search(line, match, re))
			{
				version = match[1];
				version += suffix;
				return true;
			}
		}
	} catch(boost::regex_error &err)
	{
		version = "'" + version + "' invalid regular expression '" + pattern + "'. ";
		version += err.what();
		version += tbx::to_string(err.code());
		return false;
	}
	**********/

	/* So instead use a simple thing to get the data that would appear between
	 * the brackets
	 */
	lbpos = pattern.find('(');
	if (lbpos == std::string::npos)
	{
		version = "'" + version + "' missing left bracket in '" + pattern + "'. ";
		return false;
	}
	std::string::size_type rbpos = pattern.find(')', lbpos+1);
	if (rbpos == std::string::npos)
	{
		version = "'" + version + "' missing right bracket in '" + pattern + "'. ";
		return false;
	}
	std::string vprefix(pattern, 0, lbpos);
	std::string vsuffix(pattern, rbpos+1);
	size_t ver_offset = vprefix.size();

	std::string line;
	while (std::getline(file, line))
	{
		std::string::size_type pos = line.find(vprefix);
		if (pos != std::string::npos)
		{
			std::string::size_type epos = line.find(vsuffix, pos+ver_offset);
			if (epos != std::string::npos && epos > pos + ver_offset)
			{
				version = line.substr(pos+ver_offset, epos - pos - ver_offset);
				return true;
			}
		}
	}

	version = "'" + version + "' pattern '" + pattern + "' not found in '" + file_name + "'";
	return false;
}
