// .ccp src file of the class marketSimulator

#include "../include/marketSimulator.hh"
#include <vector>

unsigned MarketSimulator::simulation_day_count = 0;

void MarketSimulator::do_simulate_daily(double bonusRate)
{
    if (simulation_day_count % 30 == 0)
    {
        bonusRate = 0.001;
        Portfolio::incr_rate(bonusRate);
    }
    
    for (auto* asset : assets)
    {
        asset->simulateOneDay();
    }
    ++simulation_day_count; 
}