/*
 * This file is part of EasyRPG Player.
 *
 * EasyRPG Player is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * EasyRPG Player is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with EasyRPG Player. If not, see <http://www.gnu.org/licenses/>.
 */

// Headers
#include "dynrpg.h"

#include <cstring>
#include <fstream>
#include <map>

enum DynRpg_ParseMode {
	ParseMode_Function,
	ParseMode_WaitForComma,
	ParseMode_WaitForArg,
	ParseMode_String,
	ParseMode_Token
};

typedef std::map<std::string, dynfunc> dyn_rpg_func;

namespace {
	bool init = false;

	// Registered DynRpg Plugins
	std::vector<std::unique_ptr<DynRpgPlugin>> plugins;

	// DynRpg Function table
	dyn_rpg_func dyn_rpg_functions;
}

void DynRpg::RegisterFunction(const std::string& name, dynfunc func) {
	dyn_rpg_functions[name] = func;
}

bool DynRpg::HasFunction(const std::string& name) {
	return dyn_rpg_functions.find(name) != dyn_rpg_functions.end();
}

// Var arg referenced by $n
std::string DynRpg::ParseVarArg(lcf::StringView func_name, dyn_arg_list args, int index, bool& parse_okay) {
	parse_okay = true;
	if (index >= static_cast<int>(args.size())) {
		parse_okay = false;
		return "";
	}

	std::string::iterator text_index, end;
	std::string text = args[index];
	text_index = text.begin();
	end = text.end();

	std::stringstream msg;

	for (; text_index != end; ++text_index) {
		char chr = *text_index;

		// Test for "" -> append "
		// otherwise end of string
		if (chr == '$' && std::distance(text_index, end) > 1) {
			char n = *std::next(text_index, 1);

			if (n == '$') {
				// $$ = $
				msg << n;
				++text_index;
			} else if (n >= '1' && n <= '9') {
				int i = (int)(n - '0');

				if (i + index < static_cast<int>(args.size())) {
					msg << args[i + index];
				}
				else {
					// $-ref out of range
					parse_okay = false;
					return "";
				}

				++text_index;
			} else {
				msg << chr;
			}
		} else {
			msg << chr;
		}
	}

	return msg.str();
}


static std::string ParseToken(const std::string& token, const std::string& function_name) {
	// Normal token
	return Utils::LowerCase(token);
}

void create_all_plugins() {
	for (auto& plugin : plugins) {
		plugin->RegisterFunctions();
	}

	init = true;
}

std::string DynRpg::ParseCommand(const std::string& command, std::vector<std::string>& args) {
	if (command.empty()) {
		// Not a DynRPG function (empty comment)
		return "";
	}

	std::string::iterator text_index, end;
	std::string text = command;
	text_index = text.begin();
	end = text.end();

	char chr = *text_index;

	if (chr != '@') {
		// Not a DynRPG function, normal comment
		return "";
	}

	DynRpg_ParseMode mode = ParseMode_Function;
	std::string function_name;
	std::string tmp;
	std::stringstream token;

	++text_index;

	// Parameters can be of type Token, Number or String
	// Strings are in "", a "-literal is represented by ""
	// Number is a valid float number
	// Tokens are Strings without "" and with Whitespace stripped o_O
	// If a token is (regex) N?V+[0-9]+ it is resolved to a var or an actor

	// All arguments are passed as string to the DynRpg functions and are
	// converted to int or float on demand.

	for (;;) {
		if (text_index != end) {
			chr = *text_index;
		}

		if (text_index == end) {
			switch (mode) {
				case ParseMode_Function:
					// End of function token
					function_name = Utils::LowerCase(token.str());
					if (function_name.empty()) {
						// empty function name
						return "";
					}
					break;
				case ParseMode_WaitForComma:
					// no-op
					break;
				case ParseMode_WaitForArg:
					if (!args.empty()) {
						// Found , but no token -> empty arg
						args.emplace_back("");
					}
					break;
				case ParseMode_String:
					// Unterminated literal, handled like a terminated literal
					args.emplace_back(token.str());
					break;
				case ParseMode_Token:
					tmp = ParseToken(token.str(), function_name);
					args.emplace_back(tmp);
					break;
			}

			break;
		} else if (chr == ' ') {
			switch (mode) {
				case ParseMode_Function:
					// End of function token
					function_name = Utils::LowerCase(token.str());
					if (function_name.empty()) {
						// empty function name
						return "";
					}
					token.str("");

					mode = ParseMode_WaitForArg;
					break;
				case ParseMode_WaitForComma:
				case ParseMode_WaitForArg:
					// no-op
					break;
				case ParseMode_String:
					token << chr;
					break;
				case ParseMode_Token:
					// Skip whitespace
					break;
			}
		} else if (chr == ',') {
			switch (mode) {
				case ParseMode_Function:
					// End of function token
					function_name = Utils::LowerCase(token.str());
					if (function_name.empty()) {
						// empty function name
						return "";
					}
					token.str("");
					// Empty arg
					args.emplace_back("");
					mode = ParseMode_WaitForArg;
					break;
				case ParseMode_WaitForComma:
					mode = ParseMode_WaitForArg;
					break;
				case ParseMode_WaitForArg:
					// Empty arg
					args.emplace_back("");
					break;
				case ParseMode_String:
					token << chr;
					break;
				case ParseMode_Token:
					tmp = ParseToken(token.str(), function_name);
					args.emplace_back(tmp);
					// already on a comma
					mode = ParseMode_WaitForArg;
					token.str("");
					break;
			}
		} else {
			// Anything else that isn't special purpose
			switch (mode) {
				case ParseMode_Function:
					token << chr;
					break;
				case ParseMode_WaitForComma:
					return "";
				case ParseMode_WaitForArg:
					if (chr == '"') {
						mode = ParseMode_String;
						// begin of string
					}
					else {
						mode = ParseMode_Token;
						token << chr;
					}
					break;
				case ParseMode_String:
					if (chr == '"') {
						// Test for "" -> append "
						// otherwise end of string
						if (std::distance(text_index, end) > 1 && *std::next(text_index, 1) == '"') {
							token << '"';
							++text_index;
						}
						else {
							// End of string
							args.emplace_back(token.str());

							mode = ParseMode_WaitForComma;
							token.str("");
						}
					}
					else {
						token << chr;
					}
					break;
				case ParseMode_Token:
					token << chr;
					break;
			}
		}

		++text_index;
	}

	return function_name;
}

std::vector<std::string> DynRpg::Invoke(std::string& command) {
	if (!init) {
		create_all_plugins();
	}

	std::vector<std::string> args;
	command = ParseCommand(command, args);

	return args;
}

bool DynRpg::Invoke(const std::string& func, dyn_arg_list args) {
	if (!init) {
		create_all_plugins();
	}

	if (!DynRpg::HasFunction(func)) {
		// Not a supported function
		return true;
	}

	return dyn_rpg_functions[func](args);
}

void DynRpg::Update() {
	for (auto& plugin : plugins) {
		plugin->Update();
	}
}

void DynRpg::Reset() {
	init = false;
	dyn_rpg_functions.clear();
	plugins.clear();
}
