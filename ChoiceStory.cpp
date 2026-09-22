#include "ChoiceStory.hpp"

// Get valid choices from current passage and state
std::vector<std::string> ChoiceStory::get_choices()
{
	std::vector<std::string> res = {};
	for (const auto &[choice, result] : current_passage->choices)
	{
		Passage *resultPassage = passages[result];

		// Return all choices where the condition is met
		if (resultPassage->condition(*state))
			res.emplace_back(choice);
	}

	return res;
}

// Make a choice -- should return resulting current passage
ChoiceStory::Passage *ChoiceStory::make_choice(std::string choice)
{
	// Update current passage
	previous_passage_id = current_passage_id;
	current_passage_id = current_passage->choices[choice];

	current_passage = passages[current_passage_id];

	// Update temperature
	state->temperature += current_passage->ambient_temperature + get_temperature_resistance();

	// Update clothes, deleting anything that has worn out
	for (auto it = state->clothes.begin(); it != state->clothes.end();)
	{
		Clothes *c = *it;

		if (c->durability > 0)
			c->durability--;

		if (c->name == "White Scarf" || c->name == "Red Scarf")
			state->scarf_durability = c->durability;

		if (c->durability == 0)
		{
			delete c;
			it = state->clothes.erase(it);
		}
		else
		{
			++it;
		}
	}

	// Run passage-specific update
	current_passage->update(*state);

	// Explicit endings take priority over temperature endings
	if (current_passage->ending != Ending::NONE)
	{
		return die(current_passage->ending);
	}

	// Automatic temperature endings
	if (state->temperature > 120)
	{
		current_passage_id = "ending_burnt";
		current_passage = passages[current_passage_id];
		return die(Ending::Burnt);
	}
	else if (state->temperature <= 0)
	{
		if (current_passage_id == "mountain_stay")
		{
			// Good ending, requires ending with more humanity than the starting value of 100.
			if (state->humanity > 100)
			{
				current_passage_id = "ending_mountain_heaven";
				current_passage = passages[current_passage_id];
				return die(Ending::MountainHeaven);
			}
			else
			{
				current_passage_id = "ending_mountain";
				current_passage = passages[current_passage_id];
				return die(Ending::Mountain);
			}
		}
		else
		{
			current_passage_id = "ending_frozen";
			current_passage = passages[current_passage_id];
			return die(Ending::Frozen);
		}
	}

	// Count the passage if the player survived it
	state->passages_survived++;

	return current_passage;
}

// Get temperature resistance based on clothes
int ChoiceStory::get_temperature_resistance()
{
	int tr = 0;
	for (const Clothes *c : state->clothes)
	{
		tr += c->resistance;
	}

	return tr;
}

// Die
ChoiceStory::Passage *ChoiceStory::die(Ending ending)
{
	state->ending = ending;

	current_passage->dialogue.emplace_back(
			"YOU DIED. Survived " +
			std::to_string(state->passages_survived) +
			" passages");

	return current_passage;
}

ChoiceStory::Passage *ChoiceStory::reset()
{
	// Clean up state
	if (state)
	{
		for (Clothes *c : state->clothes)
			delete c;

		delete state;
		state = nullptr;
	}

	// Clean up passages so they get re-initialized
	for (auto &[id, passage] : passages)
		delete passage;

	passages.clear();

	current_passage = nullptr;
	current_passage_id.clear();
	previous_passage_id.clear();

	return init();
}

// Init function for this game -- should return first passage
ChoiceStory::Passage *ChoiceStory::init()
{
	// Initial state
	state = new State{
			100, 100, {new Clothes{"White Scarf", 5, 16}}, Ending::NONE, {}};

	passages["house"] = new Passage{
			.ambient_temperature = -6,
			.dialogue = {
					"...",
					"...It's cold.",
					"The windows in my house were broken by the wind.",
					"My scarf has kept me warm so far, but its not enough anymore.",
					"I need to get moving and find some way to warm up."},
			.choices = {{"Stay", "house"}, {"Go outside", "house_outside"}},
	};

	passages["house_outside"] = new Passage{
			.ambient_temperature = -7,
			.dialogue = {
					"It's frigid out here.",
					"I don't think I'll last long out here without some warm clothes.",
					"Where should I go?"},
			.choices = {{"Head to town", "neighbor"}, {"Up the mountain", "mountain_foot"}, {"Down to the river", "river"}},
	};

	/* NEIGHBOR */
	passages["neighbor"] = new Passage{
			.ambient_temperature = -6,
			.dialogue = {
					"My neighbor's house is on the way to town.",
					"His house looks warmer than mine."},
			.choices = {{"Knock on his door", "neighbor_door"}, {"Go inside", "neighbor_house"}, {"Go inside", "neighbor_loot"}, {"Keep going to town", "town"}, {"Go back home", "house_outside"}},
	};

	passages["neighbor_door"] = new Passage{
			.ambient_temperature = -6,
			.dialogue = {
					"*knock knock knock*",
					"...",
					"No response.",
					"I tried the knob, but it wouldn't budge. Must be locked.",
					"The windows are boarded up. I can't see inside."},
			.choices = {{"Kick the door", "neighbor_house"}, {"Keep going to town", "town"}, {"Go back home", "house_outside"}},
			.condition = [](State &state)
			{
				return !state.flags["neighbor_house_burned"] && !state.flags["neighbor_door_open"];
			}};

	passages["neighbor_house"] = new Passage{
			.ambient_temperature = -4,
			.dialogue = {"I'm inside.",
									 "Mr. Cooper: Hello? Is that Nathan?",
									 "Oh... I'm sorry about your door Mr. Cooper. I didn't think anyone was home.",
									 "Mr. Cooper: Thank heavens you're here. The door has been frozen shut for days.",
									 "You've been stuck in here? What about your children?",
									 "Mr. Cooper: They haven't come back. They said they were going for more firewood but...",
									 "Mr. Cooper: I hope something terrible hasn't happened to them.",
									 "(Poor old man. Either his children abandoned him here all alone or...)",
									 "Mr. Cooper: I'm so sorry, but I don't have anything to give you.",
									 "Mr. Cooper: I've been out of firewood since they left. I'm so cold."},
			.choices = {{"Humans are warm blooded...", "neighbor_murder"}, {"You can have my scarf.", "neighbor_scarf"}, {"I'll be on my way.", "neighbor"}},
			.condition = [&](State &state)
			{
				// Only available if opening door or door already open, and neighbor is alive
				return current_passage_id == "neighbor_door" || (state.flags["neighbor_door_open"] && !state.flags["neighbor_dead"]); },
			.update = [&](State &state)
			{
				// Decrease humanity if kicking in door
				if (previous_passage_id == "neighbor_door") state.humanity -= 10;
				state.flags["neighbor_door_open"] = true; }};

	passages["neighbor_murder"] = new Passage{
			.ambient_temperature = -4,
			.dialogue = {"I'm sorry Mr. Cooper. I'm cold too.",
									 "Mr. Cooper: What...? What are you doing...?",
									 "Mr. Cooper: Please... Don't come any closer...",
									 "...",
									 "...",
									 "Yeah...",
									 "Humans are warm blooded after all."},
			.choices = {{"Leave", "neighbor"}, {"Loot his house", "neighbor_loot"}},
			.condition = [](State &state)
			{ return state.flags["has_axe"] && !state.flags["neighbor_dead"]; },
			.update = [&](State &state)
			{
				state.humanity -= 50;
				state.flags["neighbor_dead"] = true;

				if (state.flags["neighbor_has_scarf"])
				{
					state.clothes.emplace_back(new Clothes{"Red Scarf", 5, state.scarf_durability});
				}
				state.clothes.emplace_back(new Clothes{"Blood", 8, 4}); }};

	passages["neighbor_loot"] = new Passage{
			.ambient_temperature = -4,
			.dialogue = {"I look around the old house.",
									 "I can't find anything to keep me warm.",
									 "No firewood here except for the old wooden walls."},
			.choices = {{"Leave", "neighbor"}, {"Light the walls", "neighbor_house_burn"}},
			.condition = [](State &state)
			{ return state.flags["neighbor_dead"]; },
			.update = [&](State &state)
			{
				state.humanity -= 10;
				state.flags["has_wallet"] = true; }};

	passages["neighbor_scarf"] = new Passage{
			.ambient_temperature = -4,
			.dialogue = {"Here, you can have my scarf. Stay warm.",
									 "Mr. Cooper: Thank you so much. If my children...",
									 "Mr. Cooper: When my children return with firewood, I'll give you some.",
									 "Don't worry about it."},
			.choices = {{"Humans are warm blooded...", "neighbor_murder"}, {"I'll be on my way.", "neighbor"}},
			.condition = [](State &state)
			{ return !state.flags["neighbor_dead"] && state.has_clothes("White Scarf"); },
			.update = [&](State &state)
			{
				state.humanity += 20;
				state.flags["neighbor_has_scarf"] = true;
				state.remove_clothes("White Scarf"); }};

	passages["neighbor_house_burn"] = new Passage{
			.ambient_temperature = 10,
			.dialogue = {"I light the walls on fire.",
									 "The old house is quickly engulfed in flame.",
									 "Finally some warmth..."},
			.choices = {{"Stay a little longer", "neighbor_house_burn"}, {"Leave", "neighbor"}},
			.condition = [](State &state)
			{ return state.flags["has_matches"]; },
			.update = [](State &state)
			{
				if (!state.flags["neighbor_house_burned"])
					state.humanity -= 20;
				state.flags["neighbor_house_burned"] = true; }};

	/* RIVER */
	passages["river"] = new Passage{
			.ambient_temperature = -9,
			.dialogue = {"It's even colder here.",
									 "The currents under the frozen river seem to whisk away any remaining warmth.",
									 "There's a tent. I can't imagine it keeps the cold out, but maybe it's got some supplies inside."},
			.choices = {{"Go into the tent", "river_tent"}, {"Collect firewood", "river_firewood"}, {"Cross the river", "river_cross"}, {"Leave", "house_outside"}},
	};

	passages["river_firewood"] = new Passage{
			.ambient_temperature = -9,
			.dialogue = {"I use the axe to cut apart some of the dead wood along the riverbank.",
									 "It takes a while, but I gather enough dry wood to bring back to town."},
			.choices = {{"Leave", "river"}},
			.condition = [](State &state)
			{ return state.flags["has_axe"] && !state.flags["has_firewood"]; },
			.update = [](State &state)
			{ state.flags["has_firewood"] = true; }};

	passages["river_cross"] = new Passage{
			.ambient_temperature = -30,
			.dialogue = {"Maybe there's hope across this river.",
									 "Surely at this temperature the ice is frozen solid, right?",
									 "*crack*",
									 "Oh no.",
									 "I fall through the ice and am quickly swept away by the current."
									 "My body goes numb before I know what is happening."
									 "My lungs fill with cold water."},
			.ending = Ending::Drowned,
	};

	passages["river_tent"] = new Passage{
			.ambient_temperature = -6,
			.dialogue = {"I enter the tent.",
									 "There's someone in here! She's wearing a jacket. I don't recognize her.",
									 "She aims a pistol at me.",
									 "Stranger: I'm so cold. Give me something."},
			.choices = {{"Here, you can have my scarf.", "river_tent_scarf"}, {"Defend myself", "river_tent_murder"}, {"I'm not giving you anything.", "river_killed"}},
			.condition = [](State &state)
			{
				// Don't return to the tent if you've already escaped
				return !state.flags["escaped_tent"];
			},
	};

	passages["river_killed"] = new Passage{
			.ambient_temperature = -6,
			.dialogue = {"The stranger pulls her arm back and brings the pistol down on my head.",
									 "My vision goes blurry and I fall.",
									 "I feel her rummage through my pockets and steal my clothes.",
									 "She throws me out of the tent into the snow.",
									 "I can't move. Everything goes black..."},
			.ending = Ending::Killed,
	};

	passages["river_tent_scarf"] = new Passage{
			.ambient_temperature = -6,
			.dialogue = {
					"She takes my scarf without hesitating.",
					"Her aim drops while she wraps it around herself.",
					"I have this brief opportunity to act",
			},
			.choices = {{"Defend myself", "river_tent_murder"}, {"Escape", "river"}},
			.condition = [](State &state)
			{ return state.has_clothes("White Scarf") || state.has_clothes("Red Scarf"); },
			.update = [](State &state)
			{
				if (state.has_clothes("White Scarf")) state.remove_clothes("White Scarf");
				if (state.has_clothes("Red Scarf")) state.remove_clothes("Red Scarf");
				state.flags["stranger_has_scarf"] = true; }};

	passages["river_tent_murder"] = new Passage{
			.ambient_temperature = -6,
			.dialogue = {"I swing the axe before the stranger notices what's happening",
									 "...",
									 "It's over in an instant.",
									 "I grab the gun. It wasn't loaded.",
									 "...",
									 "Human blood is warm..."},
			.choices = {{"Leave", "river"}},
			.condition = [](State &state)
			{ return state.flags["has_axe"]; },
			.update = [](State &state)
			{
				state.humanity -= 50;
				state.flags["escaped_tent"] = true;
				state.flags["has_gun"] = true;
				state.clothes.emplace_back(new Clothes{"Blood", 8, 4});
				state.clothes.emplace_back(new Clothes{"Winter Jacket", 4, 7});
				if (state.flags["stranger_has_scarf"])
				{
					state.clothes.emplace_back(new Clothes{"Red Scarf", 5, state.scarf_durability});
				} },
	};

	/* TOWN */
	passages["town"] = new Passage{
			.ambient_temperature = -6,
			.dialogue = {"The roads are completely empty, except for the snow.",
									 "I can hardly make out what any of these buildings used to be.",
									 "I think I remember that one -- the general store.",
									 "There seem to be some people inside."},
			.choices = {{"Enter the store", "town_shop"}, {"Go back", "neighbor"}},
	};

	passages["town_shop"] = new Passage{
			.ambient_temperature = -4,
			.dialogue = {"...It's a little warmer, at least. The shelves are mostly bare.",
									 "A handful of people are camping out between the aisles.",
									 "Most of them are barely moving. Conserving their energy.",
									 "I recognize one of them -- Mr. Drake. He used to work here."},
			.choices = {{"Talk to Mr. Drake", "town_drake"}, {"Leave the shop", "town"}},
			.condition = [](State &state)
			{ return !state.flags["store_robbed"]; },
	};

	passages["town_drake"] = new Passage{
			.ambient_temperature = -4,
			.dialogue = {
					"Mr. Drake? Is that you?",
					"Mr. Drake: I'm surprised anyone remembers me. You'll forgive me if I can't offer you the same.",
					"Mr. Drake: It's been a while since anyone's come in here. Aside from, well... these folks.",
					"Mr. Drake: They came here back when the shelves were still stocked. Made supplies last for a while.",
					"Why are you still here, Mr. Drake?",
					"Mr. Drake: Me? Well at first it was just work. Suppose I thought the winter would pass.",
					"Mr. Drake: Even after the first big snowfall, we were still getting shipments...",
					"Mr. Drake: ...For the first few months, at least.",
					"Mr. Drake: Now it's just warmer than home, what with all these living bodies in here.",
					"I suppose you don't have anything left to sell me then.",
					"Mr. Drake: Maybe not sell, but... I can trade.",
					"Mr. Drake: If you'd be willing, maybe you could get us some firewood.",
					"Mr. Drake: I'd let you borrow my old axe if you could give me something as collateral."},
			.choices = {{"Buy the torn blanket", "town_buy_blanket"}, {"Buy the matches", "town_buy_matches"}, {"Buy the old hiking boots", "town_buy_boots"}, {"Rob them with the gun", "town_rob_gun"}, {"Offer my scarf for the axe", "town_borrow_axe"}, {"Return the firewood", "town_return_firewood"}, {"Rob them with the axe", "town_rob_axe"}, {"Leave", "town"}},
	};

	passages["town_buy_blanket"] = new Passage{
			.ambient_temperature = -4,
			.dialogue = {"All I can offer you is this wallet...",
									 "Mr. Drake: I'll take it. Maybe money's still worth something to someone out there.",
									 "Mr. Drake: Blanket's all yours.",
									 "Thank you."},
			.choices = {{"Rob them with the gun", "town_rob_gun"}, {"Offer my scarf for the axe", "town_borrow_axe"}, {"Return the firewood", "town_return_firewood"}, {"Rob them with the axe", "town_rob_axe"}, {"Leave", "town"}},
			.condition = [](State &state)
			{ return state.flags["has_wallet"] && !state.flags["store_robbed"]; },
			.update = [](State &state)
			{
				state.flags["has_wallet"] = false;
				state.clothes.emplace_back(new Clothes{"Torn Blanket", 5, 10}); }};

	passages["town_buy_matches"] = new Passage{
			.ambient_temperature = -4,
			.dialogue = {"All I can offer you is this wallet...",
									 "Mr. Drake: I'll take it. Maybe money's still worth something to someone out there.",
									 "Mr. Drake: Matches are all yours.",
									 "Thank you."},
			.choices = {{"Rob them with the gun", "town_rob_gun"}, {"Offer my scarf for the axe", "town_borrow_axe"}, {"Return the firewood", "town_return_firewood"}, {"Rob them with the axe", "town_rob_axe"}, {"Leave", "town"}},
			.condition = [](State &state)
			{ return state.flags["has_wallet"] && !state.flags["store_robbed"]; },
			.update = [](State &state)
			{
				state.flags["has_wallet"] = false;
				state.flags["has_matches"] = true; }};

	passages["town_buy_boots"] = new Passage{
			.ambient_temperature = -4,
			.dialogue = {"All I can offer you is this wallet...",
									 "Mr. Drake: I'll take it. Maybe money's still worth something to someone out there.",
									 "Mr. Drake: Boots are all yours.",
									 "Thank you."},
			.choices = {{"Rob them with the gun", "town_rob_gun"}, {"Offer my scarf for the axe", "town_borrow_axe"}, {"Return the firewood", "town_return_firewood"}, {"Rob them with the axe", "town_rob_axe"}, {"Leave", "town"}},
			.condition = [](State &state)
			{ return state.flags["has_wallet"] && !state.flags["store_robbed"]; },
			.update = [](State &state)
			{
				state.flags["has_wallet"] = false;
				state.clothes.emplace_back(new Clothes{"Old Hiking Boots", 4, 14}); }};

	passages["town_rob_gun"] = new Passage{
			.ambient_temperature = -4,
			.dialogue = {"I pull out the gun. Everyone looks scared.",
									 "Sorry about this, Mr. Drake. I'm desperate.",
									 "Mr. Drake: God damn you... we're all desperate.",
									 "I guess that means I'm just more desperate.",
									 "Mr. Drake: Nah... just means you were lucky enough to get a gun before I did.",
									 "I guess so. Enough talking... give me everything. The matches, the blanket, the boots.",
									 "Don't even think about reaching for the axe."},
			.choices = {{"Leave", "town"}},
			.condition = [](State &state)
			{ return state.flags["has_gun"] && !state.flags["store_robbed"]; },
			.update = [](State &state)
			{
				state.humanity -= 30;
				state.flags["store_robbed"] = true;
				state.flags["has_matches"] = true;
				if (!state.has_clothes("Torn Blanket")) state.clothes.emplace_back(new Clothes{"Torn Blanket", 5, 10});
				if (!state.has_clothes("Old Hiking Boots")) state.clothes.emplace_back(new Clothes{"Old Hiking Boots", 4, 14}); }};

	passages["town_borrow_axe"] = new Passage{
			.ambient_temperature = -4,
			.dialogue = {"Mr. Drake: That's a nice scarf... oughtta keep me warm for a while.",
									 "Mr. Drake: There's some dead trees down by the river. Should be dry enough.",
									 "Got it. I'll head there now.",
									 "Mr. Drake: Thanks, Nate. You're doing us all a favor. Don't freeze out there."},
			.choices = {{"Rob them with the gun", "town_rob_gun"}, {"Rob them with the axe", "town_rob_axe"}, {"Leave", "town"}},
			.condition = [](State &state)
			{ return !state.flags["store_robbed"] && !state.flags["has_axe"] && !state.flags["store_has_scarf"] && (state.has_clothes("White Scarf") || state.has_clothes("Red Scarf")); },
			.update = [](State &state)
			{
				if (state.has_clothes("White Scarf"))
				{
					state.remove_clothes("White Scarf");
					state.flags["store_has_white_scarf"] = true;
				}
				else if (state.has_clothes("Red Scarf"))
				{
					state.remove_clothes("Red Scarf");
					state.flags["store_has_red_scarf"] = true;
				}

				state.flags["store_has_scarf"] = true;
				state.flags["has_axe"] = true; }};

	passages["town_return_firewood"] = new Passage{
			.ambient_temperature = -4,
			.dialogue = {"Here, Mr. Drake. I brought the firewood you asked for.",
									 "You can have your axe back too. I don't think I'll be needing it anymore.",
									 "Mr. Drake: I knew I could trust you. Here, have your scarf back.",
									 "Mr. Drake: You can have any of my items as thanks. This firewood means a lot to us."},
			.choices = {{"I'll take the torn blanket", "town_firewood_blanket"}, {"I'll take the matches", "town_firewood_matches"}, {"I'll take the old hiking boots", "town_firewood_boots"}},
			.condition = [](State &state)
			{ return !state.flags["store_robbed"] && state.flags["has_firewood"] && state.flags["store_has_scarf"]; },
			.update = [](State &state)
			{
				state.humanity += 20;
				state.flags["has_firewood"] = false;
				state.flags["has_axe"] = false;
				state.flags["store_has_scarf"] = false;

				if (state.flags["store_has_white_scarf"])
				{
					state.clothes.emplace_back(new Clothes{"White Scarf", 5, state.scarf_durability});
					state.flags["store_has_white_scarf"] = false;
				}
				else if (state.flags["store_has_red_scarf"])
				{
					state.clothes.emplace_back(new Clothes{"Red Scarf", 5, state.scarf_durability});
					state.flags["store_has_red_scarf"] = false;
				} }};

	passages["town_firewood_blanket"] = new Passage{
			.ambient_temperature = -4,
			.dialogue = {
					"Mr Drake: Here's the blanket, Nate. Hope it keeps you warm.",
					"Thanks."},
			.choices = {{"Leave", "town"}},
			.update = [](State &state)
			{ if (!state.has_clothes("Torn Blanket")) state.clothes.emplace_back(new Clothes{"Torn Blanket", 5, 10}); }};

	passages["town_firewood_matches"] = new Passage{
			.ambient_temperature = -4,
			.dialogue = {
					"Mr Drake: Here's the matches, Nate. Hope you can find something to light with them.",
					"Thanks."},
			.choices = {{"Leave", "town"}},
			.update = [](State &state)
			{ state.flags["has_matches"] = true; }};

	passages["town_firewood_boots"] = new Passage{
			.ambient_temperature = -4,
			.dialogue = {
					"Mr Drake: Here's the boots, Nate. Hope they keep your feet warm in this snow.",
					"Thanks."},
			.choices = {{"Leave", "town"}},
			.update = [](State &state)
			{ if (!state.has_clothes("Old Hiking Boots")) state.clothes.emplace_back(new Clothes{"Old Hiking Boots", 4, 14}); }};

	passages["town_rob_axe"] = new Passage{
			.ambient_temperature = -4,
			.dialogue = {"I raise the axe over my head.",
									 "Sorry Mr. Drake... I'm desperate.",
									 "Mr Drake: Bad choice kid. you're outnumbered.",
									 "Mr Drake: That axe isn't enough to take all of us, and you gave us just the excuse we needed.",
									 "I hear shuffling behind me. The motionless store campers have begun to move.",
									 "Before I can turn around, I feel something hard hit the back of my head.",
									 "My vision goes blurry and I fall to my knees.",
									 "They take everything I have and throw me back out into the snow before I lose consciousness."},
			.ending = Ending::Killed,
			.condition = [](State &state)
			{ return state.flags["has_axe"]; },
			.update = [](State &state)
			{
				for (Clothes *c : state.clothes) delete c;
				state.clothes.clear();
				state.flags["has_axe"] = false;
				state.flags["has_gun"] = false;
				state.flags["has_wallet"] = false;
				state.flags["has_matches"] = false; },
	};

	/* MOUNTAIN */
	passages["mountain_foot"] = new Passage{
			.ambient_temperature = -12,
			.dialogue = {"The air is colder up here.",
									 "I used to hike this mountain when I was a kid.",
									 "Somehow it seems more intimidating now.",
									 "Maybe I just feel weaker.",
									 "Shortly after the snow started falling, people started talking about the mountain.",
									 "They used to say that the top was above the snow clouds.",
									 "That the sunlight would warm your skin...",
									 "...",
									 "Well, everyone who went that way never came back.",
									 "Maybe they really did find paradise up there."},
			.choices = {{"Climb", "mountain_slope"}, {"Return", "house_outside"}},
	};

	passages["mountain_slope"] = new Passage{
			.ambient_temperature = -15,
			.dialogue = {"The first stretch is just how I remember it, even though the path is drowned in snow.",
									 "It gets steeper from here... and icier too.",
									 "I can't risk trying to climb any farther without better shoes."},
			.choices = {{"Climb higher", "mountain_top"}, {"Descend", "mountain_foot"}},
	};

	passages["mountain_top"] = new Passage{
			.ambient_temperature = -18,
			.dialogue = {"The old hiking boots bite into the snow well enough to keep climbing.",
									 "The fog is so disorienting. I can hardly tell where I'm going.",
									 "I can barely make out my surroundings. There are some strange rocks in front of me.",
									 "...",
									 "Oh... I'm through the fog. Not fog, but... clouds.",
									 "It was true. The sky is clear up here. The sunlight is touching my skin...",
									 "...But it's cold.",
									 "It's colder here than anywhere else I've been.",
									 "It feels almost like the sunlight is stealing warmth from my body.",
									 "The clear sky is a curse. My surroundings are clear to me now.",
									 "Not rocks. Frozen bodies. I recognize them. My neighbor's kids.",
									 "Some paradise."},
			.choices = {{"Stay", "mountain_stay"}, {"Descend", "mountain_slope"}},
			.condition = [](State &state)
			{ return state.has_clothes("Old Hiking Boots"); },
	};

	passages["mountain_stay"] = new Passage{
			.ambient_temperature = -18,
			.dialogue = {"I stay at the summit a little longer.",
									 "At least here I can see the sun."},
			.choices = {{"Stay", "mountain_stay"}, {"Descend", "mountain_slope"}},
	};

	/* AUTOMATIC ENDINGS */
	passages["ending_frozen"] = new Passage{
			.ambient_temperature = 0,
			.dialogue = {
					"For a second it feels like my flesh is burning, then it goes completely numb.",
					"I can no longer move.",
					"I succumb to the cold."},
			.ending = Ending::Frozen,
	};

	passages["ending_burnt"] = new Passage{
			.ambient_temperature = 0,
			.dialogue = {
					"Warmth... for the first time in so long.",
					"Before I know it, warmth turns to heat...",
					"Then heat turns to pain. But it's too late.",
					"Pain turns to nothing, and my flesh turns to ash."},
			.ending = Ending::Burnt,
	};

	passages["ending_mountain"] = new Passage{
			.ambient_temperature = 0,
			.dialogue = {
					"I was stupid to think there would be hope here.",
					"I can no longer move.",
					"I succumb to the cold, surrounded by others who did the same.",
					"I walked on them to get here. For what? To end just as they did.",
			},
			.ending = Ending::Mountain,
	};

	passages["ending_mountain_heaven"] = new Passage{
			.ambient_temperature = 0,
			.dialogue = {
					"I can no longer move my body.",
					"My vision goes black.",
					"At least I followed my hope...",
					"...",
					"Suddenly I feel... warm...",
					"Voice: Nathan, is that you?",
					"Hello? Mom?",
					"Mom's Voice: Nathan, we've been waiting for you.",
					"Mom's Voice: Isn't it warm here?"},
			.ending = Ending::MountainHeaven,
	};

	current_passage_id = "house";
	current_passage = passages[current_passage_id];
	return current_passage;
}
