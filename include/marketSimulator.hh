// Header file of the class marketSimulator

#ifndef MARKET_SIMULATOR_H
#define MARKET_SIMULATOR_H

#include <string>
#include <vector>
#include <utility>
#include <cmath>
#include <memory>
#include "portfolio.hh"
#include "model.hh"
#include "visitor.hh"

class MarketSimulator {
public:
    // Constructor takes a model for simulation
    explicit MarketSimulator(std::unique_ptr<Model> model)
        : model_(std::move(model)) {}

    // Default constructor with Black-Scholes model
    MarketSimulator() : model_(std::make_unique<BlackScholesModel>()) {}

    MarketSimulator(const MarketSimulator&) = delete;
    MarketSimulator& operator=(const MarketSimulator&) = delete;
    MarketSimulator(MarketSimulator&&) = default;
    MarketSimulator& operator=(MarketSimulator&&) = default;

    // Create a portfolio in-place, returns its ID
    template <typename... Args>
    size_t create_portfolio(Args&&... args) {
        portfolios_.emplace_back(std::forward<Args>(args)...);
        return portfolios_.size() - 1;
    }

    // Reserve capacity (HPC best practice)
    void reserve_portfolios(size_t n) { portfolios_.reserve(n); }

    // Access portfolios by ID
    double get_portfolio_value(size_t id) const { return portfolios_[id].get_total_value(); }
    const Portfolio& get_portfolio(size_t id) const { return portfolios_[id]; }
    Portfolio& get_portfolio(size_t id) { return portfolios_[id]; }

    // Model access
    Model& get_model() { return *model_; }
    const Model& get_model() const { return *model_; }
    void set_model(std::unique_ptr<Model> model) { model_ = std::move(model); }

    // ========================================================================
    // SIMULATION METHODS - Use different visitors for different methods
    // ========================================================================

    // Monte Carlo simulation (default)
    void simulate_daily() {
        constexpr double dt = 1.0 / 252.0;
        MonteCarloSimulationVisitor mc_visitor(*model_, dt);
        
        for (auto& portfolio : portfolios_) {
            portfolio.snapshot_prices();  // For P&L tracking
            portfolio.accept(mc_visitor);
        }
        ++simulation_day_count_;
    }

    // Historical simulation
    void simulate_daily_historical(const std::vector<double>& returns) {
        HistoricalSimulationVisitor hist_visitor(returns, simulation_day_count_);
        
        for (auto& portfolio : portfolios_) {
            portfolio.snapshot_prices();
            portfolio.accept(hist_visitor);
        }
        ++simulation_day_count_;
    }

    // Stress test
    void apply_stress_test(double price_shock, double vol_shock, double rate_shock) {
        StressTestVisitor stress_visitor(price_shock, vol_shock, rate_shock);
        
        for (auto& portfolio : portfolios_) {
            portfolio.snapshot_prices();
            portfolio.accept(stress_visitor);
        }
    }

    // Custom visitor
    void simulate_with_visitor(InstrumentVisitor& visitor) {
        for (auto& portfolio : portfolios_) {
            portfolio.snapshot_prices();
            portfolio.accept(visitor);
        }
        ++simulation_day_count_;
    }

    // Simulate multiple days
    void simulate_days(size_t num_days) {
        for (size_t i = 0; i < num_days; ++i) {
            simulate_daily();
        }
    }

    unsigned get_day_count() const { return simulation_day_count_; }
    size_t get_portfolio_count() const { return portfolios_.size(); }

    // ========================================================================
    // ANALYTICS - Use const visitors
    // ========================================================================

    // Get aggregate Greeks for a portfolio
    Greeks get_portfolio_greeks(size_t id) const {
        PortfolioGreeksVisitor greeks_visitor(get_model());
        greeks_visitor.visit(portfolios_[id]);
        return greeks_visitor.get_total_greeks();
    }

    // Get aggregate Greeks across all portfolios
    Greeks get_total_greeks() const {
        Greeks total;
        for (const auto& portfolio : portfolios_) {
            PortfolioGreeksVisitor greeks_visitor(get_model());
            greeks_visitor.visit(portfolio);
            Greeks g = greeks_visitor.get_total_greeks();
            total.delta += g.delta;
            total.gamma += g.gamma;
            total.vega += g.vega;
            total.theta += g.theta;
            total.rho += g.rho;
        }
        return total;
    }

private:
    std::vector<Portfolio> portfolios_;
    std::unique_ptr<Model> model_;
    unsigned simulation_day_count_ = 0;
};

#endif