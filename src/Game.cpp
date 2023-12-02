#include "Game.hpp"
#include "Constants.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include <cstdint>
#include <iostream>
#include <cmath>

Game::Game() : 
	initialized_(false), 
	running_(false), 
	cell_size_(32), 
	cells_width_(constants::screen_width / cell_size_), 
	cells_height_(constants::screen_height / cell_size_), 
	mouse_left_pressed_(false), 
	mouse_right_pressed_(false), 
	setting_walls_(true), 
	render_line_(false)
{
	initialized_ = Initialize();

	board_.resize(cells_width_ * cells_height_);

	for (int y = 0; y < cells_height_; ++y)
	{
		for (int x = 0; x < cells_width_; ++x)
		{
			const int index = y * cells_width_ + x;

			board_[index].rect_.x = x * cell_size_;
			board_[index].rect_.y = y * cell_size_;
			board_[index].rect_.w = cell_size_;
			board_[index].rect_.h = board_[index].rect_.w;
			board_[index].is_wall_ = false;
			board_[index].highlighted_ = false;
		}
	}

	const int box_size = 10;

	player_.box_.x = (constants::screen_width * 1 / 3) - (box_size / 2);
	player_.box_.y = (constants::screen_height / 2) - (box_size / 2);
	player_.box_.w = box_size;
	player_.box_.h = box_size;
	player_.vx_ = 0;
	player_.vy_ = 0;

	mouse_box_.x = (constants::screen_width * 2 / 3) - (box_size / 2);
	mouse_box_.y = (constants::screen_height / 2) - (box_size / 2);
	mouse_box_.w = box_size;
	mouse_box_.h = box_size;

	mouse_position_ = { 0, 0 };
	dda_intersection_ = { -1.0f, -1.0f };
}

Game::~Game()
{
	Finalize();
}

bool Game::Initialize()
{
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		printf("SDL could not be initialized! SDL Error: %s\n", SDL_GetError());
		return false;
	}

	if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0"))
	{
		printf("%s\n", "Warning: Texture filtering is not enabled!");
	}

	window_ = SDL_CreateWindow(constants::game_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, constants::screen_width, constants::screen_height, SDL_WINDOW_SHOWN);

	if (window_ == nullptr)
	{
		printf("Window could not be created! SDL Error: %s\n", SDL_GetError());
		return false;
	}

	renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);

	if (renderer_ == nullptr)
	{
		printf("Renderer could not be created! SDL Error: %s\n", SDL_GetError());
		return false;
	}

	constexpr int img_flags = IMG_INIT_PNG;

	if (!(IMG_Init(img_flags) & img_flags))
	{
		printf("SDL_image could not be initialized! SDL_image Error: %s\n", IMG_GetError());
		return false;
	}

	return true;
}

void Game::Finalize()
{
	SDL_DestroyWindow(window_);
	window_ = nullptr;
	
	SDL_DestroyRenderer(renderer_);
	renderer_ = nullptr;

	SDL_Quit();
	IMG_Quit();
}

void Game::Run()
{
	if (!initialized_)
	{
		return;
	}

	running_ = true;

	constexpr double ms = 1.0 / 60.0;
	std::uint64_t last_time = SDL_GetPerformanceCounter();
	long double delta = 0.0;

	double timer = SDL_GetTicks();

	int frames = 0;
	int ticks = 0;

	while (running_)
	{
		const std::uint64_t now = SDL_GetPerformanceCounter();
		const long double elapsed = static_cast<long double>(now - last_time) / static_cast<long double>(SDL_GetPerformanceFrequency());

		last_time = now;
		delta += elapsed;

		HandleEvents();

		while (delta >= ms)
		{
			Tick();
			delta -= ms;
			++ticks;
		}

		//printf("%Lf\n", delta / ms);
		Render();
		++frames;

		if (SDL_GetTicks() - timer > 1000.0)
		{
			timer += 1000.0;
			//printf("Frames: %d, Ticks: %d\n", frames, ticks);
			frames = 0;
			ticks = 0;
		}
	}
}

void Game::HandleEvents()
{
	SDL_Event e;

	while (SDL_PollEvent(&e) != 0)
	{
		SDL_GetMouseState(&mouse_position_.x, &mouse_position_.y);

		if (e.type == SDL_QUIT)
		{
			running_ = false;
			return;
		}
		else if (e.type == SDL_MOUSEBUTTONDOWN)
		{
			if (e.button.button == SDL_BUTTON_LEFT)
			{
				render_line_ = true;
				mouse_left_pressed_ = true;
			}
			else if (e.button.button == SDL_BUTTON_RIGHT)
			{
				mouse_right_pressed_ = true;
				const int index = (mouse_position_.y / cell_size_) * cells_width_ + (mouse_position_.x / cell_size_);
				setting_walls_ = !board_[index].is_wall_;
				board_[index].is_wall_ = !board_[index].is_wall_;
			}
		}
		else if (e.type == SDL_MOUSEBUTTONUP)
		{
			if (e.button.button == SDL_BUTTON_LEFT)
			{
				render_line_ = false;
				mouse_left_pressed_ = false;
				dda_intersection_ = { -1.0, -1.0 };
			}
			else if (e.button.button == SDL_BUTTON_RIGHT)
			{
				mouse_right_pressed_ = false;
			}
		}
		
		if (e.type == SDL_MOUSEMOTION)
		{
			mouse_box_.x = mouse_position_.x - mouse_box_.w / 2;
			mouse_box_.y = mouse_position_.y - mouse_box_.w / 2;

			if (mouse_right_pressed_)
			{
				const int index = (mouse_position_.y / cell_size_) * cells_width_ + (mouse_position_.x / cell_size_);
				board_[index].is_wall_ = setting_walls_;
			}
		}

		int speed = 5;

		if (e.type == SDL_KEYDOWN)
		{
			if (e.key.keysym.sym == SDLK_w)
			{
				player_.vy_ = -speed;
			}

			if (e.key.keysym.sym == SDLK_a)
			{
				player_.vx_ = -speed;
			}

			if (e.key.keysym.sym == SDLK_s)
			{
				player_.vy_ = speed;
			}

			if (e.key.keysym.sym == SDLK_d)
			{
				player_.vx_ = speed;
			}
		}
		else if (e.type == SDL_KEYUP)
		{
			if (e.key.keysym.sym == SDLK_w)
			{
				player_.vy_ = 0;
			}

			if (e.key.keysym.sym == SDLK_a)
			{
				player_.vx_ = 0;
			}

			if (e.key.keysym.sym == SDLK_s)
			{
				player_.vy_ = 0;
			}

			if (e.key.keysym.sym == SDLK_d)
			{
				player_.vx_ = 0;
			}
		}
	}
}

void Game::Tick()
{
	player_.box_.x += player_.vx_;
	player_.box_.y += player_.vy_;

	if (mouse_left_pressed_)
	{
		DigitalDifferentialAnalysis();
	}
}

void Game::DigitalDifferentialAnalysis()
{
	Vector2d<float> player_pos = { static_cast<float>(player_.box_.x + (player_.box_.w / 2)), static_cast<float>(player_.box_.y + (player_.box_.h / 2)) };
	Vector2d<float> mouse_pos = { static_cast<float>(mouse_box_.x + (mouse_box_.w / 2)), static_cast<float>(mouse_box_.y + (mouse_box_.h / 2)) };

	if (player_pos.x < 0 || player_pos.x > constants::screen_width || player_pos.y < 0 || player_pos.y > constants::screen_height)
	{
		return;
	}

	if (mouse_pos.x < 0 || mouse_pos.x > constants::screen_width || mouse_pos.y < 0 || mouse_pos.y > constants::screen_height)
	{
		return;
	}

	const Vector2d<float> ray_start = { player_pos.x, player_pos.y };
	
	Vector2d<float> ray_dir = { mouse_pos.x - player_pos.x, mouse_pos.y - player_pos.y };
	ray_dir.SetLength(static_cast<float>(cell_size_));

	Vector2d<float> unit_ray_dir = { mouse_pos.x - player_pos.x, mouse_pos.y - player_pos.y };
	unit_ray_dir.SetLength(1.0);
	
	Vector2d<float> ray_step_size = { std::sqrt(1 + ((ray_dir.y / ray_dir.x) * (ray_dir.y / ray_dir.x))), std::sqrt(1 + ((ray_dir.x / ray_dir.y) * (ray_dir.x / ray_dir.y))) };
	ray_step_size.x *= static_cast<float>(cell_size_);
	ray_step_size.y *= static_cast<float>(cell_size_);
	
	Vector2d<int> map_check = { static_cast<int>(ray_start.x), static_cast<int>(ray_start.y) };
	map_check.x = map_check.x - (map_check.x % cell_size_);
	map_check.y = map_check.y - (map_check.y % cell_size_);

	Vector2d<float> ray_length = { 0.0f, 0.0f };
	Vector2d<int> step = { 0, 0 };

	if (ray_dir.x < 0)
	{
		step.x = -cell_size_;
		
		if (static_cast<int>(ray_start.x) % cell_size_ == 0)
		{
			ray_length.x = ray_step_size.x;
		}
		else
		{
			ray_length.x = ((ray_start.x - static_cast<float>(map_check.x)) / cell_size_) * ray_step_size.x;
		}
	}
	else
	{
		step.x = cell_size_;
		ray_length.x = ((static_cast<float>(map_check.x + cell_size_) - ray_start.x) / cell_size_) * ray_step_size.x;
	}

	if (ray_dir.y < 0)
	{
		step.y = -cell_size_;

		if (static_cast<int>(ray_start.y) % cell_size_ == 0)
		{
			ray_length.y = ray_step_size.y;
		}
		else
		{
			ray_length.y = ((ray_start.y - static_cast<float>(map_check.y)) / cell_size_) * ray_step_size.y;
		}
	}
	else
	{
		step.y = cell_size_;
		ray_length.y = ((static_cast<float>(map_check.y + cell_size_) - ray_start.y) / cell_size_) * ray_step_size.y;
	}

	const SDL_FPoint x_intersection = { ray_start.x + unit_ray_dir.x * ray_length.x, ray_start.y + unit_ray_dir.y * ray_length.x };
	const SDL_FPoint y_intersection = { ray_start.x + unit_ray_dir.x * ray_length.y, ray_start.y + unit_ray_dir.y * ray_length.y };

	if (ray_length.x < ray_length.y)
	{
		map_check.x = x_intersection.x;

		if (ray_dir.x > 0)
		{
			map_check.x -= cell_size_;
		}

		if (static_cast<int>(ray_start.y) % cell_size_ == 0 && ray_dir.y < 0)
		{
			map_check.y -= cell_size_;
		}
	}
	else
	{
		map_check.x = x_intersection.x;

		if (ray_dir.x > 0)
		{
			map_check.x -= cell_size_;
		}

		if (ray_dir.y < 0)
		{
			map_check.y = y_intersection.y;
		}
	}

	bool tile_found = false;
	float max_distance = std::max(constants::screen_width, constants::screen_height) * 10.0f;
	float distance = 0.0f;
	
	while (!tile_found && distance < max_distance)
	{
		if (ray_length.x < ray_length.y)
		{
			map_check.x += step.x;
			distance = ray_length.x;
			ray_length.x += ray_step_size.x;
		}
		else
		{
			map_check.y += step.y;
			distance = ray_length.y;
			ray_length.y += ray_step_size.y;
		}

		const int x_index = (map_check.x / cell_size_);
		const int y_index = (map_check.y / cell_size_);
		const int index = (y_index * cells_width_ + x_index);

		if (index >= 0 && index < static_cast<int>(board_.size()))
		{
			if (board_[index].is_wall_)
			{
				board_[index].highlighted_ = true;
				tile_found = true;
			}
		}
	}

	if (tile_found)
	{
		dda_intersection_ = { ray_start.x + unit_ray_dir.x * distance, ray_start.y + unit_ray_dir.y * distance };
	}
	else
	{
		dda_intersection_ = { -1.0, -1.0 };
	}
}

void Game::Render()
{
	SDL_RenderSetViewport(renderer_, NULL);
	SDL_SetRenderDrawColor(renderer_, 0x00, 0x00, 0x00, 0xff);
	SDL_RenderClear(renderer_);

	RenderGrid();
	RenderCells();

	if (dda_intersection_.x != -1.0f && dda_intersection_.y != -1.0f)
	{
		constexpr float box_size = 10.0f;
		const SDL_FRect collision_box = { dda_intersection_.x - (box_size / 2), dda_intersection_.y - (box_size / 2), box_size, box_size };
		SDL_SetRenderDrawColor(renderer_, 0xff, 0xff, 0xff, 0xff);
		SDL_RenderDrawRectF(renderer_, &collision_box);
	}

	SDL_SetRenderDrawColor(renderer_, 0xff, 0x00, 0x00, 0xff);
	SDL_RenderFillRect(renderer_, &player_.box_);

	SDL_SetRenderDrawColor(renderer_, 0x00, 0xff, 0x00, 0xff);
	SDL_RenderFillRect(renderer_, &mouse_box_);

	if (render_line_)
	{
		SDL_SetRenderDrawColor(renderer_, 0x00, 0xff, 0xff, 0xff);
		SDL_RenderDrawLine(renderer_, player_.box_.x + (player_.box_.w / 2), player_.box_.y + (player_.box_.h / 2), mouse_box_.x + (mouse_box_.w / 2), mouse_box_.y + (mouse_box_.h / 2));
	}

	SDL_RenderPresent(renderer_);
}

void Game::RenderGrid()
{
	SDL_SetRenderDrawColor(renderer_, 0x14, 0x14, 0x14, 0xff);

	for (int y = 1; y < cells_height_; ++y)
	{
		SDL_RenderDrawLine(renderer_, 0, y * cell_size_, constants::screen_width, y * cell_size_);
	}

	for (int x = 1; x < cells_width_; ++x)
	{
		SDL_RenderDrawLine(renderer_, x * cell_size_, 0, x * cell_size_, constants::screen_height);
	}
}

void Game::RenderCells()
{
	for (int i = 0; i < cells_width_ * cells_height_; ++i)
	{
		if (board_[i].is_wall_)
		{
			SDL_SetRenderDrawColor(renderer_, 0x00, 0x00, 0xff, 0xff);
			SDL_RenderFillRect(renderer_, &board_[i].rect_);
		}
	}
}