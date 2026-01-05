// .ccp src file of the class marketSimulator

#include "marketSimulator.hh"
#include <vector>

unsigned MarketSimulator::simulation_day_count = 0;

void MarketSimulator::do_simulate_daily(double bonusRate)
{
    if (simulation_day_count % 30 == 0)
    {
        bonusRate = 0.001;
    }
    std::vector<Portfolio*>::iterator curr_asset = assets.begin();
    std::vector<Portfolio*>::iterator assets_end = assets.end();
    for (; curr_asset != assets_end; ++curr_asset)
    {
        
        (*curr_asset)->simulateOneDay();
    }
    if (bonusRate)
            (*curr_asset)->incr_rate(bonusRate);
    ++simulation_day_count; 
}