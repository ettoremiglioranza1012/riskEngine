// .ccp src file of the class Portfolio

#include "../include/portfolio.hh"
#include <vector>

// Static member attributes definition
double Portfolio::global_market_volatility = 0.20;
double Portfolio::risk_free_rate = 0.05;

// Non-Inline member functions
void Portfolio::incr_vol(const double deltaVol)
{
    global_market_volatility += deltaVol;
}

void Portfolio::incr_rate(const double bonusRate)
{
    risk_free_rate += bonusRate;
}

void Portfolio::simulateOneDay()
{
    /*
        A very simple "Risk" calculation
        Value = Value * (1 + RiskFreeRate + Volatility)
        (In reality you'd use a random distribution here)
    */
    current_value *= (1.0 + risk_free_rate + global_market_volatility);
}