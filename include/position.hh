// Header file for the Position class

#ifndef POSITION_H
#define POSITION_H

#include <memory>
#include "instrument.hh"

// Forward declarations
class InstrumentVisitor;
class ConstInstrumentVisitor;
class Model;
struct Greeks;

// A Position = Quantity of an Instrument
// Pure data container - no simulation logic
class Position {
public:
    Position(std::shared_ptr<Instrument> instrument, double quantity)
        : instrument_(instrument), quantity_(quantity), 
          last_price_(instrument->get_price()) {}

    // Calculate total market value of this position
    double get_market_value() const {
        return quantity_ * instrument_->get_price();
    }

    // Record current price for P&L tracking
    void snapshot_price() {
        last_price_ = instrument_->get_price();
    }

    // Get P&L since last snapshot
    double get_pnl() const {
        return quantity_ * (instrument_->get_price() - last_price_);
    }

    // Accessors
    const Instrument& get_instrument() const { return *instrument_; }
    Instrument& get_instrument() { return *instrument_; }
    double get_quantity() const { return quantity_; }
    
    // Modify position
    void adjust_quantity(double delta) { quantity_ += delta; }
    void set_quantity(double q) { quantity_ = q; }

private:
    std::shared_ptr<Instrument> instrument_;
    double quantity_;
    double last_price_;  // For P&L tracking
};

#endif
