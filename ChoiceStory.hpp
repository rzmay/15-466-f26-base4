#include <algorithm>
#include <functional>
#include <map>
#include <string>
#include <vector>

struct ChoiceStory
{
	// Available endings -- NONE if not ended yet
	enum Ending
	{
		NONE,
		Frozen,
		Burnt,
		Killed,
		Drowned,
		Mountain,
		MountainHeaven,
	};

	// Clothes determine resistance to cold
	struct Clothes
	{
		std::string name;
		int resistance;
		int durability; // How many passages does this last through? Negative = infinite
	};

	// Story variables
	struct State
	{
		int temperature; // Dead when reaches 0
		int humanity;
		std::vector<Clothes *> clothes;
		Ending ending = NONE;

		// We have to store this bc the scarf changes hands a lot
		// Re-initializing without storing durability causes extra magic durability to be added
		int scarf_durability = 16;

		// Persistent story flags
		std::map<std::string, bool> flags;

		int passages_survived = 0;

		// Check if you have clothes by name
		bool has_clothes(const std::string &name) const
		{
			return std::any_of(
					clothes.begin(),
					clothes.end(),
					[&](const Clothes *c)
					{
						return c->name == name;
					});
		}

		// Remove an article of clothing by name
		void remove_clothes(const std::string &name)
		{
			auto it = std::find_if(
					clothes.begin(),
					clothes.end(),
					[&](const Clothes *c)
					{
						return c->name == name;
					});

			if (it == clothes.end())
				return;

			delete *it;
			clothes.erase(it);
		}
	};

	struct Passage
	{
		int ambient_temperature;

		std::vector<std::string> dialogue;
		std::map<std::string, std::string> choices;

		Ending ending = NONE;

		// Condition for this passage being available
		std::function<bool(State &)> condition =
				[](State &)
		{ return true; };

		// Update state when entering this passage
		std::function<void(State &)> update =
				[](State &) {};
	};

	State *state = nullptr;
	std::map<std::string, Passage *> passages;

	std::string previous_passage_id;
	std::string current_passage_id;
	Passage *current_passage = nullptr;

	// Get temperature resistance based on clothes
	int get_temperature_resistance();

	// Get valid choices from current passage and state
	std::vector<std::string> get_choices();

	// Make a choice -- should return resulting current passage
	Passage *make_choice(std::string choice);

	// Init function for this game -- should return first passage
	Passage *init();

	// Final death message
	Passage *die(Ending ending);

	// Reset to the beginning
	Passage *reset();
};
