// Header file of the class Portfolio

#ifndef PORTFOLIO_H
#define PORTFOLIO_H

#include <string>
#include <vector>
#include <stdexcept>

// Class declaration
class Portfolio;
class Auditor;

// Class definition
class Portfolio {

    // friends
    friend Auditor;

public:
    // Non-delegating constructor
    Portfolio(double val, std::string o, std::string c) :
        current_value(val), owner(o), currency(c) 
        { 
            if(val < 0) 
                throw std::runtime_error("Current value must be greater-equal than zero!");
        }
    
    // Delegating constructors
    explicit Portfolio() : Portfolio(0.0, "Unknown", "USD") { } // Excp. not needed, but safe, prevents accidental initialization 
    explicit Portfolio(double val) : Portfolio(val, "Unknown", "USD") { }
    
    // Static member functions
    static void update_global_vol(double newVol) {
        if (newVol < 0) 
            throw std::runtime_error("Volatily value must be greater-equal than zero");
        global_market_volatility = newVol; 
    }
    
    static double get_vol() { return global_market_volatility; }
    static double get_rate() { return risk_free_rate; }
    static void incr_vol(const double);
    static void incr_rate(const double); 
    
    // Member functions
    void simulateOneDay();
    double get_val(void) const;
    Portfolio& set_val(double);

private:
    // In-class initializer member attributes
    double current_value = 0.0;
    std::string owner = "Unknown";
    std::string currency = "USD";
    
    // Static member attributes
    static double global_market_volatility;
    static double risk_free_rate;
};

class Auditor {

};

inline
double Portfolio::get_val(void) const { return current_value; }

inline
Portfolio& Portfolio::set_val(double newVal)
{
    current_value = newVal;
    return *this;
}

#endif 




