// Header file of the class marketSimulator

#ifndef MARKET_SIMULATOR_H
#define MARKET_SIMULATOR_H

#include <string>
#include <vector>
#include <utility>
#include <cmath>
#include <memory>
#include <set>
#include "portfolio.hh"
#include "model.hh"
#include "visitor.hh"
#include "marketEnvironment.hh"

class MarketSimulator {
public:
    // Constructor takes a model for simulation
    explicit MarketSimulator(std::unique_ptr<Model> model)
        : model_(std::move(model)), 
          multi_asset_sim_(std::make_unique<MultiAssetSimulator>(*model_)) {}

    // Default constructor with Black-Scholes model
    MarketSimulator() 
        : model_(std::make_unique<BlackScholesModel>()),
          multi_asset_sim_(std::make_unique<MultiAssetSimulator>(*model_)) {}

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
    void set_model(std::unique_ptr<Model> model) { 
        model_ = std::move(model); 
        multi_asset_sim_ = std::make_unique<MultiAssetSimulator>(*model_);
    }

    // Market environment access (for correlated simulation)
    void set_market_environment(const MarketEnvironment& env) { market_env_ = env; }
    MarketEnvironment& get_market_environment() { return market_env_; }
    const MarketEnvironment& get_market_environment() const { return market_env_; }

    // ========================================================================
    // SIMULATION METHODS - CORRELATED by default
    // ========================================================================

    // Monte Carlo simulation - CORRELATED (correct method)
    // Simulates all unique underlyings together using Cholesky decomposition
    void simulate_daily() {
        constexpr double dt = 1.0 / 252.0;
        
        // Step 1: Collect all unique stock tickers and their current prices
        std::map<std::string, double> current_prices;
        std::map<std::string, Stock*> stock_ptrs;  // To update prices after simulation
        
        for (auto& portfolio : portfolios_) {
            portfolio.snapshot_prices();  // For P&L tracking
            collect_stocks(portfolio, current_prices, stock_ptrs);
        }
        
        // Step 2: If we have stocks and a correlation matrix, do correlated simulation
        if (!current_prices.empty() && market_env_.get_correlation_matrix().size() > 0) {
            // Simulate all stocks together (CORRELATED)
            auto new_prices = multi_asset_sim_->simulate_market_step(current_prices, dt, market_env_);
            
            // Update stock prices
            for (auto& [ticker, new_price] : new_prices) {
                if (stock_ptrs.count(ticker)) {
                    stock_ptrs[ticker]->set_price(new_price);
                }
            }
            
            // Update options (re-price based on new underlying + decay time)
            for (auto& portfolio : portfolios_) {
                update_options(portfolio, dt);
            }
        } else {
            // Fallback: uncorrelated simulation (legacy behavior)
            MonteCarloSimulationVisitor mc_visitor(*model_, dt);
            for (auto& portfolio : portfolios_) {
                portfolio.accept(mc_visitor);
            }
        }
        
        ++simulation_day_count_;
    }

    // LEGACY: Uncorrelated simulation (explicitly named to discourage use)
    void simulate_daily_uncorrelated() {
        constexpr double dt = 1.0 / 252.0;
        MonteCarloSimulationVisitor mc_visitor(*model_, dt);
        
        for (auto& portfolio : portfolios_) {
            portfolio.snapshot_prices();
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
    std::unique_ptr<MultiAssetSimulator> multi_asset_sim_;
    MarketEnvironment market_env_;
    unsigned simulation_day_count_ = 0;

    // Helper: Collect all stocks from a portfolio
    void collect_stocks(Portfolio& portfolio, 
                        std::map<std::string, double>& prices,
                        std::map<std::string, Stock*>& stock_ptrs) {
        for (size_t i = 0; i < portfolio.get_position_count(); ++i) {
            Position& pos = portfolio.get_position(i);
            Instrument& inst = pos.get_instrument();
            
            // Check if it's a Stock
            if (auto* stock = dynamic_cast<Stock*>(&inst)) {
                const std::string& ticker = stock->get_ticker();
                if (prices.find(ticker) == prices.end()) {
                    prices[ticker] = stock->get_price();
                    stock_ptrs[ticker] = stock;
                }
            }
            // Check if it's an Option (need to get underlying)
            else if (auto* option = dynamic_cast<Option*>(&inst)) {
                const Stock& underlying = option->get_underlying();
                const std::string& ticker = underlying.get_ticker();
                if (prices.find(ticker) == prices.end()) {
                    prices[ticker] = underlying.get_price();
                    // Note: underlying is const, but the shared_ptr may be modifiable
                    // We'll update via the stock_ptrs we already collected
                }
            }
        }
    }

    // Helper: Update options after underlying prices change
    void update_options(Portfolio& portfolio, double dt) {
        for (size_t i = 0; i < portfolio.get_position_count(); ++i) {
            Position& pos = portfolio.get_position(i);
            Instrument& inst = pos.get_instrument();
            
            if (auto* option = dynamic_cast<Option*>(&inst)) {
                // Decay time to expiry
                double new_tte = option->get_time_to_expiry() - dt;
                option->set_time_to_expiry(std::max(0.0, new_tte));
                
                // Re-price option based on new underlying price
                const Stock& underlying = option->get_underlying();
                if (option->get_time_to_expiry() > 0) {
                    double S = underlying.get_price();
                    double K = option->get_strike();
                    double T = option->get_time_to_expiry();
                    bool is_call = (option->get_type() == Option::Type::Call);
                    
                    // Get vol/rate from market environment or model
                    double new_price;
                    if (market_env_.get_correlation_matrix().size() > 0) {
                        new_price = model_->price_option(S, K, T, 
                            underlying.get_ticker(), market_env_, is_call);
                    } else {
                        double r = dynamic_cast<BlackScholesModel*>(model_.get())->get_rate();
                        double sigma = dynamic_cast<BlackScholesModel*>(model_.get())->get_volatility();
                        new_price = model_->price_option(S, K, T, r, sigma, is_call);
                    }
                    option->set_price(new_price);
                } else {
                    // At expiry - intrinsic value only
                    double S = underlying.get_price();
                    double K = option->get_strike();
                    bool is_call = (option->get_type() == Option::Type::Call);
                    double intrinsic = is_call ? std::max(0.0, S - K) : std::max(0.0, K - S);
                    option->set_price(intrinsic);
                }
            }
        }
    }
};

#endif