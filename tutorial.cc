#include <sc2api/sc2_api.h>
#include "sc2lib/sc2_lib.h"
#include <iterator>

#include <iostream>

using namespace sc2;

class Bot : public Agent {
public:

    virtual void OnGameStart() final {
        std::cout << "Game Started." << std::endl;
        
    }

    virtual void OnStep() final {
        TryBuildSupplyDepot();
        TryBuildVespeneGas();
        TryBuildBarracks();
        TryBuildFactory();
        TryBuildStarPort();
        //TryExpand(ABILITY_ID::BUILD_COMMANDCENTER, UNIT_TYPEID::TERRAN_SCV);
        TryBuildEngineerBay();
        TryBuildArmory();
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
            
            Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_MARINE);
            break;
        }
        case UNIT_TYPEID::TERRAN_MARINE: {
            float rx = GetRandomScalar();
            float ry = GetRandomScalar();

            const GameInfo& game_info = Observation()->GetGameInfo();
            Actions()->UnitCommand(unit, ABILITY_ID::MOVE, Point2D(rx * 15.0f,ry * 15.0f));
            if (CountUnitType(UNIT_TYPEID::TERRAN_MARINE) > 20) {
                Actions()->UnitCommand(unit, ABILITY_ID::ATTACK_ATTACK, game_info.enemy_start_locations.front());
            }
            break;
        }
        case UNIT_TYPEID::TERRAN_STARPORT: {

            Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_MEDIVAC);
            break;
        }
        case UNIT_TYPEID::TERRAN_FACTORY: {

            //Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_WIDOWMINE);
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

      //  if (ability_type_for_structure == ABILITY_ID::BUILD_COMMANDCENTER){
         //   Point3D close_exp = FindClosestExpansion(ABILITY_ID::BUILD_COMMANDCENTER);
            
         //   Actions()->UnitCommand(unit_to_build, ability_type_for_structure, Point2D(close_exp));
           // return true;
       // }
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
    const ObservationInterface* observation = Observation();
    std::vector<Point3D> expansions_ = search::CalculateExpansionLocations(Observation(), Query());
    Point3D startLocation_ = Observation()->GetStartLocation();
    bool TryExpand(AbilityID build_ability, UnitTypeID worker_type) {
        
        //Point3D staging_location_ = startLocation_;
        const ObservationInterface* observation = Observation();
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
        if (CountUnitType(UNIT_TYPEID::TERRAN_FACTORY) > 1) {
            return false;
        }
        return TryBuildStructure(ABILITY_ID::BUILD_FACTORY);
    }
    bool TryBuildStarPort() {
        if (CountUnitType(UNIT_TYPEID::TERRAN_STARPORT) > 1) {
            return false;
        }
        return TryBuildStructure(ABILITY_ID::BUILD_STARPORT);
    }
    bool TryBuildEngineerBay() {
        if (CountUnitType(UNIT_TYPEID::TERRAN_ENGINEERINGBAY) > 1) {
            return false;
        }
        return TryBuildStructure(ABILITY_ID::BUILD_ENGINEERINGBAY);
    }
    bool TryBuildArmory() {
        if (CountUnitType(UNIT_TYPEID::TERRAN_ARMORY) > 1) {
            return false;
        }
        return TryBuildStructure(ABILITY_ID::BUILD_ARMORY);
    }
    bool TryBuildCC() {
        return TryBuildStructure(ABILITY_ID::BUILD_COMMANDCENTER);
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
