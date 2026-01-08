// Implementation of Visitor Pattern for pricing and simulation

#include "../include/visitor.hh"
#include "../include/instrument.hh"
#include "../include/position.hh"
#include "../include/portfolio.hh"
#include "../include/model.hh"

// ============================================================================
// MONTE CARLO SIMULATION VISITOR
// ============================================================================

void MonteCarloSimulationVisitor::visit(Stock& stock) {
    double new_price = model_.simulate_step(stock.get_price(), dt_);
    stock.set_price(new_price);
}

void MonteCarloSimulationVisitor::visit(Option& option) {
    // Reduce time to expiry
    double tte = option.get_time_to_expiry() - dt_;
    if (tte < 0) tte = 0;
    option.set_time_to_expiry(tte);

    // Re-price using model
    bool is_call = (option.get_type() == Option::Type::Call);
    double S = option.get_underlying().get_price();
    
    const auto* bs = dynamic_cast<const BlackScholesModel*>(&model_);
    double r = bs ? bs->get_rate() : 0.05;
    double sigma = bs ? bs->get_volatility() : 0.20;
    
    double new_price = model_.price_option(S, option.get_strike(), tte, r, sigma, is_call);
    option.set_price(new_price);
}

void MonteCarloSimulationVisitor::visit(Bond& bond) {
    const auto* bs = dynamic_cast<const BlackScholesModel*>(&model_);
    
    // Simulate small rate change
    double rate_change = (model_.simulate_step(1.0, dt_) - 1.0) * 0.1;
    double new_price = bond.get_price() * (1.0 - bond.get_duration() * rate_change);
    
    // Add accrued interest
    new_price += bond.get_coupon_rate() * dt_ * 100.0;
    bond.set_price(new_price);
}

// ============================================================================
// HISTORICAL SIMULATION VISITOR
// ============================================================================

void HistoricalSimulationVisitor::visit(Stock& stock) {
    double return_today = historical_returns_[day_index_];
    double new_price = stock.get_price() * (1.0 + return_today);
    stock.set_price(new_price);
}

void HistoricalSimulationVisitor::visit(Option& option) {
    // For historical sim, we apply the return to underlying and reprice
    // Note: underlying is already updated, so just reprice
    double tte = option.get_time_to_expiry() - (1.0/252.0);
    if (tte < 0) tte = 0;
    option.set_time_to_expiry(tte);
    
    // Simple intrinsic value for historical sim
    double S = option.get_underlying().get_price();
    double K = option.get_strike();
    bool is_call = (option.get_type() == Option::Type::Call);
    
    double intrinsic = is_call ? std::max(0.0, S - K) : std::max(0.0, K - S);
    // Add some time value approximation
    double time_value = option.get_price() * 0.99;  // Simple decay
    option.set_price(std::max(intrinsic, time_value));
}

void HistoricalSimulationVisitor::visit(Bond& bond) {
    // Bonds use rate returns (inversely)
    double rate_return = historical_returns_[day_index_] * 0.1;  // Scaled
    double new_price = bond.get_price() * (1.0 - bond.get_duration() * rate_return);
    new_price += bond.get_coupon_rate() * (1.0/252.0) * 100.0;
    bond.set_price(new_price);
}

// ============================================================================
// STRESS TEST VISITOR
// ============================================================================

void StressTestVisitor::visit(Stock& stock) {
    double new_price = stock.get_price() * (1.0 + price_shock_);
    stock.set_price(new_price);
}

void StressTestVisitor::visit(Option& option) {
    // Apply shock to underlying (already done if shared)
    // Re-price with stressed vol
    double S = option.get_underlying().get_price();
    double K = option.get_strike();
    double tte = option.get_time_to_expiry();
    bool is_call = (option.get_type() == Option::Type::Call);
    
    double base_vol = 0.20;
    double stressed_vol = base_vol + vol_shock_;
    double r = 0.05 + rate_shock_;
    
    // Simple BS approximation for stress test
    BlackScholesModel bs(r, stressed_vol);
    double new_price = bs.price_option(S, K, tte, r, stressed_vol, is_call);
    option.set_price(new_price);
}

void StressTestVisitor::visit(Bond& bond) {
    // Bonds are shocked by rate change
    double new_price = bond.get_price() * (1.0 - bond.get_duration() * rate_shock_);
    bond.set_price(new_price);
}

// ============================================================================
// GREEKS VISITOR
// ============================================================================

void GreeksVisitor::visit(const Stock& stock) {
    (void)stock;
    result_.delta = 1.0;
    result_.gamma = 0.0;
    result_.vega = 0.0;
    result_.theta = 0.0;
    result_.rho = 0.0;
}

void GreeksVisitor::visit(const Option& option) {
    bool is_call = (option.get_type() == Option::Type::Call);
    double S = option.get_underlying().get_price();
    double K = option.get_strike();
    double T = option.get_time_to_expiry();
    
    const auto* bs = dynamic_cast<const BlackScholesModel*>(&model_);
    double r = bs ? bs->get_rate() : 0.05;
    double sigma = bs ? bs->get_volatility() : 0.20;
    
    result_ = model_.calculate_greeks(S, K, T, r, sigma, is_call);
}

void GreeksVisitor::visit(const Bond& bond) {
    result_.delta = 0.0;
    result_.gamma = 0.0;
    result_.vega = 0.0;
    result_.theta = bond.get_coupon_rate() / 365.0;
    result_.rho = -bond.get_duration() * bond.get_price();
}

// ============================================================================
// MARKET VALUE VISITOR
// ============================================================================

void MarketValueVisitor::visit(const Stock& stock) {
    value_ = stock.get_price();
}

void MarketValueVisitor::visit(const Option& option) {
    value_ = option.get_price();
}

void MarketValueVisitor::visit(const Bond& bond) {
    value_ = bond.get_price();
}

// ============================================================================
// PORTFOLIO VISITORS
// ============================================================================

void PortfolioSimulationVisitor::visit(Portfolio& portfolio) {
    for (size_t i = 0; i < portfolio.get_position_count(); ++i) {
        portfolio.get_position(i).get_instrument().accept(visitor_);
    }
}

void PortfolioGreeksVisitor::visit(const Portfolio& portfolio) {
    GreeksVisitor greeks_visitor(model_);
    
    for (size_t i = 0; i < portfolio.get_position_count(); ++i) {
        const Position& pos = portfolio.get_position(i);
        greeks_visitor.reset();
        pos.get_instrument().accept(greeks_visitor);
        
        Greeks g = greeks_visitor.get_result();
        double qty = pos.get_quantity();
        
        total_greeks_.delta += g.delta * qty;
        total_greeks_.gamma += g.gamma * qty;
        total_greeks_.vega += g.vega * qty;
        total_greeks_.theta += g.theta * qty;
        total_greeks_.rho += g.rho * qty;
    }
}

double VaRVisitor::calculate_var(Portfolio& portfolio) {
    std::vector<double> pnl_distribution;
    double initial_value = portfolio.get_total_value();
    
    // For each historical scenario
    for (size_t day = 0; day < historical_returns_.size(); ++day) {
        // Create a copy of current prices
        std::vector<double> original_prices;
        for (size_t i = 0; i < portfolio.get_position_count(); ++i) {
            original_prices.push_back(portfolio.get_position(i).get_instrument().get_price());
        }
        
        // Apply historical scenario
        HistoricalSimulationVisitor hist_visitor(historical_returns_[day], 0);
        PortfolioSimulationVisitor port_visitor(hist_visitor);
        port_visitor.visit(portfolio);
        
        // Calculate P&L
        double scenario_value = portfolio.get_total_value();
        pnl_distribution.push_back(scenario_value - initial_value);
        
        // Restore original prices
        for (size_t i = 0; i < portfolio.get_position_count(); ++i) {
            portfolio.get_position(i).get_instrument().set_price(original_prices[i]);
        }
    }
    
    // Sort P&L and find VaR at confidence level
    std::sort(pnl_distribution.begin(), pnl_distribution.end());
    size_t var_index = static_cast<size_t>((1.0 - confidence_level_) * pnl_distribution.size());
    
    return -pnl_distribution[var_index];  // VaR is positive loss
}
