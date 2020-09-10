/*
 * Copyright (c) 2016 LcfTrans authors
 * This file is released under the MIT License
 * http://opensource.org/licenses/MIT
 */

#include "translation.h"

#include <map>
#include <regex>
#include <sstream>
#include <fstream>
#include <lcf/context.h>
#include <lcf/data.h>
#include <lcf/rpg/eventcommand.h>
#include <lcf/ldb/reader.h>
#include <lcf/lmu/reader.h>
#include <lcf/rpg/database.h>

static std::string escape(const std::string& str) {
	static std::regex quotation(R"raw(("))raw");
	static std::regex backslash(R"raw((\\))raw");

	return std::regex_replace(std::regex_replace(str, backslash, "\\$1"), quotation, "\\$1");
}

static std::string unescape(const std::string& str) {
	static std::regex quotation(R"raw((""))raw");
	static std::regex backslash(R"raw((\\\\))raw");

	return std::regex_replace(std::regex_replace(str, backslash, "\"$1"), quotation, "\\$1");
}

static std::string formatLines(const std::vector<std::string> lines) {
	std::string ret;
	for (const std::string& s : lines) {
		ret += s + "\\n";
	}
	return ret;
}

static void getStrings(std::vector<std::string>& ret_val, const std::vector<lcf::rpg::EventCommand>& list, int index) {
#if 0
	// Let's find the choices
	int current_indent = list[index + 1].indent;
	unsigned int index_temp = index + 1;
	std::vector<std::string> s_choices;
	while (index_temp < list.size()) {
		auto code = static_cast<lcf::rpg::EventCommand::Code>(list[index_temp].code);
		if (code == lcf::rpg::EventCommand::Code::ShowChoiceOption && (list[index_temp].indent == current_indent)) {
			// Choice found
			s_choices.push_back(list[index_temp].string);
		}
		// If found end of show choice command
		if ((code == lcf::rpg::EventCommand::Code::ShowChoiceEnd && (list[index_temp].indent == current_indent)) ||
			// Or found Cancel branch
			(code == lcf::rpg::EventCommand::Code::ShowChoiceOption && (list[index_temp].indent == current_indent) &&
				(list[index_temp].string == ""))) {

			break;
		}
		// Move on to the next command
		index_temp++;
	}
	ret_val.swap(s_choices);
#endif
}

Translation::Translation() {
}

void Translation::write(std::ostream& out) {
	writeHeader(out);
	out << std::endl;

	writeEntries(out);
}

void Translation::writeHeader(std::ostream& out) const {
	out << "msgid \"\"" << std::endl;
	out << "msgstr \"\"" << std::endl;
	out << "\"Project-Id-Version: GAME_NAME 1.0\\n\"" << std::endl;
	out << "\"Language-Team: YOUR NAME <mail@your.address>\\n\"" << std::endl;
	out << "\"Language: \\n\"" << std::endl;
	out << "\"MIME-Version: 1.0\\n\"" << std::endl;
	out << "\"Content-Type: text/plain; charset=UTF-8\\n\"" << std::endl;
	out << "\"Content-Transfer-Encoding: 8bit\\n\"" << std::endl;
	out << "\"X-CreatedBy: LcfTrans\"" << std::endl;
}

void Translation::writeEntries(std::ostream& out) {
	std::map<std::string, std::vector<Entry*>> items;
	std::vector<std::string> order;
	std::vector<Entry*> item;
	std::string old_context = "\1";

	for (Entry& e : entries) {
		std::string key = e.context + "\1" + e.original;

		items[key].push_back(&e);

		if (old_context != key && items[key].size() == 1) {
			old_context = key;
			order.push_back(key);
		}
	}

	for (std::string& s : order) {
		for (Entry* e : items[s]) {
			if (!e->info.empty()) {
				std::stringstream ss(e->info);

				std::string info;
				while (std::getline(ss, info, '\n')) {
					out << "#. " << info << std::endl;
				}
			}
		}
		items[s][0]->write(out);

		out << std::endl;
	}
}

bool Translation::addEntry(const Entry& entry) {
	if (entry.original.empty()) {
		return false;
	}
	entries.push_back(entry);
	return true;
}

const std::vector<Translation::Entry>& Translation::getEntries() const {
	return entries;
}

Translation* Translation::fromLDB(const std::string& filename, const std::string& encoding) {
	Translation* t = new Translation();

	lcf::LDB_Reader::Load(filename, encoding);

	auto chunks = { "actors", "classes", "skills", "items", "enemies", "states", "terms" };

	std::vector<std::vector<std::string>> fields = {
		{ "actors", "name", "title", "skill_name" },
		{ "classes", "name" },
		{ "skills", "name", "description", "using_message1", "using_message2" },
		{ "items", "name", "description" },
		{ "enemies", "name" },
		{ "states", "name", "message_actor", "message_enemy", "message_already", "message_affected", "message_affected" }
	};

	// Process non-event strings
	lcf::rpg::ForEachString(lcf::Data::data, nullptr, [&](lcf::Context<lcf::DBString>& ctx) {
		if (!ctx.parent || ctx.parent->parent) {
			// Only care about entries one level deep
			return;
		}

		if (!ctx.value->empty()) {
			lcf::StringView pname = ctx.parent->name;
			lcf::StringView name = ctx.name;

			if (std::find(chunks.begin(), chunks.end(), pname) == chunks.end()) {
				return;
			}

			if (pname != "terms") {
				for (auto& field : fields) {
					if (ctx.parent->name == field[0]) {
						if (std::find(field.begin(), field.end(), name) == field.end()) {
							return;
						}
					}
				}
			}

			Entry e;
			e.original = lcf::ToString(*ctx.value);
			e.context = lcf::ToString(ctx.parent->name) + "." + lcf::ToString(ctx.name);
			// e.info
			t->addEntry(e);
		}
	});
#if 0
	// Process common events
	Entry e;
	lcf::rpg::ForEachString(lcf::Data::data, nullptr, [&](lcf::Context<lcf::DBString>& ctx) {
		if (!ctx.parent || ctx.parent->name != "commonevents" || ctx.name != "string") {
			// Only care about the text parameter of common events
			return;
		}

		using Cmd = lcf::rpg::EventCommand::Code;
		const auto& evt = *reinterpret_cast<lcf::rpg::EventCommand*>(ctx.obj);

		// Read common events
		bool has_message = false;
		std::vector<std::string> choices;

		switch (static_cast<lcf::rpg::EventCommand::Code>(evt.code)) {
			case Cmd::ShowMessage:
				if (has_message) {
					// New message
					if (!e.original.empty()) {
						e.original.pop_back(); // remove \n
						t->addEntry(e);
					}
				}

				e.info = "Common Event " + std::to_string(ctx.parent->index + 1) + ", Line " + std::to_string(line_count);
				has_message = true;
				e.original = evt.string + "\n";

				break;
			case lcf::rpg::EventCommand::Code::ShowMessage_2:
				if (!has_message) {
					// shouldn't happen
					e.original = "";
				}

				// Next message line
				e.original += evt.string + "\n";
				break;
			case lcf::rpg::EventCommand::Code::ShowChoice:
				if (has_message) {
					has_message = false;
					if (!e.original.empty()) {
						e.original.pop_back();
						t->addEntry(e);
						e.info = "Common Event " + std::to_string(evt_count) + ", Line " + std::to_string(line_count);
					}
				}

				e.info = "Common Event " + std::to_string(evt_count) + ", Line " + std::to_string(line_count) + " (Choice)";

				choices.clear();
				getStrings(choices, events, j);
				if (!choices.empty()) {
					e.original = choices[0] + "\n";

					for (size_t choice_i = 1; choice_i < choices.size(); ++choice_i) {
						e.original += choices[choice_i] + "\n";
					}

					if (!e.original.empty()) {
						e.original.pop_back();
						t->addEntry(e);
					}
				}

				break;
			default:
				if (has_message) {
					has_message = false;
					if (!e.original.empty()) {
						e.original.pop_back();
						t->addEntry(e);
					}
				}
				break;
		}
	}

		for (size_t i = 0; i < lcf::Data::commonevents.size(); ++i) {
			int evt_count = i + 1;

			const std::vector<lcf::rpg::EventCommand>& events = lcf::Data::commonevents[i].event_commands;
			for (size_t j = 0; j < events.size(); ++j) {
				const lcf::rpg::EventCommand& evt = events[j];
				int line_count = j + 1;

				e.context = "event";



			if (has_message) {
				// Write last event
				has_message = false;
				if (!e.original.empty()) {
					e.original.pop_back();
					t->addEntry(e);
				}
			}
		}
	}
#endif
#if 0
	Entry e;

	for (size_t i = 0; i < lcf::Data::actors.size(); ++i) {
		const lcf::rpg::Actor& actor = lcf::Data::actors[i];

		e.original = actor.name;
		e.context = "actor.name";
		e.info = "Actor " + std::to_string(i + 1) + ": Name";
		t->addEntry(e);

		e.original = actor.title;
		e.context = "actor.title";
		e.info = "Actor " + std::to_string(i + 1) + ": Title";
		t->addEntry(e);

		e.original = actor.skill_name;
		e.context = "actor.skill_name";
		e.info = "Actor " + std::to_string(i + 1) + ": Skill name";
		t->addEntry(e);
	}

	for (size_t i = 0; i < lcf::Data::classes.size(); ++i) {
		const lcf::rpg::Class& cls = lcf::Data::classes[i];

		e.original = cls.name;
		e.context = "cls.name";
		e.info = "Class " + std::to_string(i + 1) + ": Name";
		t->addEntry(e);
	}

	for (size_t i = 0; i < lcf::Data::skills.size(); ++i) {
		const lcf::rpg::Skill& skill = lcf::Data::skills[i];

		e.original = skill.name;
		e.context = "skill.name";
		e.info = "Skill " + std::to_string(i + 1) + ": Name";
		t->addEntry(e);

		e.original = skill.description;
		e.context = "skill.description";
		e.info = "Skill " + std::to_string(i + 1) + ": Description";
		t->addEntry(e);

		e.original = "%S" + skill.using_message1;
		e.context = "skill.using_message1";
		e.info = "Skill " + std::to_string(i + 1) + ": Using message 1\n%S: Source name";
		if (e.original != "%S")
			t->addEntry(e);

		e.original = "%S" + skill.using_message2;
		e.context = "skill.using_message2";
		e.info = "Skill " + std::to_string(i + 1) + ": Using message 2\n%S: Source name";
		if (e.original != "%S")
			t->addEntry(e);
	}

	for (size_t i = 0; i < lcf::Data::items.size(); ++i) {
		const lcf::rpg::Item& item = lcf::Data::items[i];

		e.original = item.name;
		e.context = "item.name";
		e.info = "Item " + std::to_string(i + 1) + ": Name";
		t->addEntry(e);

		e.original = item.description;
		e.context = "item.description";
		e.info = "Item " + std::to_string(i + 1) + ": Description";
		t->addEntry(e);
	}

	for (size_t i = 0; i < lcf::Data::enemies.size(); ++i) {
		const lcf::rpg::Enemy& enemy = lcf::Data::enemies[i];

		e.original = enemy.name;
		e.context = "enemy.name";
		e.info = "Enemy " + std::to_string(i + 1) + ": Name";
		t->addEntry(e);
	}

	for (size_t i = 0; i < lcf::Data::states.size(); ++i) {
		const lcf::rpg::State& state = lcf::Data::states[i];

		e.original = state.name;
		e.context = "state.name";
		e.info = "State " + std::to_string(i + 1) + ": Name";
		t->addEntry(e);

		e.original = "%S" + state.message_actor;
		e.context = "state.message_actor";
		e.info = "State " + std::to_string(i + 1) + ": Message actor\n%S: Target name";
		if (e.original != "%S")
			t->addEntry(e);

		e.original = "%S" + state.message_enemy;
		e.context = "state.message_enemy";
		e.info = "State " + std::to_string(i + 1) + ": Message enemy\n%S: Target name";
		if (e.original != "%S")
			t->addEntry(e);

		e.original = "%S" + state.message_already;
		e.context = "state.message_already";
		e.info = "State " + std::to_string(i + 1) + ": Message already\n%S: Target name";
		if (e.original != "%S")
			t->addEntry(e);

		e.original = "%S" + state.message_affected;
		e.context = "state.message_affected";
		e.info = "State " + std::to_string(i + 1) + ": Message affected\n%S: Target name";
		if (e.original != "%S")
			t->addEntry(e);

		e.original = "%S" + state.message_recovery;
		e.context = "state.message_recovery";
		e.info = "State " + std::to_string(i + 1) + ": Message recovery\n%S: Target name";
		if (e.original != "%S")
			t->addEntry(e);
	}

	for (size_t i = 0; i < lcf::Data::battlecommands.commands.size(); ++i) {
		const lcf::rpg::BattleCommand& bcmd = lcf::Data::battlecommands.commands[i];

		e.original = bcmd.name;
		e.context = "bcmd.name";
		e.info = "Battle command " + std::to_string(i + 1) + ": Name";
		t->addEntry(e);
	}

	// ToDo: Check for new RPG2k
	e.original = "%S" + lcf::Data::terms.encounter;
	e.context = "term";
	e.info = "Term: Battle encounter\n%S: Enemy name";
	if (e.original != "%S")
		t->addEntry(e);

	e.original = lcf::Data::terms.special_combat;
	e.context = "term";
	e.info = "Term: Battle surprise attack";
	t->addEntry(e);

	e.original = lcf::Data::terms.escape_success;
	e.context = "term";
	e.info = "Term: Battle escape success";
	t->addEntry(e);

	e.original = lcf::Data::terms.escape_failure;
	e.context = "term";
	e.info = "Term: Battle escape failed";
	t->addEntry(e);

	e.original = lcf::Data::terms.victory;
	e.context = "term";
	e.info = "Term: Battle victory";
	t->addEntry(e);

	e.original = lcf::Data::terms.defeat;
	e.context = "term";
	e.info = "Term: Battle defeat";
	t->addEntry(e);

	e.original = "%V %U" + lcf::Data::terms.exp_received;
	e.context = "term";
	e.info = "Term: Battle exp received\n%V: EXP amount\n%U: EXP term";
	if (e.original != "%V %U")
		t->addEntry(e);

	e.original = lcf::Data::terms.gold_recieved_a + "%V%U" + lcf::Data::terms.gold_recieved_b;
	e.context = "term";
	e.info = "Term: Battle gold received\n%V: Gold amount\n%U: Gold term";
	if (e.original != "%V%U")
		t->addEntry(e);

	e.original = "%S" + lcf::Data::terms.item_recieved;
	e.context = "term";
	e.info = "Term: Battle item received\n%S: Item name";
	if (e.original != "%S")
		t->addEntry(e);

	e.original = "%S" + lcf::Data::terms.attacking;
	e.context = "term";
	e.info = "Term: Battle normal attack\n%S: Source name";
	if (e.original != "%S")
		t->addEntry(e);

	e.original = "%S" + lcf::Data::terms.actor_critical;
	e.context = "term";
	e.info = "Term: Battle ally landed critical hit\n%S: Source name\n%O: Target name";
	if (e.original != "%S")
		t->addEntry(e);

	e.original = "%S" + lcf::Data::terms.enemy_critical;
	e.context = "term";
	e.info = "Term: Battle enemy landed critical hit\n%S: Source name\n%O: Target name";
	if (e.original != "%S")
		t->addEntry(e);

	e.original = "%S" + lcf::Data::terms.defending;
	e.context = "term";
	e.info = "Term: Battle defending\n%S: Source name";
	if (e.original != "%S")
		t->addEntry(e);

	e.original = "%S" + lcf::Data::terms.observing;
	e.context = "term";
	e.info = "Term: Battle observing\n%S: Source name";
	if (e.original != "%S")
		t->addEntry(e);

	e.original = "%S" + lcf::Data::terms.focus;
	e.context = "term";
	e.info = "Term: Battle focus\n%S: Source name";
	if (e.original != "%S")
		t->addEntry(e);

	e.original = "%S" + lcf::Data::terms.autodestruction;
	e.context = "term";
	e.info = "Term: Battle enemy autodestruct\n%S: Source name";
	if (e.original != "%S")
		t->addEntry(e);

	e.original = "%S" + lcf::Data::terms.enemy_escape;
	e.context = "term";
	e.info = "Term: Battle enemy escape\n%S: Source name";
	if (e.original != "%S")
		t->addEntry(e);

	e.original = "%S" + lcf::Data::terms.enemy_transform;
	e.context = "term";
	e.info = "Term: Battle enemy transform\n%S: Source name";
	if (e.original != "%S")
		t->addEntry(e);

	e.original = "%S %V" + lcf::Data::terms.enemy_damaged;
	e.context = "term";
	e.info = "Term: Battle enemy damaged\n%S: Source name\n%V: Damage amount";
	if (e.original != "%S %V")
		t->addEntry(e);

	e.original = "%S" + lcf::Data::terms.enemy_undamaged;
	e.context = "term";
	e.info = "Term: Battle enemy not damaged\n%S: Target name";
	if (e.original != "%S")
		t->addEntry(e);

	e.original = "%S %V" + lcf::Data::terms.actor_damaged;
	e.context = "term";
	e.info = "Term: Battle actor damaged\n%S: Source name\n%V: Damage amount";
	if (e.original != "%S %V")
		t->addEntry(e);

	e.original = "%S" + lcf::Data::terms.actor_undamaged;
	e.context = "term";
	e.info = "Term: Battle actor not damaged\n%S: Target name";
	if (e.original != "%S")
		t->addEntry(e);

	e.original = "%O" + lcf::Data::terms.skill_failure_a;
	e.context = "term";
	e.info = "Term: Battle skill failure a\n%S: Source name\n%O: Target name";
	if (e.original != "%O")
		t->addEntry(e);

	e.original = "%O" + lcf::Data::terms.skill_failure_b;
	e.context = "term";
	e.info = "Term: Battle skill failure b\n%S: Source name\n%O: Target name";
	if (e.original != "%O")
		t->addEntry(e);

	e.original = "%O" + lcf::Data::terms.skill_failure_c;
	e.context = "term";
	e.info = "Term: Battle skill failure c\n%S: Source name\n%O: Target name";
	if (e.original != "%O")
		t->addEntry(e);

	e.original = "%O" + lcf::Data::terms.dodge;
	e.context = "term";
	e.info = "Term: Battle dodge\n%S: Source name\n%O: Target name";
	if (e.original != "%O")
		t->addEntry(e);

	e.original = "%S %O" + lcf::Data::terms.use_item;
	e.context = "term";
	e.info = "Term: Battle use item\n%S: Source name\n%O: Item name";
	if (e.original != "%S %O")
		t->addEntry(e);

	e.original = "%S" + lcf::Data::terms.hp_recovery ;
	e.context = "term";
	e.info = "Term: Battle hp recovery\n%S: Source name\n%V: HP amount\n%U: HP term";
	if (e.original != "%S")
		t->addEntry(e);

	e.original = "%S %V %U" + lcf::Data::terms.parameter_increase;
	e.context = "term";
	e.info = "Term: Battle parameter increase\n%S: Source name\n%V: Parameter amount\n%U: Parameter term";
	if (e.original != "%S %V %U")
		t->addEntry(e);

	e.original = "%S %V %U" + lcf::Data::terms.parameter_decrease;
	e.context = "term";
	e.info = "Term: Battle parameter decrease\n%S: Source name\n%V: Parameter amount\n%U: Parameter term";
	if (e.original != "%S %V %U")
		t->addEntry(e);

	e.original = "%S %V %U" + lcf::Data::terms.actor_hp_absorbed;
	e.context = "term";
	e.info = "Term: Battle actor hp absorbed\n%S: Source name\n%O: Target name\n%V: Parameter amount\n%U: Parameter term";
	if (e.original != "%S %V %U")
		t->addEntry(e);

	e.original = "%S %V %U" + lcf::Data::terms.enemy_hp_absorbed;
	e.context = "term";
	e.info = "Term: Battle enemy hp absorbed\n%S: Source name\n%O: Target name\n%V: Parameter amount\n%U: Parameter term";
	if (e.original != "%S %V %U")
		t->addEntry(e);

	e.original = "%S %O" + lcf::Data::terms.resistance_increase;
	e.context = "term";
	e.info = "Term: Battle resistance increase\n%S: Source name\n%O: Parameter term";
	if (e.original != "%S %O")
		t->addEntry(e);

	e.original = "%S %O" + lcf::Data::terms.resistance_decrease;
	e.context = "term";
	e.info = "Term: Battle resistance decrease\n%S: Source name\n%O: Parameter term";
	if (e.original != "%S %O")
		t->addEntry(e);

	e.original = "%S %V %U" + lcf::Data::terms.level_up;
	e.context = "term";
	e.info = "Term: Level up\n%S: Source name\n%U: Level\n%V: Level term";
	if (e.original != "%S %V %U")
		t->addEntry(e);

	e.original = "%S" + lcf::Data::terms.skill_learned;
	e.context = "term";
	e.info = "Term: Skill learned\n%S: Source name\n%O: Skill name";
	if (e.original != "%S")
		t->addEntry(e);

	e.original = lcf::Data::terms.battle_start;
	e.context = "term";
	e.info = "Term: Battle start";
	t->addEntry(e);

	e.original = lcf::Data::terms.miss;
	e.context = "term";
	e.info = "Term: Miss (RPG2k3)";
	t->addEntry(e);

	e.original = lcf::Data::terms.shop_greeting1;
	e.context = "term";
	e.info = "Term: Shop greeting 1";
	t->addEntry(e);

	e.original = lcf::Data::terms.shop_regreeting1;
	e.context = "term";
	e.info = "Term: Shop regreeting 1";
	t->addEntry(e);

	e.original = lcf::Data::terms.shop_buy1;
	e.context = "term";
	e.info = "Term: Shop buy 1";
	t->addEntry(e);

	e.original = lcf::Data::terms.shop_sell1;
	e.context = "term";
	e.info = "Term: Shop sell 1";
	t->addEntry(e);

	e.original = lcf::Data::terms.shop_leave1;
	e.context = "term";
	e.info = "Term: Shop leave 1";
	t->addEntry(e);

	e.original = lcf::Data::terms.shop_buy_select1;
	e.context = "term";
	e.info = "Term: Shop buy select 1";
	t->addEntry(e);

	e.original = lcf::Data::terms.shop_buy_number1;
	e.context = "term";
	e.info = "Term: Shop buy amount 1";
	t->addEntry(e);

	e.original = lcf::Data::terms.shop_purchased1;
	e.context = "term";
	e.info = "Term: Shop purchased 1";
	t->addEntry(e);

	e.original = lcf::Data::terms.shop_sell_select1;
	e.context = "term";
	e.info = "Term: Shop sell select 1";
	t->addEntry(e);

	e.original = lcf::Data::terms.shop_sell_number1;
	e.context = "term";
	e.info = "Term: Shop sell amount 1";
	t->addEntry(e);

	e.original = lcf::Data::terms.shop_sold1;
	e.context = "term";
	e.info = "Term: Shop sold 1";
	t->addEntry(e);

	e.original = lcf::Data::terms.shop_greeting2;
	e.context = "term";
	e.info = "Term: Shop greeting 2";
	t->addEntry(e);

	e.original = lcf::Data::terms.shop_regreeting2;
	e.context = "term";
	e.info = "Term: Shop regreeting 2";
	t->addEntry(e);

	e.original = lcf::Data::terms.shop_buy2;
	e.context = "term";
	e.info = "Term: Shop buy 2";
	t->addEntry(e);

	e.original = lcf::Data::terms.shop_sell2;
	e.context = "term";
	e.info = "Term: Shop sell 2";
	t->addEntry(e);

	e.original = lcf::Data::terms.shop_leave2;
	e.context = "term";
	e.info = "Term: Shop leave 2";
	t->addEntry(e);

	e.original = lcf::Data::terms.shop_buy_select2;
	e.context = "term";
	e.info = "Term: Shop buy select2";
	t->addEntry(e);

	e.original = lcf::Data::terms.shop_buy_number2;
	e.context = "term";
	e.info = "Term: Shop buy amount 2";
	t->addEntry(e);

	e.original = lcf::Data::terms.shop_purchased2;
	e.context = "term";
	e.info = "Term: Shop purchased 2";
	t->addEntry(e);

	e.original = lcf::Data::terms.shop_sell_select2;
	e.context = "term";
	e.info = "Term: Shop sell select 2";
	t->addEntry(e);

	e.original = lcf::Data::terms.shop_sell_number2;
	e.context = "term";
	e.info = "Term: Shop sell amount 2";
	t->addEntry(e);

	e.original = lcf::Data::terms.shop_sold2;
	e.context = "term";
	e.info = "Term: Shop sold 2";
	t->addEntry(e);

	e.original = lcf::Data::terms.shop_greeting3;
	e.context = "term";
	e.info = "Term: Shop greeting 3";
	t->addEntry(e);

	e.original = lcf::Data::terms.shop_regreeting3;
	e.context = "term";
	e.info = "Term: Shop regreeting 3";
	t->addEntry(e);

	e.original = lcf::Data::terms.shop_buy3;
	e.context = "term";
	e.info = "Term: Shop buy 3";
	t->addEntry(e);

	e.original = lcf::Data::terms.shop_sell3;
	e.context = "term";
	e.info = "Term: shop sell 3";
	t->addEntry(e);

	e.original = lcf::Data::terms.shop_leave3;
	e.context = "term";
	e.info = "Term: Shop leave 3";
	t->addEntry(e);

	e.original = lcf::Data::terms.shop_buy_select3;
	e.context = "term";
	e.info = "Term: Shop buy select 3";
	t->addEntry(e);

	e.original = lcf::Data::terms.shop_buy_number3;
	e.context = "term";
	e.info = "Term: Shop buy amount 3";
	t->addEntry(e);

	e.original = lcf::Data::terms.shop_purchased3;
	e.context = "term";
	e.info = "Term: Shop purchased 3";
	t->addEntry(e);

	e.original = lcf::Data::terms.shop_sell_select3;
	e.context = "term";
	e.info = "Term: Shop sell select 3";
	t->addEntry(e);

	e.original = lcf::Data::terms.shop_sell_number3;
	e.context = "term";
	e.info = "Term: Shop sell amount 3";
	t->addEntry(e);

	e.original = lcf::Data::terms.shop_sold3;
	e.context = "term";
	e.info = "Term: Shop sold 3";
	t->addEntry(e);

	// greeting 1 and 2 combined because it's one sentence
	e.original = lcf::Data::terms.inn_a_greeting_1 + "%V%U" + lcf::Data::terms.inn_a_greeting_2;
	e.context = "term";
	e.info = "Term: Inn A greeting 1\n%V: Gold amount\n%U: Gold term";
	if (e.original != "%V%U")
		t->addEntry(e);

	e.original = lcf::Data::terms.inn_a_greeting_3;
	e.context = "term";
	e.info = "Term: Inn A greeting 2";
	t->addEntry(e);

	e.original = lcf::Data::terms.inn_a_accept;
	e.context = "term";
	e.info = "Term: Inn A accept";
	t->addEntry(e);

	e.original = lcf::Data::terms.inn_a_cancel;
	e.context = "term";
	e.info = "Term: Inn A cancel";
	t->addEntry(e);

	// greeting 1 and 2 combined because it's one sentence
	e.original = lcf::Data::terms.inn_b_greeting_1 + "%V%U" + lcf::Data::terms.inn_b_greeting_2;
	e.context = "term";
	e.info = "Term: Inn B greeting 1\n%V: Gold amount\n%U: Gold term";
	if (e.original != "%V%U")
		t->addEntry(e);

	e.original = lcf::Data::terms.inn_b_greeting_3;
	e.context = "term";
	e.info = "Term: Inn B greeting 2";
	t->addEntry(e);

	e.original = lcf::Data::terms.inn_b_accept;
	e.context = "term";
	e.info = "Term: Inn B accept";
	t->addEntry(e);

	e.original = lcf::Data::terms.inn_b_cancel;
	e.context = "term";
	e.info = "Term: Inn B cancel";
	t->addEntry(e);

	e.original = lcf::Data::terms.possessed_items;
	e.context = "term";
	e.info = "Term: Shop owned items";
	t->addEntry(e);

	e.original = lcf::Data::terms.equipped_items;
	e.context = "term";
	e.info = "Term: Shop equipped items";
	t->addEntry(e);

	e.original = lcf::Data::terms.gold;
	e.context = "term";
	e.info = "Term: gold";
	t->addEntry(e);

	e.original = lcf::Data::terms.battle_fight;
	e.context = "term";
	e.info = "Term: Battle fight";
	t->addEntry(e);

	e.original = lcf::Data::terms.battle_auto;
	e.context = "term";
	e.info = "Term: Battle auto";
	t->addEntry(e);

	e.original = lcf::Data::terms.battle_escape;
	e.context = "term";
	e.info = "Term: Battle escape";
	t->addEntry(e);

	e.original = lcf::Data::terms.command_attack;
	e.context = "term";
	e.info = "Term: Battle command attack";
	t->addEntry(e);

	e.original = lcf::Data::terms.command_defend;
	e.context = "term";
	e.info = "Term: Battle command defend";
	t->addEntry(e);

	e.original = lcf::Data::terms.command_item;
	e.context = "term";
	e.info = "Term: Battle command item";
	t->addEntry(e);

	e.original = lcf::Data::terms.command_skill;
	e.context = "term";
	e.info = "Term: Battle command skill";
	t->addEntry(e);

	e.original = lcf::Data::terms.menu_equipment;
	e.context = "term";
	e.info = "Term: Menu equipment";
	t->addEntry(e);

	e.original = lcf::Data::terms.menu_save;
	e.context = "term";
	e.info = "Term: Menu save";
	t->addEntry(e);

	e.original = lcf::Data::terms.menu_quit;
	e.context = "term";
	e.info = "Term: Menu quit";
	t->addEntry(e);

	e.original = lcf::Data::terms.new_game;
	e.context = "term";
	e.info = "Term: New game";
	t->addEntry(e);

	e.original = lcf::Data::terms.load_game;
	e.context = "term";
	e.info = "Term: Load game";
	t->addEntry(e);

	e.original = lcf::Data::terms.exit_game;
	e.context = "term";
	e.info = "Term: Exit game";
	t->addEntry(e);

	e.original = lcf::Data::terms.status;
	e.context = "term";
	e.info = "Term: Status";
	t->addEntry(e);

	e.original = lcf::Data::terms.row;
	e.context = "term";
	e.info = "Term: Row";
	t->addEntry(e);

	e.original = lcf::Data::terms.order;
	e.context = "term";
	e.info = "Term: Order";
	t->addEntry(e);

	e.original = lcf::Data::terms.wait_on;
	e.context = "term";
	e.info = "Term: ATB wait on";
	t->addEntry(e);

	e.original = lcf::Data::terms.wait_off;
	e.context = "term";
	e.info = "Term: ATB wait off";
	t->addEntry(e);

	e.original = lcf::Data::terms.level;
	e.context = "term";
	e.info = "Term: Level";
	t->addEntry(e);

	e.original = lcf::Data::terms.health_points;
	e.context = "term";
	e.info = "Term: Health points";
	t->addEntry(e);

	e.original = lcf::Data::terms.spirit_points;
	e.context = "term";
	e.info = "Term: Spirit points";
	t->addEntry(e);

	e.original = lcf::Data::terms.normal_status;
	e.context = "term";
	e.info = "Term: Normal status";
	t->addEntry(e);

	e.original = lcf::Data::terms.exp_short;
	e.context = "term";
	e.info = "Term: Exp short";
	t->addEntry(e);

	e.original = lcf::Data::terms.lvl_short;
	e.context = "term";
	e.info = "Term: Level short";
	t->addEntry(e);

	e.original = lcf::Data::terms.hp_short;
	e.context = "term";
	e.info = "Term: HP short";
	t->addEntry(e);

	e.original = lcf::Data::terms.sp_short;
	e.context = "term";
	e.info = "Term: SP short";
	t->addEntry(e);

	e.original = lcf::Data::terms.sp_cost;
	e.context = "term";
	e.info = "Term: SP cost";
	t->addEntry(e);

	e.original = lcf::Data::terms.attack;
	e.context = "term";
	e.info = "Term: Attack stat";
	t->addEntry(e);

	e.original = lcf::Data::terms.defense;
	e.context = "term";
	e.info = "Term: Defense stat";
	t->addEntry(e);

	e.original = lcf::Data::terms.spirit;
	e.context = "term";
	e.info = "Term: Spirit stat";
	t->addEntry(e);

	e.original = lcf::Data::terms.agility;
	e.context = "term";
	e.info = "Term: Agility stat";
	t->addEntry(e);

	e.original = lcf::Data::terms.weapon;
	e.context = "term";
	e.info = "Term: Weapon equipment";
	t->addEntry(e);

	e.original = lcf::Data::terms.shield;
	e.context = "term";
	e.info = "Term: Shield equipment";
	t->addEntry(e);

	e.original = lcf::Data::terms.armor;
	e.context = "term";
	e.info = "Term: Armor equipment";
	t->addEntry(e);

	e.original = lcf::Data::terms.helmet;
	e.context = "term";
	e.info = "Term: Helmet equipment";
	t->addEntry(e);

	e.original = lcf::Data::terms.accessory;
	e.context = "term";
	e.info = "Term: Accessory equipment";
	t->addEntry(e);

	e.original = lcf::Data::terms.save_game_message;
	e.context = "term";
	e.info = "Term: Save game message";
	t->addEntry(e);

	e.original = lcf::Data::terms.load_game_message;
	e.context = "term";
	e.info = "Term: Load game message";
	t->addEntry(e);

	e.original = lcf::Data::terms.file;
	e.context = "term";
	e.info = "Term: File";
	t->addEntry(e);

	e.original = lcf::Data::terms.exit_game_message;
	e.context = "term";
	e.info = "Term: Exit game message";
	t->addEntry(e);

	e.original = lcf::Data::terms.yes;
	e.context = "term";
	e.info = "Term: Yes";
	t->addEntry(e);

	e.original = lcf::Data::terms.no;
	e.context = "term";
	e.info = "Term: No";
	t->addEntry(e);

	// Read common events
	bool has_message = false;
	std::vector<std::string> choices;

	for (size_t i = 0; i < lcf::Data::commonevents.size(); ++i) {
		int evt_count = i + 1;

		const std::vector<lcf::rpg::EventCommand>& events = lcf::Data::commonevents[i].event_commands;
		for (size_t j = 0; j < events.size(); ++j) {
			const lcf::rpg::EventCommand& evt = events[j];
			int line_count = j + 1;

			e.context = "event";

			switch (static_cast<lcf::rpg::EventCommand::Code>(evt.code)) {
			case lcf::rpg::EventCommand::Code::ShowMessage:
				if (has_message) {
					// New message
					if (!e.original.empty()) {
						e.original.pop_back();
						t->addEntry(e);
					}
				}

				e.info = "Common Event " + std::to_string(evt_count) + ", Line " + std::to_string(line_count);
				has_message = true;
				e.original = evt.string + "\n";

				break;
			case lcf::rpg::EventCommand::Code::ShowMessage_2:
				if (!has_message) {
					// shouldn't happen
					e.original = "";
				}

				// Next message line
				e.original += evt.string + "\n";
				break;
			case lcf::rpg::EventCommand::Code::ShowChoice:
				if (has_message) {
					has_message = false;
					if (!e.original.empty()) {
						e.original.pop_back();
						t->addEntry(e);
						e.info = "Common Event " + std::to_string(evt_count) + ", Line " + std::to_string(line_count);
					}
				}

				e.info = "Common Event " + std::to_string(evt_count) + ", Line " + std::to_string(line_count) + " (Choice)";

				choices.clear();
				getStrings(choices, events, j);
				if (!choices.empty()) {
					e.original = choices[0] + "\n";

					for (size_t choice_i = 1; choice_i < choices.size(); ++choice_i) {
						e.original += choices[choice_i] + "\n";
					}

					if (!e.original.empty()) {
						e.original.pop_back();
						t->addEntry(e);
					}
				}

				break;
			default:
				if (has_message) {
					has_message = false;
					if (!e.original.empty()) {
						e.original.pop_back();
						t->addEntry(e);
					}
				}
				break;
			}
		}

		if (has_message) {
			// Write last event
			has_message = false;
			if (!e.original.empty()) {
				e.original.pop_back();
				t->addEntry(e);
			}
		}
	}
#endif
	return t;
}

Translation* Translation::fromLMU(const std::string& filename, const std::string& encoding) {
	lcf::rpg::Map map = *lcf::LMU_Reader::Load(filename, encoding);

	bool has_message = false;
	std::vector<std::string> choices;

	Translation* t = new Translation();
#if 0

	for (size_t i = 0; i < map.events.size(); ++i) {
		const lcf::rpg::Event rpg_evt = map.events[i];

		for (size_t j = 0; j < rpg_evt.pages.size(); ++j) {
			int page_count = j + 1;
			const lcf::rpg::EventPage& page = rpg_evt.pages[j];
			Entry e;

			for (size_t k = 0; k < page.event_commands.size(); ++k) {
				const lcf::rpg::EventCommand& evt = page.event_commands[k];
				int line_count = k + 1;

				switch (static_cast<lcf::rpg::EventCommand::Code>(evt.code)) {
				case lcf::rpg::EventCommand::Code::ShowMessage:
					if (has_message) {
						// New message
						if (!e.original.empty()) {
							e.original.pop_back();
							t->addEntry(e);
						}
					}

					e.info = "Event " + std::to_string(rpg_evt.ID) + ", Page " + std::to_string(page_count) + ", Line " + std::to_string(line_count);
					has_message = true;
					e.original = evt.string + "\n";

					break;
				case lcf::rpg::EventCommand::Code::ShowMessage_2:
					if (!has_message) {
						// shouldn't happen
						e.original = "";
					}

					// Next message line
					e.original += evt.string + "\n";
					break;
				case lcf::rpg::EventCommand::Code::ShowChoice:
					if (has_message) {
						has_message = false;
						if (!e.original.empty()) {
							e.original.pop_back();
							t->addEntry(e);
						}
					}

					e.info = "Event " + std::to_string(rpg_evt.ID) + ", Page " + std::to_string(page_count) + ", Line " + std::to_string(line_count) + " (Choice)";

					choices.clear();
					getStrings(choices, page.event_commands, j);
					if (!choices.empty()) {
						e.original = choices[0] + "\n";

						for (size_t choice_i = 1; choice_i < choices.size(); ++choice_i) {
							e.original += choices[choice_i] + "\n";
						}

						if (!e.original.empty()) {
							e.original.pop_back();
							t->addEntry(e);
						}
					}

					break;
				default:
					if (has_message) {
						has_message = false;
						if (!e.original.empty()) {
							e.original.pop_back();
							t->addEntry(e);
						}
					}
					break;
				}
			}
		
			if (has_message) {
				// Write last event
				has_message = false;
				if (!e.original.empty()) {
					e.original.pop_back();
					t->addEntry(e);
				}
			}
		}
	}
#endif
	return t;
}

static bool starts_with(const std::string& str, const std::string& search) {
	return str.find(search) == 0;
}

static std::string extract_string(const std::string& str) {
	// Extraction of a sub-match
	std::regex base_regex(R"raw(^.*?"(.*)\n?")raw");
	std::smatch base_match;

	if (std::regex_match(str, base_match, base_regex)) {
		if (base_match.size() == 2) {
			std::ssub_match base_sub_match = base_match[1];
			std::string base = base_sub_match.str();
			return unescape(base);
		}
	}

	return "";
}

Translation* Translation::fromPO(const std::string& filename) {
	// Super simple parser.
	// Ignores header and comments but good enough for use in liblcf later...
	
	Translation* t = new Translation();

	std::ifstream in(filename);

	std::string line;
	bool found_header = false;
	bool parse_item = false;

	Entry e;

	while (std::getline(in, line, '\n')) {
		if (!found_header) {
			if (starts_with(line, "msgstr")) {
				found_header = true;
			}
			continue;
		}

		if (!parse_item) {
			if (starts_with(line, "msgctxt")) {
				e.context = extract_string(line);

				parse_item = true;
			} else if (starts_with(line, "msgid")) {
				parse_item = true;

				goto read_msgid;
			}
		} else {
			if (starts_with(line, "msgid")) {
			read_msgid:;
				// Parse multiply lines until empty line or msgstr is encountered
				e.original = extract_string(line);

				while (std::getline(in, line, '\n')) {
					if (line.empty() || starts_with(line, "msgstr")) {
						goto read_msgstr;
					}
					e.original += "\n" + extract_string(line);
				}
			} else if (starts_with(line, "msgstr")) {
			read_msgstr:;
				// Parse multiply lines until empty line or comment
				e.translation = extract_string(line);

				while (std::getline(in, line, '\n')) {
					if (line.empty() || starts_with(line, "#")) {
						break;
					}
					e.translation += "\n" + extract_string(line);
				}

				parse_item = false;
				t->addEntry(e);
			}
		}
	}

	return t;
}

static void write_n(std::ostream& out, const std::string& line, const std::string& prefix) {
	if (line.find("\n") != std::string::npos) {
		std::stringstream ss(escape(line));
		out << prefix << " \"\"" << std::endl;

		std::string item;
		bool write_n = false;
		while (std::getline(ss, item, '\n')) {
			if (write_n) {
				out << "\\n\"" << std::endl;
			}

			out << "\"" << item;

			write_n = true;
		}
		out << "\"" << std::endl;
	} else {
		out << prefix << " \"" << escape(line) << "\"" << std::endl;
	}
}

void Translation::Entry::write(std::ostream& out) const {
	if (!context.empty()) {
		out << "msgctxt \"" << context << "\"" << std::endl;
	}

	write_n(out, original, "msgid");
	write_n(out, translation, "msgstr");
}
