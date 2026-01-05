// Main.cpp of riskEngine

#include <iostream>
#include <stdexcept>
#include "../include/marketSimulator.hh"

int main() {

    try {
        MarketSimulator Market;
        Portfolio Retirement(20000, "Grandfather", "Euros");
        Portfolio Savings(50000, "Sons", "Euros");
        Portfolio Aggressive(5000, "Nephew", "Euros");
        Market.add_to_assets(&Retirement);
        Market.add_to_assets(&Savings);
        Market.add_to_assets(&Aggressive);

        Retirement.update_global_vol(-0.15);

        while (Market.get_day_count() < 365) {
            Market.do_simulate_daily();
        }
        std::cout << "Retirements Portfolio value: " << Retirement.get_val()
                << std::endl;
        std::cout << " Savings Portfolio value: " << Savings.get_val()
                << std::endl;
        std::cout << " Aggressive Portfolio value: " << Aggressive.get_val()
                << std::endl;
    } catch (std::runtime_error e) {
        std::cout << "Error occured: " << e.what()
                  << std::endl;
    }
    
    return 0;
}