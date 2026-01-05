// Header file of the class marketSimulator

#ifndef MARKET_SIMULATOR_H
#define MARKET_SIMULATOR_H

#include <string>
#include <vector>
#include "portfolio.hh"

class MarketSimulator {
public:
    MarketSimulator() = default;
    MarketSimulator(const MarketSimulator&) = delete;
    MarketSimulator& operator=(MarketSimulator&) = delete;
    MarketSimulator(MarketSimulator&&) = delete;
    MarketSimulator& operator=(const MarketSimulator&&) = delete; 
    void do_simulate_daily(double bonusRate = 0.0);
    void add_to_assets(Portfolio* p);
    unsigned get_day_count() { return simulation_day_count; } 
private:
    std::vector<Portfolio*> assets = {};
    static unsigned simulation_day_count;
};

inline 
void MarketSimulator::add_to_assets(Portfolio* p) 
{ 
    assets.push_back(p); 
}

#endif