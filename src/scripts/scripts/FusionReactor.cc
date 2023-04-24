#include <algorithm>
#include <iostream>

int main() {
    const double zeroCelsius = 273.15; // Zero Celsius in Kelvin
    const double backupPower = 10.0; // Minimum power available to the reactor (in kW)
    const double ambientTemp = zeroCelsius + 20.0; // Ambient temperature of the reactor (in K)
    const double maxReactorTemp = zeroCelsius + 10000.0; // Maximum temperature of the reactor (in K)
    const double tempIncreasePerPellet = 8.0; // Temperature increase per pellet (in K)
    const double powerOutputPerPellet = 1000.0; // Power output per pellet (in kW)
    const double pelletReactivity = 300000.0; // A higher number means pellets require more energy to react

    // Reactor parameters:
    double temperature = ambientTemp; // Reactor temperature
    double powerOutput = 0.0; // Reactor power output (in kW)
    double recoveryRate = 1.0; // Initial recovery rate (feed back 100% of output power)

    // Player controls:
    double deuteriumMix;
    double tritiumMix;
    double pelletDropRate;

    // D-T mixture for startup (1:1 ratio)
    deuteriumMix = 1.0;
    tritiumMix = 1.0;
    pelletDropRate = 3.0; // Initial pellet drop rate

    // Simulate reactor operation
    int i = 0;
    while (true) {
        if (i > 1000) break;
        double pelletReactionRate = temperature / pelletDropRate;
        if (deuteriumMix > tritiumMix) {
            double totalFuel = deuteriumMix + tritiumMix;
            double dtFraction = tritiumMix / totalFuel;
            double ddFraction = 0.5 * (deuteriumMix - tritiumMix) / totalFuel;
            // Assume D-D fusion requires 4x the input energy of D-T fusion
            pelletReactionRate *= dtFraction + ddFraction * 0.25;
        }
        double requiredLaserPower = std::max(pelletReactivity / pelletReactionRate, 0.0);
        double laserPulsePower = backupPower;
        if (powerOutput > 0) {
            recoveryRate = std::clamp(requiredLaserPower / powerOutput, 0.0, 1.0);
            laserPulsePower = std::max(backupPower, powerOutput * recoveryRate);
        }
        pelletReactionRate = std::clamp(pelletReactionRate * laserPulsePower, 0.0, pelletReactivity) / pelletReactivity;

        temperature += pelletReactionRate * deuteriumMix * tempIncreasePerPellet * pelletDropRate;
        temperature = temperature * 0.995 + ambientTemp * 0.005; // Reactor cooling (0.5% per iteration)
        powerOutput = pelletReactionRate * deuteriumMix * powerOutputPerPellet * pelletDropRate;

        // Output reactor status
        std::cout << "Iteration " << i + 1 << ": Temperature = " << std::round(temperature - zeroCelsius)
                  << " C, Fuel Mix = " << std::round(deuteriumMix / (deuteriumMix + tritiumMix) * 1000.0) / 10.0
                  << "%, Power Output = " << (powerOutput * (1.0 - recoveryRate) / 1000.0)
                  << " MW, LaserPower Required = " << requiredLaserPower / 1000.0
                  << " MW, LaserPower Used = " << laserPulsePower / 1000.0
                  << " MW, RecoveryRate = " << std::round(recoveryRate * 1000.0) / 10.0
                  << "%, Reaction Rate = " << std::round(pelletReactionRate * 1000.0) / 10.0 << "%" << std::endl;

        // Adjust deuterium and tritium mixtures (this is what the player does)
        if (temperature > 500.0 + zeroCelsius) {
            tritiumMix = 0.0;
        } else {
            tritiumMix = 1.0;
        }
        tritiumMix = std::clamp(tritiumMix, 0.0, 1.0);
        deuteriumMix = 2.0 - tritiumMix;
        i++;
    }

    return 0;
}
