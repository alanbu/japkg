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

#ifndef Packager_h
#define Packager_h

#include <string>
#include <map>
#include <vector>
#include <ostream>
#include <istream>

enum PackageItem {
  PACKAGE_NAME,
  VERSION,
  PACKAGE_VERSION,
  SECTION,
  PRIORITY,
  MAINTAINER,
  STANDARDS_VERSION,
  SUMMARY,
  DESCRIPTION,
  LICENCE,
  COPYRIGHT,
  ITEM_TO_PACKAGE,
  INSTALL_TO,
  DEPENDS,
  RECOMMENDS,
  SUGGESTS,
  CONFLICTS,
  COMPONENT_FLAGS,
  NUM_ITEMS // Dummy value to give number of items
  };

class  PackagerTextEndPoint;

class CZipArchive;

namespace tbx
{
   class Path;
   class PathInfo;
}

/**
 * Call back to report errors in package
 */
class PackageErrorListener
{
    public:
       virtual ~PackageErrorListener() {};
       virtual void on_item_error(PackageItem item) = 0;
       virtual void on_item_valid(PackageItem item) = 0;
};

/**
 * Call back to report when package modified status has changed
 */
class PackageModifiedListener
{
public:
	virtual ~PackageModifiedListener() {};

	virtual void modified_changed(bool new_value) = 0;
};

/**
 * Exception thrown if there is an error in the package or it
 * is using feature not supported by this program
 */
class PackageFormatException
{
	std::string _message;
public:
	PackageFormatException(const char *msg) : _message(msg) {};
	PackageFormatException(const std::string &msg) : _message(msg) {};

	const std::string &message() const {return _message;}
};

/**
 * Component flags
 */
enum ComponentFlags
{
	CF_None,
	CF_Movable
	// Implement others as needed
};

/**
 * Class to represent a file or directory to package
 */
class ItemToPackage
{
private:
	std::string _source;
	std::string _install_to;
	unsigned int _component_flags;

public:
	/**
	 * Construct an item to package
	 *
	 * @param source source item (file or directory) on disc
	 * @param install_to directory to install to
	 * @param flags component flags for item
	 */
	ItemToPackage(const std::string &source, const std::string &install_to, unsigned int flags = CF_None ) :
		_source(source), _install_to(install_to), _component_flags(flags)
	{

	}

	const std::string &source() const {return _source;}
	const std::string &install_to() const {return _install_to;}
	void install_to(const std::string &new_path) {_install_to = new_path;}
	unsigned int component_flags() const {return _component_flags;}
	/**
	 * Return the component install location
	 */
	std::string component() const
	{
		std::string::size_type leaf_pos = _source.rfind('.');
		std::string leaf_name = (leaf_pos == std::string::npos) ? _source : _source.substr(leaf_pos+1);
        return _install_to + "." + leaf_name;
	}
};

/**
 * Class to handle all the files and fields for a package
 */
class Packager
{
public:

    private:
       std::string _package_name;
       std::string _version;
       std::string _package_version;
       std::string _section;
       std::string _priority;
       std::string _maintainer;
       std::string _standards_version;
       std::vector<ItemToPackage> _items_to_package;
       std::string _summary;
       std::string _description;
       std::string _licence;
       std::string _depends;
       std::string _recommends;
       std::string _suggests;
       std::string _conflicts;
       std::string _copyright;

       bool _modified;

       int _error_count;
       std::string _errors[NUM_ITEMS];
       static const char *_item_names[NUM_ITEMS];

       // work variables for save
       int _base_dir_size;

    public:
       Packager();
       ~Packager();

       bool save(std::string filename, std::string *error = nullptr);

       bool modified() const {return _modified;}
       void modified(bool modified);

       int error_count() const {return _error_count;}
       int first_error() const {return next_error(-1);}
       int next_error(int i) const;
       const std::string &error_text(int i) const {return _errors[i];}

       /**
        * Name of each package item
        */
       std::string item_name(int i) const {return _item_names[i];}

       void package_name(std::string value);
       void version(std::string value);
       void package_version(std::string value);
       void section(std::string value);
       void priority(std::string value);
       void maintainer (std::string value);
       void standards_version(std::string value);

       std::string package_name() const  {return _package_name;}
       std::string version() const  {return _version;}
       std::string package_version() const  {return _package_version;}
       std::string section() const  {return _section;}
       std::string priority() const  {return _priority; }
       std::string maintainer() const  {return _maintainer;}
       std::string standards_version() const  {return _standards_version;}

       const std::vector<ItemToPackage> &items_to_package() const {return _items_to_package;};
       void set_item_to_package(const ItemToPackage &item);
       void remove_item_to_package(const std::string &source);

       std::string summary() const  {return _summary; }
       void summary(std::string value);
       std::string description() const { return _description; }
       void description(std::string description);

       std::string licence() const { return _licence;}
       void licence(std::string licence);

       std::string copyright() const { return _copyright;}
       void copyright(std::string value);

       std::string depends() const {return _depends;}
       void depends(std::string value);
       std::string recommends() const {return _recommends;}
       void recommends(std::string value);
       std::string suggests() const {return _suggests;}
       void suggests(std::string value);
       std::string conflicts() const {return _conflicts;}
       void conflicts(std::string value);

       bool files_in_zip(std::string &zipfile, std::string &zipitem) const;

       bool read_zip_item(const std::string &zipfile, const std::string &zipitem, std::string &data);

       void read_control(const std::string &filename);
       void read_control(std::istream &in);

       std::string standard_leafname() const;

       bool same_as(const std::string &pkgfilename, std::string *diff = nullptr) const;

    private:
       void validate_install_to(std::string where);
       void set_error(PackageItem where, std::string message);
       void clear_error(PackageItem where);

       bool standards_version_lt(std::string value);

       void check_depends(PackageItem where, std::string depends);
       std::string check_one_dependency(std::string dep) const;

       // Load package helpers
       void set_install_item(std::string &install_item, const std::string &item_name, bool &can_grow);
       void set_payload(const std::string &name);
       std::string zip_to_riscos_name(const std::string &zipname) const;
       std::string riscos_to_zip_name(const std::string &riscosname) const;

       bool read_zip_item(CZipArchive &zip, int index, std::string &data);

       void set_control_field(std::string name, std::string value);
       // Save package helpers
       void write_control(CZipArchive &zip) const;
       void write_copyright(CZipArchive &zip) const;

       // Zip file creation helpers
       void write_text_file(CZipArchive &zip, const char *filename, std::string text) const;
       void get_file_list(const tbx::Path &dirname, std::vector<std::pair<tbx::Path, tbx::PathInfo> > &file_list) const;
       void copy_files(CZipArchive &zip, const tbx::Path &dirname, const std::string &install_to) const;
       void copy_file(CZipArchive &zip, const tbx::Path &filename, const std::string &install_to) const;
       void copy_file(CZipArchive &zip, const tbx::Path &filename, tbx::PathInfo &entry, const std::string &install_to) const;

       // Package with existing package comparison helpers
       std::string control_as_text() const;
       bool compare_file_text_size(std::map<std::string, int> &zip_contents, const std::string &zip_filename, const std::string &text, std::string *diff) const;
       bool file_text_is_same(CZipArchive &zip_compare, const std::string &zip_filename, const std::string &text, std::string *diff) const;
       bool build_disc_list(std::map<std::string, std::string> &disc_file_list, std::map<std::string, int> &zip_contents, const std::string &disc_dirname, const std::string &zip_dirname, std::string *diff) const;
       bool file_is_same(CZipArchive &zip_compare, const std::string &disc_filename, const std::string &zip_filename, std::string *diff) const;

};

#endif
