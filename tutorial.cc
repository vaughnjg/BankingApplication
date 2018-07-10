#include <sc2api/sc2_api.h>
#include "sc2lib/sc2_lib.h"
#include <iterator>
#include <iostream>

using namespace sc2;

struct IsStructure {
    IsStructure(const ObservationInterface* obs) : observation_(obs) {};

    bool operator()(const Unit& unit) {
        auto& attributes = observation_->GetUnitTypeData().at(unit.unit_type).attributes;
        bool is_structure = false;
        for (const auto& attribute : attributes) {
            if (attribute == Attribute::Structure) {
                is_structure = true;
            }
        }
        return is_structure;
    }

    const ObservationInterface* observation_;
};

class Bot : public Agent {
public:
    const ObservationInterface* observation = Observation();
    std::vector<Point3D> expansions_;
    Point3D startLocation_;
    Point3D staging_location_;

    virtual void OnGameStart() final {
        std::cout << "Game Started." << std::endl;
        expansions_ = search::CalculateExpansionLocations(Observation(), Query());
        startLocation_ = Observation()->GetStartLocation();
        staging_location_ = startLocation_;
    }

    virtual void OnStep() final {
        TryBuildSupplyDepot();
        TryBuildVespeneGas();
        TryBuildBarracks();
        TryAddOn();
        TryBuildFactory();
        TryBuildStarPort();
        TryExpand(ABILITY_ID::BUILD_COMMANDCENTER, UNIT_TYPEID::TERRAN_SCV);
        TryBuildEngineerBay();
        TryBuildArmory();
        //TryBuildRaxReact();
        delegateWorkers(UNIT_TYPEID::TERRAN_SCV, ABILITY_ID::HARVEST_GATHER_SCV, UNIT_TYPEID::TERRAN_REFINERY);
    }

    virtual void OnUnitIdle(const Unit* unit) final {
        switch (unit->unit_type.ToType()) {
        case UNIT_TYPEID::TERRAN_COMMANDCENTER: {
            if (CountUnitType(UNIT_TYPEID::TERRAN_SCV) < 22) {
                Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_SCV);
            }    
            break;
        }
        case UNIT_TYPEID::TERRAN_SCV: {
            const Unit* mineral_target = FindNearestMineralPatch(unit->pos);
            
            if (!mineral_target) {
                break;
            }
            
            Actions()->UnitCommand(unit, ABILITY_ID::SMART, mineral_target);
            
            break;
        }
        case UNIT_TYPEID::TERRAN_BARRACKS: {
            TryAddOn();
            Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_MARINE);
            break;
        }
        case UNIT_TYPEID::TERRAN_MARINE: {
            float rx = GetRandomScalar();
            float ry = GetRandomScalar();

            const GameInfo& game_info = Observation()->GetGameInfo();
            Actions()->UnitCommand(unit, ABILITY_ID::ATTACK, staging_location_);
            if (CountUnitType(UNIT_TYPEID::TERRAN_MARINE) > 50) {
                Actions()->UnitCommand(unit, ABILITY_ID::ATTACK_ATTACK, game_info.enemy_start_locations.front());
            }
            break;
        }
        case UNIT_TYPEID::TERRAN_STARPORT: {
            if (CountUnitType(UNIT_TYPEID::TERRAN_MEDIVAC) >= 5) {
                break;
            }
            TryAddOn();
            Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_MEDIVAC);
            break;
        }
        case UNIT_TYPEID::TERRAN_MEDIVAC: {
            Actions()->UnitCommand(unit, ABILITY_ID::MOVE, staging_location_);
            break;
        }
        case UNIT_TYPEID::TERRAN_FACTORY: {
            if (CountUnitType(UNIT_TYPEID::TERRAN_SIEGETANK) >= 5) {
                break;
            }
            TryAddOn();
            Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_SIEGETANK);
            break;
        }
        case UNIT_TYPEID::TERRAN_SIEGETANK: {
            Actions()->UnitCommand(unit, ABILITY_ID::ATTACK, staging_location_);
            break;
        }
        default: {
            break;
        }
        }
    }

private:

    size_t CountUnitType(UNIT_TYPEID unit_type) {
        return Observation()->GetUnits(Unit::Alliance::Self, IsUnit(unit_type)).size();
    }
    void TryAddOn() {
        const ObservationInterface* observation = Observation();

        Units bases = observation->GetUnits(Unit::Self, IsUnit(UNIT_TYPEID::TERRAN_COMMANDCENTER));
        Units barracks = observation->GetUnits(Unit::Self, IsUnit(UNIT_TYPEID::TERRAN_BARRACKS));
        Units factorys = observation->GetUnits(Unit::Self, IsUnit(UNIT_TYPEID::TERRAN_FACTORY));
        Units starports = observation->GetUnits(Unit::Self, IsUnit(UNIT_TYPEID::TERRAN_STARPORT));
        Units raxTech = observation->GetUnits(Unit::Self, IsUnit(UNIT_TYPEID::TERRAN_BARRACKSTECHLAB));
        Units portTech = observation->GetUnits(Unit::Self, IsUnit(UNIT_TYPEID::TERRAN_STARPORTTECHLAB));
        Units factTech = observation->GetUnits(Unit::Self, IsUnit(UNIT_TYPEID::TERRAN_FACTORYTECHLAB));
        Units supply_depots = observation->GetUnits(Unit::Self, IsUnit(UNIT_TYPEID::TERRAN_SUPPLYDEPOT));

        for (const auto& supply_depot : supply_depots) {
            Actions()->UnitCommand(supply_depot, ABILITY_ID::MORPH_SUPPLYDEPOT_LOWER);
        }

        for (const auto& barrack : barracks) {
            if (!barrack->orders.empty() || barrack->build_progress != 1) {
                continue;
            }

            if (observation->GetUnit(barrack->add_on_tag) == nullptr) {
                if (raxTech.empty()) {
                    TryBuildRaxTech(ABILITY_ID::BUILD_TECHLAB_BARRACKS, barrack->tag);
                }
                else {
                    TryBuildRaxTech(ABILITY_ID::BUILD_REACTOR_BARRACKS, barrack->tag);
                }
            }
        }
        for (const auto& factory : factorys) {
            if (!factory->orders.empty() || factory->build_progress != 1) {
                continue;
            }

            if (observation->GetUnit(factory->add_on_tag) == nullptr) {
                if (factTech.empty()) {
                    TryBuildRaxTech(ABILITY_ID::BUILD_TECHLAB_FACTORY, factory->tag);
                }
                else {
                    TryBuildRaxTech(ABILITY_ID::BUILD_REACTOR_FACTORY, factory->tag);
                }
            }
        }
        for (const auto& starport : starports) {
            if (!starport->orders.empty() || starport->build_progress != 1) {
                continue;
            }

            if (observation->GetUnit(starport->add_on_tag) == nullptr) {
                if (raxTech.empty()) {
                    TryBuildRaxTech(ABILITY_ID::BUILD_TECHLAB_STARPORT, starport->tag);
                }
                else {
                    TryBuildRaxTech(ABILITY_ID::BUILD_REACTOR_STARPORT, starport->tag);
                }
            }
        }
    }
    bool TryBuildStructure(AbilityID ability_type_for_structure, UnitTypeID unit_type, Point2D location) {
        const ObservationInterface* observation = Observation();

        const Unit* unit_to_build = nullptr;
        Units units = observation->GetUnits(Unit::Alliance::Self);

        for (const auto& unit : units) {
            for (const auto& order : unit->orders) {
                if (order.ability_id == ability_type_for_structure) {
                    return false;
                }
            }

            if (unit->unit_type == unit_type) {
                unit_to_build = unit;
            }
        }

        

        Actions()->UnitCommand(unit_to_build, ability_type_for_structure, location);
        return true;


    }
    bool TryBuildStructure(ABILITY_ID ability_type_for_structure, UNIT_TYPEID unit_type = UNIT_TYPEID::TERRAN_SCV) {
        const ObservationInterface* observation = Observation();

        const Unit* unit_to_build = nullptr;
        Units units = observation->GetUnits(Unit::Alliance::Self);

        for (const auto& unit : units) {
            for (const auto& order : unit->orders) {
                if (order.ability_id == ability_type_for_structure) {
                    return false;
                }
            }

            if (unit->unit_type == unit_type) {
                unit_to_build = unit;
            }
        }


        if (ability_type_for_structure == ABILITY_ID::BUILD_REFINERY)
        {
           const Unit* vespene_target = FindNearestVespeneGas(unit_to_build->pos);
            Actions()->UnitCommand(unit_to_build, ability_type_for_structure, vespene_target);
            return true;
        }

        //if (ability_type_for_structure == ABILITY_ID::BUILD_COMMANDCENTER){
            //Point3D close_exp = FindClosestExpansion(ABILITY_ID::BUILD_COMMANDCENTER);
            
          //  Actions()->UnitCommand(unit_to_build, ability_type_for_structure, Point2D(close_exp));
            //return true;
     //   }
        float rx = GetRandomScalar();
        float ry = GetRandomScalar();

        Actions()->UnitCommand(unit_to_build,
            ability_type_for_structure,
            Point2D(unit_to_build->pos.x + rx * 15.0f, unit_to_build->pos.y + ry * 15.0f));

        return true;
    }

    bool TryBuildSupplyDepot() {
        const ObservationInterface* observation = Observation();

        if (observation->GetFoodCap()  == 200)
            return false;

        // Try and build a depot. Find a random SCV and give it the order.
        return TryBuildStructure(ABILITY_ID::BUILD_SUPPLYDEPOT);
    }
    bool TryBuildVespeneGas() {

        if (CountUnitType(UNIT_TYPEID::TERRAN_REFINERY) > 1) {
            return false;
        }

        return TryBuildStructure(ABILITY_ID::BUILD_REFINERY);
    }
    const Unit* FindNearestVespeneGas(const Point2D& start) {
        Units units = Observation()->GetUnits(Unit::Alliance::Neutral);
        float distance = std::numeric_limits<float>::max();
        const Unit* target = nullptr;
        for (const auto& u : units) {
            if (u->unit_type == UNIT_TYPEID::NEUTRAL_VESPENEGEYSER) {
                float d = DistanceSquared2D(u->pos, start);
                if (d < distance) {
                    distance = d;
                    target = u;
                }
            }
        }
        return target;
    }

    const Unit* FindNearestMineralPatch(const Point2D& start) {
        Units units = Observation()->GetUnits(Unit::Alliance::Neutral);
        float distance = std::numeric_limits<float>::max();
        const Unit* target = nullptr;
        for (const auto& u : units) {
            if (u->unit_type == UNIT_TYPEID::NEUTRAL_MINERALFIELD) {
                float d = DistanceSquared2D(u->pos, start);
                if (d < distance) {
                    distance = d;
                    target = u;
                }
            }
        }
        return target;
    }

    bool TryExpand(AbilityID build_ability, UnitTypeID worker_type) {
        const ObservationInterface* observation = Observation();
        Units bases = observation->GetUnits(Unit::Self, IsUnit(UNIT_TYPEID::TERRAN_COMMANDCENTER));
        if (bases.size() >= 2) {
            return false;
        }
        //Point3D staging_location_ = startLocation_;
        //const ObservationInterface* observation = Observation();
        //std::vector<Point3D> expansions_ = search::CalculateExpansionLocations(Observation(), Query());
      //  Point3D startLocation_ = Observation()->GetStartLocation();
        float min_distance = std::numeric_limits<float>::max();
        
        Point3D closest_expansion;
        for (const auto& expansion : expansions_) {
            float current_distance = Distance2D(startLocation_, expansion);
            if (current_distance < .01f) {
                continue;
            }

            if (current_distance < min_distance) {
                if (Query()->Placement(build_ability, expansion)) {
                    closest_expansion = expansion;
                    min_distance = current_distance;
                }
            }
        }
        staging_location_ = closest_expansion;
        return TryBuildStructure(build_ability, worker_type, closest_expansion);
    }

    bool TryBuildBarracks() {

        if (CountUnitType(UNIT_TYPEID::TERRAN_SUPPLYDEPOT) < 1) {
            return false;
        }

        if (CountUnitType(UNIT_TYPEID::TERRAN_BARRACKS) > 2) {
            return false;
        }

        return TryBuildStructure(ABILITY_ID::BUILD_BARRACKS);
    }

    bool TryBuildFactory() {
        if (CountUnitType(UNIT_TYPEID::TERRAN_FACTORY) >= 1) {
            return false;
        }
        return TryBuildStructure(ABILITY_ID::BUILD_FACTORY);
    }
    bool TryBuildStarPort() {
        if (CountUnitType(UNIT_TYPEID::TERRAN_STARPORT) >= 1) {
            return false;
        }
        return TryBuildStructure(ABILITY_ID::BUILD_STARPORT);
    }
    bool TryBuildEngineerBay() {
        if (CountUnitType(UNIT_TYPEID::TERRAN_ENGINEERINGBAY) >= 1) {
            return false;
        }
        return TryBuildStructure(ABILITY_ID::BUILD_ENGINEERINGBAY);
    }
    bool TryBuildArmory() {
        if (CountUnitType(UNIT_TYPEID::TERRAN_ARMORY) >= 1) {
            return false;
        }
        return TryBuildStructure(ABILITY_ID::BUILD_ARMORY);
    }
    bool TryBuildCC() {
        return TryBuildStructure(ABILITY_ID::BUILD_COMMANDCENTER);
    }
    bool TryBuildRaxReact() {
        return TryBuildStructure(ABILITY_ID::BUILD_REACTOR_BARRACKS);
    }
    bool TryBuildRaxTech(ABILITY_ID ability_type_for_structure, Tag base_structure) {

        float rx = GetRandomScalar();
        float ry = GetRandomScalar();
        const Unit* unit = Observation()->GetUnit(base_structure);

        if (unit->build_progress != 1) {
            return false;
        }

        Point2D build_location = Point2D(unit->pos.x + rx * 15, unit->pos.y + ry * 15);

        Units units = Observation()->GetUnits(Unit::Self, IsStructure(Observation()));

        if (Query()->Placement(ability_type_for_structure, unit->pos, unit)) {
            Actions()->UnitCommand(unit, ability_type_for_structure);
            return true;
        }

        float distance = std::numeric_limits<float>::max();
        for (const auto& u : units) {
            float d = Distance2D(u->pos, build_location);
            if (d < distance) {
                distance = d;
            }
        }
        if (distance < 6) {
            return false;
        }
        if (Query()->Placement(ability_type_for_structure, build_location, unit)) {
            Actions()->UnitCommand(unit, ability_type_for_structure, build_location);
            return true;
        }
        return false;
    }
    bool TryBuildPortReact() {
        return TryBuildStructure(ABILITY_ID::BUILD_REACTOR_STARPORT);
    }
    bool TryBuildPortTech() {
        return TryBuildStructure(ABILITY_ID::BUILD_TECHLAB_STARPORT);
    }
    bool TryBuildFactReact() {
        return TryBuildStructure(ABILITY_ID::BUILD_REACTOR_FACTORY);
    }
    bool TryBuildFactTech() {
        return TryBuildStructure(ABILITY_ID::BUILD_TECHLAB_FACTORY);
    }

    void delegateWorkers(UNIT_TYPEID worker_type, ABILITY_ID command, UNIT_TYPEID refinery) {
        const ObservationInterface* observation = Observation();
        Units bases = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_COMMANDCENTER));
        Units geysers = observation->GetUnits(Unit::Alliance::Self, IsUnit(refinery));
        Units workers = observation->GetUnits(Unit::Alliance::Self, IsUnit(worker_type));
        const Unit* closest_mineral_patch = nullptr;
        if (bases.empty()) {
            return;
        }

        for (const auto& base : bases) {
            if (base->ideal_harvesters == 0 || base->build_progress != 1) {
                continue;
            }

            for (const auto& geyser : geysers) {
                if (geyser->assigned_harvesters < geyser->ideal_harvesters) {

                    for (const auto& worker : workers) {
                        if (!worker->orders.empty()) {
                            if (worker->orders.front().target_unit_tag == base->tag)
                            {
                                Actions()->UnitCommand(worker, command, geyser);
                                return;
                            }
                        }
                    }

                }

                if (geyser->assigned_harvesters > geyser->ideal_harvesters) {
                    closest_mineral_patch = FindNearestMineralPatch(base->pos);
                    for (const auto& worker : workers) {
                        if (!worker->orders.empty()) {
                            if (worker->orders.front().target_unit_tag == geyser->tag)
                            {
                                Actions()->UnitCommand(worker, command, closest_mineral_patch);
                                return;
                            }
                        }
                    }
                }
            }
        }
    }
};

int main(int argc, char* argv[]) {
    Coordinator coordinator;
    coordinator.LoadSettings(argc, argv);

    Bot bot;
    coordinator.SetParticipants({
        CreateParticipant(Race::Terran, &bot),
        CreateComputer(Race::Zerg)
        });

    coordinator.LaunchStarcraft();
    coordinator.StartGame(sc2::kMapBelShirVestigeLE);

    while (coordinator.Update()) {
    }

    return 0;
}
