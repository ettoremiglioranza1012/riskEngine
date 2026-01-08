// Header file for the Instrument hierarchy

#ifndef INSTRUMENT_H
#define INSTRUMENT_H

#include <string>
#include <memory>
#include "model.hh"

// Forward declarations
class Model;
class InstrumentVisitor;
class ConstInstrumentVisitor;

// Abstract base class for all tradeable instruments
// NOTE: Instruments are pure DATA - no simulation logic here (Visitor Pattern)
class Instrument {
public:
    Instrument(std::string ticker, double price)
        : ticker_(ticker), current_price_(price) {}
    
    virtual ~Instrument() = default;

    // Visitor pattern - accept visitors for operations
    virtual void accept(InstrumentVisitor& visitor) = 0;
    virtual void accept(ConstInstrumentVisitor& visitor) const = 0;

    // Mark-to-market P&L from price change
    virtual double calculate_pnl(double old_price) const {
        return current_price_ - old_price;
    }
    
    // Common interface - pure data accessors
    const std::string& get_ticker() const { return ticker_; }
    double get_price() const { return current_price_; }
    void set_price(double p) { current_price_ = p; }

protected:
    std::string ticker_;
    double current_price_;
};

// Stock: Linear risk profile, follows GBM directly
// Pure data - no simulation logic
class Stock : public Instrument {
public:
    Stock(std::string ticker, double price)
        : Instrument(ticker, price) {}

    void accept(InstrumentVisitor& visitor) override;
    void accept(ConstInstrumentVisitor& visitor) const override;
};

// Option: Non-linear (convex) risk profile
// Pure data - no pricing logic
class Option : public Instrument {
public:
    enum class Type { Call, Put };

    Option(std::string ticker, double premium, double strike, 
           std::shared_ptr<Stock> underlying, double time_to_expiry, Type type)
        : Instrument(ticker, premium), strike_(strike), 
          underlying_(underlying), time_to_expiry_(time_to_expiry), type_(type) {}

    void accept(InstrumentVisitor& visitor) override;
    void accept(ConstInstrumentVisitor& visitor) const override;

    // Data accessors
    double get_strike() const { return strike_; }
    double get_time_to_expiry() const { return time_to_expiry_; }
    void set_time_to_expiry(double tte) { time_to_expiry_ = tte; }
    Type get_type() const { return type_; }
    const Stock& get_underlying() const { return *underlying_; }

private:
    double strike_;
    std::shared_ptr<Stock> underlying_;
    double time_to_expiry_;  // In years
    Type type_;
};

// Bond: Interest rate sensitive
// Pure data - no simulation logic
class Bond : public Instrument {
public:
    Bond(std::string ticker, double price, double duration, double coupon_rate = 0.0)
        : Instrument(ticker, price), duration_(duration), coupon_rate_(coupon_rate) {}

    void accept(InstrumentVisitor& visitor) override;
    void accept(ConstInstrumentVisitor& visitor) const override;

    // Data accessors
    double get_duration() const { return duration_; }
    double get_coupon_rate() const { return coupon_rate_; }

private:
    double duration_;     // Macaulay duration in years
    double coupon_rate_;  // Annual coupon as decimal
};

#endif
