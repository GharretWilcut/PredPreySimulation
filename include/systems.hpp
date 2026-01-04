#pragma once
#include "ecs.hpp"
#include "components.hpp"
#include <random>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <functional>
#include <map>
#include <SFML/Graphics.hpp>

struct SpeciesParams {
    float speed=1.0f;
    float vision=5.0f;
    float reproduce_chance=0.01f;
    float hunger_rate=0.1f;
    float hunger_max=10.0f;
    float max_age=9999.0f;
    
};

struct Config {
    int width=80, height=40;
    int initial_predators=10, initial_prey=40;
    SpeciesParams prey_params, predator_params;
    float tick_dt=0.2f;
    int max_ticks=10000;
    bool render_ascii=true;
    unsigned seed=12345;
};

inline float dist2(const Position& a, const Position& b) {
    float dx = a.x - b.x, dy = a.y - b.y;
    return dx*dx + dy*dy;
}
inline float clampf(float v, float a, float b) { return std::max(a, std::min(b, v)); }

class MovementSystem {
public:
    void update(World& w, float dt) {
        for (auto [e, pos] : w.all<Position>()) {
            Velocity* v = w.get<Velocity>(e);
            if (!v) continue;
            pos->x += v->dx * dt;
            pos->y += v->dy * dt;
        }
    }
};

class BoundarySystem {
public:
    BoundarySystem(int w, int h) : W(w), H(h) {}
    void clamp(World& world) {
        for (auto [e, pos] : world.all<Position>()) {
            pos->x = clampf(pos->x, 0.0f, (float)W - 1.0f);
            pos->y = clampf(pos->y, 0.0f, (float)H - 1.0f);
        }
    }
private:
    int W,H;
};

class HungerSystem {
public:
    HungerSystem(const Config& cfg) : cfg(cfg) {}
    void update(World& w, float dt) {
        for (auto [e, h] : w.all<Hunger>()) {
            h->value += h->rate * dt;
            Age* age = w.get<Age>(e);
            if (age) age->age += dt;
            // if hunger too high or too old, mark entity by setting health to 0
            Health* health = w.get<Health>(e);
            if (health && (h->value > h->max || (age && age->age > age->max_age))) {
                health->hp = 0.0f;
            }
        }
    }
private:
    const Config& cfg;
};

class PredatorSystem {
public:
    PredatorSystem(const Config& cfg) : cfg(cfg) {}
    void update(World& w, float dt) {
        // For each predator, find nearest prey within vision and move toward it.
        auto preds = w.all<Species>();
        for (auto [e, s] : preds) {
            if (s->type != SpeciesType::Predator) continue;
            Position* pos = w.get<Position>(e);
            Velocity* vel = w.get<Velocity>(e);
            Hunger* hunger = w.get<Hunger>(e);
            if (!pos || !vel) continue;

            // find nearest prey
            float bestd = cfg.predator_params.vision * cfg.predator_params.vision;
            Entity target = 0;
            for (auto [pe, ps] : w.all<Species>()) {
                if (ps->type != SpeciesType::Prey) continue;
                Health* ph = w.get<Health>(pe);
                if (!ph || ph->hp <= 0.0f) continue;
                Position* ppos = w.get<Position>(pe);

                if (!ppos) continue;
                float d2 = dist2(*pos, *ppos);
                if (d2 < bestd) { bestd = d2; target = pe; }
            }

            if (target != 0) {
                Position* tpos = w.get<Position>(target);
                float dx = tpos->x - pos->x;
                float dy = tpos->y - pos->y;
                float len = std::sqrt(dx*dx + dy*dy) + 1e-6f;
                float sp = cfg.predator_params.speed;
                vel->dx = (dx/len) * sp;
                vel->dy = (dy/len) * sp;

                // if close enough "eat" target
                if (len < 1.0f) {
                    // remove prey by setting its health to 0
                    Health* ph = w.get<Health>(target);
                    if (ph) ph->hp = 0.0f;
                    hunger->value = 0.0f;
                }
            } else {
                // wander randomly
                // pick a fresh random direction
                float ang = randf() * 6.283185f; // 0-2π
                float sp  = cfg.predator_params.speed * 0.25f; // slow wander
                vel->dx = cosf(ang) * sp;
                vel->dy = sinf(ang) * sp;
            }
        }
    }

private:
    const Config& cfg;
    static float randf() { return (float)rand() / (float)RAND_MAX; }
};

class PreyAvoidanceSystem {
public:
    PreyAvoidanceSystem(const Config& cfg) : cfg(cfg) {}

    void update(World& w, float dt) {
        for (auto [e, s] : w.all<Species>()) {
            if (s->type != SpeciesType::Prey) continue;

            Position* pos = w.get<Position>(e);
            Velocity* vel = w.get<Velocity>(e);
            Hunger* hunger = w.get<Hunger>(e);
            if (!pos || !vel || !hunger) continue;

            // --------------------------------
            // 1. Flee predator (your code)
            // --------------------------------
            float bestd = cfg.prey_params.vision * cfg.prey_params.vision;
            Entity threat = 0;

            for (auto [pe, ps] : w.all<Species>()) {
                if (ps->type != SpeciesType::Predator) continue;

                Position* ppos = w.get<Position>(pe);
                if (!ppos) continue;

                float d2 = dist2(*pos, *ppos);
                if (d2 < bestd) {
                    bestd = d2;
                    threat = pe;
                }
            }

            if (threat != 0) {
                Position* tpos = w.get<Position>(threat);
                float dx = pos->x - tpos->x;
                float dy = pos->y - tpos->y;
                float len = sqrtf(dx*dx + dy*dy) + 1e-6f;

                float sp = cfg.prey_params.speed;
                vel->dx = (dx / len) * sp;
                vel->dy = (dy / len) * sp;
                continue; // fleeing overrides everything
            }

            // ---------------------------------------------------------------
            // 2. Seek highest-growth grass if hunger is above threshold
            // ---------------------------------------------------------------
            float hunger_ratio = hunger->value / hunger->max;
            const float HUNGER_THRESHOLD = 0.3f;

            if (hunger_ratio > HUNGER_THRESHOLD) {
                Entity best_grass = 0;
                float best_growth = 0.f;

                for (auto [ge, g] : w.all<Grass>()) {
                    Position* gp = w.get<Position>(ge);
                    if (!gp) continue;

                    float d2 = dist2(*pos, *gp);
                    if (d2 < cfg.prey_params.vision * cfg.prey_params.vision) {
                        if (g->growth > best_growth) {
                            best_growth = g->growth;
                            best_grass = ge;
                        }
                    }
                }

                if (best_grass != 0) {
                    Position* gp = w.get<Position>(best_grass);
                    float dx = gp->x - pos->x;
                    float dy = gp->y - pos->y;
                    float len = sqrtf(dx*dx + dy*dy) + 1e-6f;

                    float sp = cfg.prey_params.speed;
                    vel->dx = (dx / len) * sp;
                    vel->dy = (dy / len) * sp;
                    continue; // grass seeking overrides wandering
                }
            }

            // --------------------------------
            // 3. Wander (your existing behavior)
            // --------------------------------
            float ang = randf() * 6.283185f;
            float sp  = cfg.prey_params.speed * 0.25f;
            vel->dx = cosf(ang) * sp;
            vel->dy = sinf(ang) * sp;
        }
    }

private:
    const Config& cfg;

    static float randf() { return (float)rand() / RAND_MAX; }
};

class ReproductionSystem {
public:
    ReproductionSystem(const Config& cfg) : cfg(cfg) {}
    void update(World& w, float dt) {
        std::vector<std::function<void()>> births;
        for (auto [e, s] : w.all<Species>()) {
            Position* pos = w.get<Position>(e);
            Hunger* h = w.get<Hunger>(e);
            Age* age = w.get<Age>(e);
            if (!pos || !h || !age) continue;

            float chance = (s->type == SpeciesType::Prey) ? cfg.prey_params.reproduce_chance : cfg.predator_params.reproduce_chance;
            if ((randf() < chance) && (h->value < h->max * 0.7f)) {
                // schedule birth (defer to avoid modifying containers while iterating)
                births.push_back([=,&w]() {
                    Entity n = w.create_entity();
                    w.add<Species>(n, Species{ s->type });
                    w.add<Position>(n, Position{ pos->x + (randf()-0.5f)*2.0f, pos->y + (randf()-0.5f)*2.0f });
                    w.add<Velocity>(n, Velocity{0,0});
                    w.add<Health>(n, Health{10.0f, 10.0f});
                    Hunger hun = *h;
                    hun.value = hun.value * 0.5f;
                    w.add<Hunger>(n, hun);
                    Age a; a.age = 0.0f; a.max_age = (s->type==SpeciesType::Prey ? cfg.prey_params.max_age : cfg.predator_params.max_age);
                    w.add<Age>(n, a);
                });
            }
        }
        for (auto &f : births) f();
    }
private:
    const Config& cfg;
    static float randf() { return (float)rand() / (float)RAND_MAX; }
};

class CleanupSystem {
public:
    void update(World& w) {
        // remove entities with health <= 0
        std::vector<Entity> dead;
        for (auto [e, h] : w.all<Health>()) {
            if (h->hp <= 0.0f) dead.push_back(e);
        }
        for (Entity e : dead) {
            // remove from all component stores by attempting removals
            w.remove<Position>(e); w.remove<Velocity>(e); w.remove<Species>(e);
            w.remove<Health>(e); w.remove<Hunger>(e); w.remove<Age>(e);
        }
    }
};

class SimpleAsciiRenderer {
public:
    SimpleAsciiRenderer(int w, int h) : W(w), H(h) {}
    void render(World& world) {
        std::vector<std::string> grid(H, std::string(W, '.'));
        for (auto [e, s] : world.all<Species>()) {
            Position* p = world.get<Position>(e);
            if (!p) continue;
            int x = (int)std::round(p->x);
            int y = (int)std::round(p->y);
            x = std::max(0, std::min(W-1, x));
            y = std::max(0, std::min(H-1, y));
            grid[y][x] = (s->type == SpeciesType::Predator ? 'P' : 'p');
        }
        // clear console (simple)
        #if defined(_WIN32)
            system("cls");
        #else
            std::cout << "\x1B[2J\x1B[H";
        #endif
        for (auto &row : grid) std::cout << row << "\n";
    }
private:
    int W,H;
};


class SfmlRenderer {
public:
    SfmlRenderer(int w, int h) : W(w), H(h), window(sf::VideoMode(w, h), "Ecosystem Simulation") {
        window.setFramerateLimit(60);
        
        predatorShape.setRadius(4);
        predatorShape.setFillColor(sf::Color::Red);
        preyShape.setRadius(3);
        preyShape.setFillColor(sf::Color::Green);
    }
    
    bool isOpen() const { return window.isOpen(); }

    void handleEvents(){
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
        }
    }

    void render (World& world) {
        window.clear(sf::Color::Black);

        for (auto [e,s] : world.all<Species>()){
            Position* p = world.get<Position>(e);
            if (!p) continue;

            float x = p->x;
            float y = p->y;

            x = std::max(0.f, std::min((float)W-1, x));
            y = std::max(0.f, std::min((float)H-1, y));

            if (s->type == SpeciesType::Predator){
                predatorShape.setPosition(x,y);
                window.draw(predatorShape);
            }
            else{
                preyShape.setPosition(x,y);
                window.draw(preyShape);
            }
        }
    
        window.display();
    }

private:

    int W,H;
    sf::RenderWindow window;
    
    sf::CircleShape predatorShape;
    sf::CircleShape preyShape;

};

class GrassGrowthSystem {
public:
    GrassGrowthSystem(const Config& cfg) : cfg(cfg) {}

    void update(World& w, float dt) {
        for (auto [e, g] : w.all<Grass>()) {
            g->growth += cfg.grass_params.growth_rate * dt;
            if (g->growth > cfg.grass_params.max_growth)
                g->growth = cfg.grass_params.max_growth;
        }
    }
private:
    const Config& cfg;
};


class GrassConsumptionSystem {
public:
    GrassConsumptionSystem(const Config& cfg) : cfg(cfg) {}

    void update(World& w, float dt) {
        for (auto [e, s] : w.all<Species>()) {
            if (s->type != SpeciesType::Prey) continue;

            Position* pos = w.get<Position>(e);
            Hunger* hunger = w.get<Hunger>(e);
            if (!pos || !hunger) continue;

            for (auto [ge, g] : w.all<Grass>()) {
                Position* gp = w.get<Position>(ge);
                if (!gp) continue;

                // same tile or very close
                if (dist2(*pos, *gp) < 1.0f) {
                    float eat = std::min(g->growth, 2.0f);
                    g->growth -= eat;
                    hunger->value -= eat * 3.0f;
                    if (hunger->value < 0) hunger->value = 0.0f;
                }
            }
        }
    }

private:
    const Config& cfg;
};
