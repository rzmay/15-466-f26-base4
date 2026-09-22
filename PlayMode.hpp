#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"
#include "ChoiceStory.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

struct PlayMode : Mode
{
	PlayMode();
	virtual ~PlayMode();

	// functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	// Story
	ChoiceStory story;

	// Dialogue & choice rendering
	size_t dialogue_i = 0;
	size_t choice_i = 0;
	std::vector<std::string> choices;

	// Map marker default
	std::string marker_passage_id = "house";

	// Cloud anim
	float cloud_time = 0.0f;

	// Music
	std::shared_ptr<Sound::PlayingSample> music_base;
	std::shared_ptr<Sound::PlayingSample> music_cold;
	std::shared_ptr<Sound::PlayingSample> music_humanity;
};
