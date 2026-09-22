#include "PlayMode.hpp"

#include "ColorTextureProgram.hpp"
#include "load_save_png.hpp"

#include "DrawLines.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <hb.h>
#include <hb-ft.h>

#include <string>
#include <random>

/*
 * Credit to Bernardo Miranda for the following code adapted from his PlayMode:
 *	struct Vert
 *	struct Glyph
 *	static Glyph const &glyph(uint32_t index)
 *	static GLuint load_texture(std::string const &filename)
 *	static void sprite_draw(GLuint tex, float x, float y, float w, float h)
 *	static std::vector<std::string> wrap(std::string const &s, size_t n)
 *	static void text_draw(std::string const &line, float x, float y, glm::u8vec4 color, float scale = 1.0f)
 */
static FT_Face face = nullptr;
static hb_font_t *hb_font = nullptr;
static GLuint text_vao = 0, text_vbo = 0;

static const float CLOUD_SPEED = 0.25f;

struct Glyph
{
	GLuint tex;
	int w, h, bx, by;
};
static std::unordered_map<uint32_t, Glyph> glyphs;

struct Vert
{
	float x, y;
	glm::u8vec4 col;
	float u, v;
};

static Glyph const &glyph(uint32_t index)
{
	auto f = glyphs.find(index);
	if (f != glyphs.end())
		return f->second;

	FT_Load_Glyph(face, index, FT_LOAD_RENDER);
	FT_Bitmap &bm = face->glyph->bitmap;

	std::vector<glm::u8vec4> data(bm.width * bm.rows);
	for (uint32_t i = 0; i < bm.width * bm.rows; ++i)
		data[i] = glm::u8vec4(255, 255, 255, bm.buffer[i]);

	Glyph g;
	glGenTextures(1, &g.tex);
	glBindTexture(GL_TEXTURE_2D, g.tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bm.width, bm.rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	g.w = bm.width;
	g.h = bm.rows;
	g.bx = face->glyph->bitmap_left;
	g.by = face->glyph->bitmap_top;
	return glyphs[index] = g;
}

static GLuint load_texture(std::string const &filename)
{
	glm::uvec2 size;
	std::vector<glm::u8vec4> data;
	load_png(data_path(filename), &size, &data, LowerLeftOrigin);
	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, GLsizei(size.x), GLsizei(size.y), 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	return tex;
}

static void sprite_draw(GLuint tex, float x, float y, float w, float h)
{
	Vert verts[6] = {
			{x, y, glm::u8vec4(0xff), 0.0f, 0.0f},
			{x + w, y, glm::u8vec4(0xff), 1.0f, 0.0f},
			{x + w, y + h, glm::u8vec4(0xff), 1.0f, 1.0f},
			{x, y, glm::u8vec4(0xff), 0.0f, 0.0f},
			{x + w, y + h, glm::u8vec4(0xff), 1.0f, 1.0f},
			{x, y + h, glm::u8vec4(0xff), 0.0f, 1.0f},
	};
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);
	glBindTexture(GL_TEXTURE_2D, tex);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}

static std::vector<std::string> wrap(std::string const &s, size_t n)
{
	std::vector<std::string> out;
	for (size_t i = 0; i < s.size();)
	{
		size_t end = i + n < s.size() ? s.rfind(' ', i + n) : s.size();
		if (end == std::string::npos || end < i)
			end = std::min(i + n, s.size());
		out.push_back(s.substr(i, end - i));
		i = end;
		while (i < s.size() && s[i] == ' ')
			++i;
	}
	if (out.empty())
		out.emplace_back("");
	return out;
}

static void text_draw(std::string const &line, float x, float y, glm::u8vec4 color, float scale = 1.0f)
{
	if (!face)
	{
		FT_Library ft;
		FT_Init_FreeType(&ft);
		FT_New_Face(ft, data_path("m6x11.ttf").c_str(), 0, &face);
		FT_Set_Pixel_Sizes(face, 0, 32);
		hb_font = hb_ft_font_create_referenced(face);

		glGenVertexArrays(1, &text_vao);
		glGenBuffers(1, &text_vbo);
		glBindVertexArray(text_vao);
		glBindBuffer(GL_ARRAY_BUFFER, text_vbo);

		glVertexAttribPointer(color_texture_program->Position_vec4, 2, GL_FLOAT, GL_FALSE, sizeof(Vert), (GLbyte *)0 + offsetof(Vert, x));
		glEnableVertexAttribArray(color_texture_program->Position_vec4);
		glVertexAttribPointer(color_texture_program->Color_vec4, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vert), (GLbyte *)0 + offsetof(Vert, col));
		glEnableVertexAttribArray(color_texture_program->Color_vec4);
		glVertexAttribPointer(color_texture_program->TexCoord_vec2, 2, GL_FLOAT, GL_FALSE, sizeof(Vert), (GLbyte *)0 + offsetof(Vert, u));
		glEnableVertexAttribArray(color_texture_program->TexCoord_vec2);
	}

	glBindVertexArray(text_vao);
	glBindBuffer(GL_ARRAY_BUFFER, text_vbo);

	hb_buffer_t *buf = hb_buffer_create();
	hb_buffer_add_utf8(buf, line.c_str(), -1, 0, -1);
	hb_buffer_guess_segment_properties(buf);
	hb_shape(hb_font, buf, nullptr, 0);

	unsigned n;
	hb_glyph_info_t *info = hb_buffer_get_glyph_infos(buf, &n);
	hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(buf, &n);

	for (unsigned i = 0; i < n; ++i)
		x -= pos[i].x_advance * scale / 128.0f;

	for (unsigned i = 0; i < n; ++i)
	{
		Glyph const &g = glyph(info[i].codepoint);
		float gx = x + g.bx * scale, gy = y - (g.h - g.by) * scale;
		float gw = g.w * scale, gh = g.h * scale;
		Vert verts[6] = {
				{gx, gy, color, 0.0f, 1.0f},
				{gx + gw, gy, color, 1.0f, 1.0f},
				{gx + gw, gy + gh, color, 1.0f, 0.0f},
				{gx, gy, color, 0.0f, 1.0f},
				{gx + gw, gy + gh, color, 1.0f, 0.0f},
				{gx, gy + gh, color, 0.0f, 0.0f},
		};
		glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);
		glBindTexture(GL_TEXTURE_2D, g.tex);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		x += pos[i].x_advance * scale / 64.0f;
	}

	hb_buffer_destroy(buf);
}

// Textures
static GLuint map_tex = 0;
static GLuint cloud_tex = 0;
static GLuint frame_tex = 0;
static GLuint marker_tex = 0;
static GLuint edge_tex = 0;

// Music
static Load<Sound::Sample> music_base_sample(LoadTagDefault, []() -> Sound::Sample const *
																						 { return new Sound::Sample(data_path("icw_music_base.wav")); });

static Load<Sound::Sample> music_cold_sample(LoadTagDefault, []() -> Sound::Sample const *
																						 { return new Sound::Sample(data_path("icw_music_cold.wav")); });

static Load<Sound::Sample> music_humanity_sample(LoadTagDefault, []() -> Sound::Sample const *
																								 { return new Sound::Sample(data_path("icw_music_humanity.wav")); });

// Hard coded marker coordinates on map
static glm::vec2 marker_position(std::string const &id)
{
	if (id.rfind("river", 0) == 0)
		return glm::vec2(130.0f, 190.0f);

	if (id.rfind("neighbor", 0) == 0)
		return glm::vec2(220.0f, 260.0f);

	if (id.rfind("town", 0) == 0)
		return glm::vec2(360.0f, 260.0f);

	if (id == "mountain_foot")
		return glm::vec2(220.0f, 290.0f);

	if (id == "mountain_slope")
		return glm::vec2(190.0f, 350.0f);

	if (id == "mountain_top" || id == "mountain_stay")
		return glm::vec2(190.0f, 380.0f);

	// Default is home
	return glm::vec2(100.0f, 250.0f);
}

PlayMode::PlayMode()
{
	// Load textures
	map_tex = load_texture("map.png");
	cloud_tex = load_texture("clouds.png");
	frame_tex = load_texture("frame.png");
	marker_tex = load_texture("marker.png");
	edge_tex = load_texture("frame_border.png");

	story.init();
	choices = story.get_choices();

	// Start music
	music_base = Sound::loop(*music_base_sample, 0.0f);
	music_cold = Sound::loop(*music_cold_sample, 0.0f);
	music_humanity = Sound::loop(*music_humanity_sample, 0.0f);

	// Volume defaults
	music_base->set_volume(1.0f, 0.0f);
	music_cold->set_volume(0.0f, 0.0f);
	music_humanity->set_volume(1.0f, 0.0f);
}

PlayMode::~PlayMode()
{
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size)
{

	if (evt.type == SDL_EVENT_KEY_DOWN)
	{
		// Choosing options if finished dialogue
		if (dialogue_i >= story.current_passage->dialogue.size())
		{
			// Detect ending state and allow restart
			if (story.state->ending != ChoiceStory::Ending::NONE)
			{
				if (evt.key.key == SDLK_RETURN)
				{
					story.reset();

					marker_passage_id = story.current_passage_id;
					dialogue_i = 0;
					choice_i = 0;
					choices = story.get_choices();

					return true;
				}

				return true;
			}

			if (evt.key.key == SDLK_UP)
			{
				choice_i = choice_i == 0 ? choices.size() - 1 : choice_i - 1;
				return true;
			}
			else if (evt.key.key == SDLK_DOWN)
			{
				choice_i = (choice_i + 1) % choices.size();
				return true;
			}
			else if (evt.key.key == SDLK_RETURN)
			{
				std::string const choice = choices[choice_i];
				story.make_choice(choice);

				marker_passage_id = story.current_passage_id;
				dialogue_i = 0;
				choice_i = 0;
				choices = story.get_choices();
				return true;
			}
		}
		else if (evt.key.key == SDLK_RETURN)
		{
			dialogue_i++;
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed)
{
	// Update cloud animation
	cloud_time += elapsed;

	// Update volumes
	float cold = glm::clamp(1.0f - (float(story.state->temperature) / 100.0f), 0.0f, 1.0f);
	float humanity = glm::clamp(float(story.state->humanity) / 100.0f, 0.0f, 1.0f);

	music_cold->set_volume(cold, 2.0f);
	music_humanity->set_volume(humanity, 2.0f);
}

void PlayMode::draw(glm::uvec2 const &drawable_size)
{
	glUseProgram(0);

	// Credit to Bernardo Miranda for initialization code
	glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glm::mat4 to_clip = glm::ortho(0.0f, float(drawable_size.x), 0.0f, float(drawable_size.y));
	glUseProgram(color_texture_program->program);
	glUniformMatrix4fv(color_texture_program->OBJECT_TO_CLIP_mat4, 1, GL_FALSE, glm::value_ptr(to_clip));

	glBindVertexArray(text_vao);
	glBindBuffer(GL_ARRAY_BUFFER, text_vbo);

	float w = float(drawable_size.x);
	float h = float(drawable_size.y);
	float scale = std::min(w, h) / 512.0f;
	float size = 512.0f * scale;

	float ox = (w - size) * 0.5f;
	float oy = (h - size) * 0.5f;
	glm::u8vec4 white(0xff, 0xff, 0xff, 0xff);
	glm::u8vec4 selected(0xd8, 0xe8, 0xc8, 0xff);

	// Draw map first, bottom layer
	sprite_draw(map_tex, ox, oy, size, size);

	// Draw map marker (10x10)
	glm::vec2 marker_pos = marker_position(marker_passage_id);
	sprite_draw(marker_tex,
							ox + (marker_pos.x - 5.0f) * scale,
							oy + (marker_pos.y - 5.0f) * scale,
							10.0f * scale,
							10.0f * scale);

	// Draw clouds moving 21px max (frame margin)
	float cloud_x = 21.0f * std::sin(cloud_time * CLOUD_SPEED);
	sprite_draw(cloud_tex, ox + cloud_x * scale, oy, size, size);

	// Draw edges to cover clouds
	sprite_draw(edge_tex, 0.0f, oy, ox, size);
	sprite_draw(edge_tex, ox + size, oy, ox, size);

	// Draw frame
	sprite_draw(frame_tex, ox, oy, size, size);

	// Draw status text
	float status_x = ox + 45.0f * scale;
	float status_y = oy + 100.0f * scale;
	float status_step = 18.0f * scale;
	float table_step = 50.0f * scale;
	float status_scale = 0.3f * scale;

	text_draw("TEMP: " + std::to_string(story.state->temperature), status_x, status_y, white, status_scale);
	text_draw("RESIST: " + std::to_string(story.get_temperature_resistance()), status_x + table_step, status_y, white, status_scale);
	status_y -= status_step * 1.5f;

	// Clothes header
	text_draw("CLOTHES", status_x, status_y, white, status_scale);
	text_draw("DURABILITY", status_x + table_step, status_y, white, status_scale);
	status_y -= status_step;

	for (ChoiceStory::Clothes const *c : story.state->clothes)
	{
		text_draw(c->name, status_x, status_y, white, status_scale);
		text_draw(std::to_string(c->durability), status_x + table_step, status_y, white, status_scale);
		status_y -= status_step;
	}

	// Draw dialogue text
	float text_x = ox + 326.0f * scale;
	float text_y = oy + 90.0f * scale;
	float text_step = 18.0f * scale;
	float text_scale = 0.5 * scale;

	// If dialogue
	if (dialogue_i < story.current_passage->dialogue.size())
	{
		std::vector<std::string> lines = wrap(story.current_passage->dialogue[dialogue_i], 48);
		for (std::string const &line : lines)
		{
			text_draw(line, text_x, text_y, white, text_scale);
			text_y -= text_step;
		}
	}
	else
	{
		// Detect ending state
		if (story.state->ending != ChoiceStory::Ending::NONE)
		{
			text_draw("PRESS ENTER TO PLAY AGAIN", text_x, text_y, white, text_scale);
		}
		else
		{
			// Choosing
			for (size_t i = 0; i < choices.size(); ++i)
			{
				std::string line = (i == choice_i ? "> " : "  ") + choices[i];
				text_draw(line, text_x, text_y, i == choice_i ? selected : white, text_scale);
				text_y -= text_step;
			}
		}
	}

	GL_ERRORS();
}
