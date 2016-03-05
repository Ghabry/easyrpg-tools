/*
 * Copyright (c) 2016 LcfTrans authors
 * This file is released under the MIT License
 * http://opensource.org/licenses/MIT
 */

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

#include "translation.h"

#ifdef _WIN32
#include "dirent_win.h"
#else
#include <dirent.h>
#endif

#define DATABASE_FILE "rpg_rt.ldb"

void DumpLdb(const std::string& filename);
void DumpLmu(const std::string& filename);

bool endsWith(std::string const & val, std::string const & ending);
std::string toLower(std::string const & instr);

int main(int argc, char** argv) {
	std::string infile;

	DIR *dirHandle;
	struct dirent * dirEntry;

	dirHandle = opendir(".");
	if (dirHandle) {
		while (0 != (dirEntry = readdir(dirHandle))) {
			std::string name = dirEntry->d_name;
			std::string lname = toLower(name);

			if (lname == DATABASE_FILE) {
				std::cout << "Parsing Database " << name << std::endl;
				DumpLdb(name);
			}
			else if (endsWith(lname, ".lmu")) {
				std::cout << "Parsing Map " << name << std::endl;
				DumpLmu(name);
			}
		}
		closedir(dirHandle);
	}
}

// https://stackoverflow.com/questions/874134/
bool endsWith(std::string const & val, std::string const & ending)
{
	if (ending.size() > val.size()) return false;
	return std::equal(ending.rbegin(), ending.rend(), val.rbegin());
}

std::string toLower(std::string const & in) {
	std::string data = in;

	std::transform(data.begin(), data.end(), data.begin(), static_cast<int(*)(int)>(std::tolower));

	return data;
}

// https://stackoverflow.com/questions/2896600/
std::string ReplaceAll(std::string str, const std::string& from, const std::string& to) {
	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
		str.replace(start_pos, from.length(), to);
		start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
	}
	return str;
}

/** Returns the filename (without extension). */
std::string GetFilename(const std::string& str)
{
	std::string s = str;
#ifdef _WIN32
	std::replace(s.begin(), s.end(), '\\', '/');
#endif
	// Extension
	size_t found = s.find_last_of(".");
	if (found != std::string::npos)
	{
		s = s.substr(0, found);
	}

	// Filename
	found = s.find_last_of("/");
	if (found == std::string::npos)	{
		return s;
	}

	s = s.substr(found + 1);
	return s;
}

void DumpLdb(const std::string& filename) {
	Translation* t = Translation::fromLDB(filename, "1252");

	std::ofstream outfile(GetFilename(filename) + ".po");

	t->write(outfile);
}

void DumpLmu(const std::string& filename) {
	Translation* t = Translation::fromLMU(filename, "1252");

	std::ofstream outfile(GetFilename(filename) + ".po");

	t->write(outfile);
}
