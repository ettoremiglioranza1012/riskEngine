// Header file of the class Portfolio

#ifndef PORTFOLIO_H
#define PORTFOLIO_H

#include <string>
#include <vector>
#include <stdexcept>
#include <memory>
#include "position.hh"

// Forward declarations
class InstrumentVisitor;
class ConstInstrumentVisitor;

class Auditor;

// Portfolio: A container of Positions belonging to an owner
// Pure DATA - no simulation logic (Visitor Pattern)
class Portfolio {
    friend Auditor;

public:
    Portfolio(std::string owner, std::string currency)
        : owner_(owner), currency_(currency) {}

    explicit Portfolio() : Portfolio("Unknown", "USD") {}

    // Add a position to the portfolio
    void add_position(std::shared_ptr<Instrument> instrument, double quantity) {
        positions_.emplace_back(instrument, quantity);
    }

    // Calculate total market value across all positions
    double get_total_value() const {
        double total = 0.0;
        for (const auto& pos : positions_) {
            total += pos.get_market_value();
        }
        return total;
    }

    // Get total P&L since last snapshot
    double get_total_pnl() const {
        double total_pnl = 0.0;
        for (const auto& pos : positions_) {
            total_pnl += pos.get_pnl();
        }
        return total_pnl;
    }

    // Snapshot all positions for P&L tracking
    void snapshot_prices() {
        for (auto& pos : positions_) {
            pos.snapshot_price();
        }
    }

    // Apply a visitor to all instruments
    void accept(InstrumentVisitor& visitor) {
        for (auto& pos : positions_) {
            pos.get_instrument().accept(visitor);
        }
    }

    void accept(ConstInstrumentVisitor& visitor) const {
        for (const auto& pos : positions_) {
            pos.get_instrument().accept(visitor);
        }
    }

    // Accessors
    const std::string& get_owner() const { return owner_; }
    const std::string& get_currency() const { return currency_; }
    size_t get_position_count() const { return positions_.size(); }
    
    const Position& get_position(size_t idx) const { return positions_[idx]; }
    Position& get_position(size_t idx) { return positions_[idx]; }

private:
    std::string owner_;
    std::string currency_;
    std::vector<Position> positions_;
};

class Auditor {};

#endif 




