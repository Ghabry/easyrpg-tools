/*
 * Copyright (c) 2016 LcfTrans authors
 * This file is released under the MIT License
 * http://opensource.org/licenses/MIT
 */

#ifndef LCFTRANS_TRANSLATION
#define LCFTRANS_TRANSLATION

#include <string>
#include <vector>

class Translation
{
public:
	class Entry {
	public:
		std::string original; // msgid
		std::string translation; // msgstr
		std::string context; // msgctxt
		std::string info; // #.

		void write(std::ostream& out) const;
	};

	Translation();

	void write(std::ostream& out);

	void writeHeader(std::ostream& out) const;

	void writeEntries(std::ostream& out);

	bool addEntry(const Entry& entry);

	static Translation* fromLDB(const std::string& filename, const std::string& encoding);
	static Translation* fromLMU(const std::string& filename, const std::string& encoding);
	static Translation* fromPO(const std::string& filename);

private:
	std::vector<Entry> entries;
};

#endif