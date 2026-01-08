// Header file for the Visitor Pattern implementation
// Separates pricing/simulation logic from instrument data

#ifndef VISITOR_H
#define VISITOR_H

#include <vector>
#include <algorithm>
#include <numeric>
#include "model.hh"  // For Greeks struct

// Forward declarations
class Stock;
class Option;
class Bond;
class Position;
class Portfolio;
class Model;

// Abstract Visitor interface - defines what operations can be performed
class InstrumentVisitor {
public:
    virtual ~InstrumentVisitor() = default;
    
    virtual void visit(Stock& stock) = 0;
    virtual void visit(Option& option) = 0;
    virtual void visit(Bond& bond) = 0;
};

// Const visitor for read-only operations (Greeks, valuation)
class ConstInstrumentVisitor {
public:
    virtual ~ConstInstrumentVisitor() = default;
    
    virtual void visit(const Stock& stock) = 0;
    virtual void visit(const Option& option) = 0;
    virtual void visit(const Bond& bond) = 0;
};

// ============================================================================
// SIMULATION VISITORS - Different methods to evolve prices
// ============================================================================

// Monte Carlo simulation using GBM (or any stochastic model)
class MonteCarloSimulationVisitor : public InstrumentVisitor {
public:
    MonteCarloSimulationVisitor(Model& model, double dt)
        : model_(model), dt_(dt) {}

    void visit(Stock& stock) override;
    void visit(Option& option) override;
    void visit(Bond& bond) override;

private:
    Model& model_;
    double dt_;
};

// Historical simulation - uses historical returns
class HistoricalSimulationVisitor : public InstrumentVisitor {
public:
    HistoricalSimulationVisitor(const std::vector<double>& historical_returns, size_t day_index)
        : historical_returns_(historical_returns), day_index_(day_index % historical_returns.size()) {}

    void visit(Stock& stock) override;
    void visit(Option& option) override;
    void visit(Bond& bond) override;

private:
    const std::vector<double>& historical_returns_;
    size_t day_index_;
};

// Stress test simulation - applies a fixed shock
class StressTestVisitor : public InstrumentVisitor {
public:
    StressTestVisitor(double price_shock, double vol_shock, double rate_shock)
        : price_shock_(price_shock), vol_shock_(vol_shock), rate_shock_(rate_shock) {}

    void visit(Stock& stock) override;
    void visit(Option& option) override;
    void visit(Bond& bond) override;

private:
    double price_shock_;  // e.g., -0.20 for 20% crash
    double vol_shock_;    // e.g., +0.30 for vol spike
    double rate_shock_;   // e.g., +0.01 for 100bp rate hike
};

// ============================================================================
// VALUATION VISITORS - Different methods to calculate value/Greeks
// ============================================================================

// Greeks calculation visitor
class GreeksVisitor : public ConstInstrumentVisitor {
public:
    explicit GreeksVisitor(const Model& model) : model_(model) {}

    void visit(const Stock& stock) override;
    void visit(const Option& option) override;
    void visit(const Bond& bond) override;

    Greeks get_result() const { return result_; }
    void reset() { result_ = Greeks{}; }

private:
    const Model& model_;
    Greeks result_{};
};

// Market value visitor
class MarketValueVisitor : public ConstInstrumentVisitor {
public:
    MarketValueVisitor() = default;

    void visit(const Stock& stock) override;
    void visit(const Option& option) override;
    void visit(const Bond& bond) override;

    double get_value() const { return value_; }
    void reset() { value_ = 0.0; }

private:
    double value_ = 0.0;
};

// ============================================================================
// PORTFOLIO-LEVEL VISITORS
// ============================================================================

// Visits all positions in a portfolio and applies instrument visitor
class PortfolioSimulationVisitor {
public:
    PortfolioSimulationVisitor(InstrumentVisitor& instrument_visitor)
        : visitor_(instrument_visitor) {}

    void visit(Portfolio& portfolio);

private:
    InstrumentVisitor& visitor_;
};

// Portfolio Greeks aggregator
class PortfolioGreeksVisitor {
public:
    explicit PortfolioGreeksVisitor(const Model& model) : model_(model) {}

    void visit(const Portfolio& portfolio);
    Greeks get_total_greeks() const { return total_greeks_; }
    void reset() { total_greeks_ = Greeks{}; }

private:
    const Model& model_;
    Greeks total_greeks_{};
};

// VaR (Value at Risk) calculator using historical simulation
class VaRVisitor {
public:
    VaRVisitor(const std::vector<std::vector<double>>& historical_returns, 
               double confidence_level = 0.95)
        : historical_returns_(historical_returns), 
          confidence_level_(confidence_level) {}

    double calculate_var(Portfolio& portfolio);

private:
    const std::vector<std::vector<double>>& historical_returns_;
    double confidence_level_;
};

#endif
