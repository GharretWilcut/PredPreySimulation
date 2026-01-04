#pragma once
#include <string>

enum class SpeciesType { Prey, Predator };

struct Position { float x, y; };
struct Velocity { float dx=0, dy=0; };
struct Species { SpeciesType type; };
struct Health { float hp; float max_hp; };
struct Hunger { float value = 0.0f; float rate = 0.1f; float max = 10.0f; };
struct Age { float age = 0.0f; float max_age = 9999.0f; };
struct WanderTimer {
    float t = 0.0f;          // current timer
    float interval = 1.0f;   // how often an animal chooses a new direction
};

struct Grass {
    float growth = 10.0f;
    float max_growth = 20.0f;
    float rate = 1.0f;       // growth per second
};


enum class Faction { Neutral, Herbivore, Carnivore, Mutant, Military, Stalker };
struct FactionTag { Faction f; };
