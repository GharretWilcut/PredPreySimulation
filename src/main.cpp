#include <iostream>
#include <fstream>
#include <sstream>
#include <random>
#include <chrono>
#include "ecs.hpp"
#include "components.hpp"
#include "systems.hpp"
#include <thread>
#include <nlohmann/json.hpp>

// Minimal nlohmann/json single header inclusion (tiny shim).
// For this starter, we'll include a minimal JSON parser -- but in real projects, add nlohmann/json as a dependency.
// To keep this example self-contained, we'll parse a very small subset using std::stringstream. For real use, replace with a proper JSON library.
#include <string>
#include <unordered_map>

// --- simple, naive JSON replacement for this demo: parse only the fields we need ---
#include <regex>

using json = nlohmann::json;
static std::string read_file(const std::string &path) {
    std::ifstream in(path);
    if (!in) return {};
    std::ostringstream ss; ss << in.rdbuf();
    return ss.str();
}

static std::string extract(const std::string &s, const std::string &key) {
    std::regex r("\"" + key + "\"\\s*:\\s*([\\d\\.truefalsenull\"\\{\\[][^,\\}\\]]*)", std::regex::icase);
    std::smatch m;
    if (std::regex_search(s, m, r)) return m[1].str();
    return "";
}

// a very small helper to find numbers by key
static int extract_int(const std::string &s, const std::string &key, int def) {
    auto t = extract(s,key);
    if (t.empty()) return def;
    std::smatch m;
    std::regex r("([0-9]+)");
    if (std::regex_search(t,m,r)) return std::stoi(m[1].str());
    return def;
}
static float extract_float(const std::string &s, const std::string &key, float def) {
    auto t = extract(s,key);
    if (t.empty()) return def;
    std::smatch m;
    std::regex r("([0-9]+\\.?[0-9]*)");
    if (std::regex_search(t,m,r)) return std::stof(m[1].str());
    return def;
}
static bool extract_bool(const std::string &s, const std::string &key, bool def) {
    auto t = extract(s,key);
    if (t.empty()) return def;
    if (t.find("true") != std::string::npos) return true;
    if (t.find("false") != std::string::npos) return false;
    return def;
}

int main(int argc, char** argv) {
    // load config.json
    
    std::string cfgs = read_file("config.json");
    Config cfg;
    if (cfgs.empty()) {
        std::cout << "No config.json found, using defaults.\n";
    } else {
        json j = json::parse(cfgs);
        std::cout << (j);

        //get sim parameters
        auto js = j["sim"];
        cfg.tick_dt = js["tick_dt"];
        cfg.max_ticks = js["max_ticks"];
        cfg.render_ascii = js["render_ascii"];
        cfg.seed = js["seed"];
        std::cout << ("sim loaded");
        
        //get world parameters
        auto jw = j["world"];
        cfg.initial_prey = jw["initial_prey"];
        cfg.width = jw["width"];
        cfg.height = jw["height"];
        cfg.initial_predators = jw["initial_predators"];
        std::cout << ("world loaded");

        //get predator parameters
        auto jX = j["species"]["predator"];
        cfg.predator_params.speed = jX["speed"];     
        cfg.predator_params.vision = jX["vision"];  
        cfg.predator_params.reproduce_chance = jX["reproduce_chance"];   
        cfg.predator_params.hunger_max = jX["hunger_max"];    
        cfg.predator_params.hunger_rate = jX["hunger_rate"];  
        cfg.predator_params.max_age = jX["max_age"];          
        std::cout << ("predator loaded");

        //get prey parameters
        auto jo = j["species"]["prey"];
        cfg.prey_params.speed = jo["speed"];  
        cfg.prey_params.vision = jo["vision"];  
        cfg.prey_params.reproduce_chance = jo["reproduce_chance"];   
        cfg.prey_params.hunger_max = jo["hunger_max"];    
        cfg.prey_params.hunger_rate = jo["hunger_rate"];  
        cfg.prey_params.max_age = jo["max_age"];         
        std::cout << ("prey loaded");    
    }

    srand(cfg.seed);
    World world;

    // initialize systems
    MovementSystem movement;
    BoundarySystem boundary(cfg.width, cfg.height);
    HungerSystem hunger(cfg);
    PredatorSystem predator(cfg);
    PreyAvoidanceSystem preyAvoid(cfg);
    ReproductionSystem reproduce(cfg);
    CleanupSystem cleanup;
    SimpleAsciiRenderer ascii(cfg.width, cfg.height);
    SfmlRenderer sfml(cfg.width, cfg.height);

    // spawn initial entities
    auto spawn = [&](SpeciesType stype, int count){
        for (int i=0;i<count;++i){
            Entity e = world.create_entity();
            float x = (float)(rand() % cfg.width);
            float y = (float)(rand() % cfg.height);
            world.add<Position>(e, Position{ x, y });
            world.add<Velocity>(e, Velocity{ 0.0f, 0.0f });
            world.add<Species>(e, Species{ stype });
            world.add<Health>(e, Health{ 10.0f, 10.0f });
            Hunger h; h.value = 0.0f; 
            h.rate = (stype==SpeciesType::Prey?cfg.prey_params.hunger_rate:cfg.predator_params.hunger_rate);
            h.max = (stype==SpeciesType::Prey?cfg.prey_params.hunger_max:cfg.predator_params.hunger_max);
            world.add<Hunger>(e, h);
            Age a; a.age = 0.0f; a.max_age = (stype==SpeciesType::Prey?cfg.prey_params.max_age:cfg.predator_params.max_age);
            world.add<Age>(e, a);
        }
    };

    spawn(SpeciesType::Prey, cfg.initial_prey);
    spawn(SpeciesType::Predator, cfg.initial_predators);

    // main loop
    for (int tick=0; tick < cfg.max_ticks; ++tick) {
        float dt = cfg.tick_dt;

        hunger.update(world, dt);
        predator.update(world, dt);
        preyAvoid.update(world, dt);
        movement.update(world, dt);
        boundary.clamp(world);
        reproduce.update(world, dt);
        cleanup.update(world);

        if (cfg.render_ascii) {
            ascii.render(world);
            std::cout << "Tick: " << tick << "  (Predators = ";
            int np=0, no=0;
            for (auto [e,s] : world.all<Species>())
                (s->type == SpeciesType::Predator ? ++np : ++no);
            std::cout << np << " Prey = " << no << ")\n";
        } else {
            if (!sfml.isOpen()) break;
            sfml.handleEvents();
            sfml.render(world);
        }


        // tiny sleep to make console readable
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
    }

    std::cout << "Simulation finished.\n";
    return 0;
}
