// Implementation of Model methods that use MarketEnvironment

#include "../include/model.hh"
#include "../include/marketEnvironment.hh"

// ============================================================================
// BlackScholesModel - Market Environment implementations
// ============================================================================

// Simulate with market environment (internal RNG - UNCORRELATED!)
double BlackScholesModel::simulate_step(double current_price, double dt,
                                         const std::string& ticker,
                                         const MarketEnvironment& env) {
    double z = normal_dist_(generator_);
    return simulate_step(current_price, dt, z, ticker, env);
}

// Simulate with market environment AND external random (CORRECT - supports correlation)
double BlackScholesModel::simulate_step(double current_price, double dt, double random_z,
                                         const std::string& ticker,
                                         const MarketEnvironment& env) {
    // Get rate from yield curve (short rate for simulation)
    double r = env.get_yield_curve().get_short_rate();
    
    // Get ATM vol from vol surface
    double sigma = env.get_vol_surface(ticker).get_atm_vol(dt);
    
    double drift = (r - 0.5 * sigma * sigma) * dt;
    double diffusion = sigma * std::sqrt(dt) * random_z;
    
    return current_price * std::exp(drift + diffusion);
}

double BlackScholesModel::price_option(double S, double K, double T,
                                        const std::string& ticker,
                                        const MarketEnvironment& env,
                                        bool is_call) const {
    // Get rate from yield curve at option maturity
    double r = env.get_rate(T);
    
    // Get implied vol from vol surface at this strike and expiry
    double sigma = env.get_vol(ticker, K, T);
    
    return price_option(S, K, T, r, sigma, is_call);
}

Greeks BlackScholesModel::calculate_greeks(double S, double K, double T,
                                            const std::string& ticker,
                                            const MarketEnvironment& env,
                                            bool is_call) const {
    // Get rate from yield curve
    double r = env.get_rate(T);
    
    // Get implied vol from vol surface
    double sigma = env.get_vol(ticker, K, T);
    
    return calculate_greeks(S, K, T, r, sigma, is_call);
}

// ============================================================================
// JumpDiffusionModel - Market Environment implementations
// ============================================================================

// Simulate with market environment (internal RNG - UNCORRELATED!)
double JumpDiffusionModel::simulate_step(double current_price, double dt,
                                          const std::string& ticker,
                                          const MarketEnvironment& env) {
    double z = normal_dist_(generator_);
    return simulate_step(current_price, dt, z, ticker, env);
}

// Simulate with market environment AND external random (CORRECT - supports correlation)
double JumpDiffusionModel::simulate_step(double current_price, double dt, double random_z,
                                          const std::string& ticker,
                                          const MarketEnvironment& env) {
    // Get rate from yield curve
    double r = env.get_yield_curve().get_short_rate();
    
    // Get vol from vol surface
    double sigma = env.get_vol_surface(ticker).get_atm_vol(dt);
    
    // GBM component with external random
    double k = std::exp(jump_mean_ + 0.5 * jump_vol_ * jump_vol_) - 1.0;
    double drift = (r - jump_intensity_ * k - 0.5 * sigma * sigma) * dt;
    double diffusion = sigma * std::sqrt(dt) * random_z;
    
    // Jump component (Poisson process) - note: jumps are idiosyncratic (independent)
    int num_jumps = poisson_dist_(generator_);
    double jump_component = 0.0;
    for (int i = 0; i < num_jumps; ++i) {
        jump_component += jump_size_dist_(generator_);
    }
    
    return current_price * std::exp(drift + diffusion + jump_component);
}

double JumpDiffusionModel::price_option(double S, double K, double T,
                                         const std::string& ticker,
                                         const MarketEnvironment& env,
                                         bool is_call) const {
    double r = env.get_rate(T);
    double sigma = env.get_vol(ticker, K, T);
    
    // Use BS approximation (would need MC for proper jump-diffusion pricing)
    BlackScholesModel bs(r, sigma);
    return bs.price_option(S, K, T, r, sigma, is_call);
}

Greeks JumpDiffusionModel::calculate_greeks(double S, double K, double T,
                                             const std::string& ticker,
                                             const MarketEnvironment& env,
                                             bool is_call) const {
    double r = env.get_rate(T);
    double sigma = env.get_vol(ticker, K, T);
    
    BlackScholesModel bs(r, sigma);
    return bs.calculate_greeks(S, K, T, r, sigma, is_call);
}

// ============================================================================
// MultiAssetSimulator - Correlated simulation implementations
// ============================================================================

std::map<std::string, double> MultiAssetSimulator::generate_correlated_shocks(
    const std::vector<std::string>& tickers,
    const MarketEnvironment& env) {
    
    const auto& corr_matrix = env.get_correlation_matrix();
    size_t n = tickers.size();
    
    // Generate independent standard normals
    std::vector<double> independent_z(n);
    for (size_t i = 0; i < n; ++i) {
        independent_z[i] = normal_dist_(generator_);
    }
    
    // Apply Cholesky transformation if correlation matrix is available
    std::vector<double> correlated_z;
    if (corr_matrix.size() == n) {
        correlated_z = corr_matrix.correlate(independent_z);
    } else {
        // Fall back to independent if no correlation defined
        correlated_z = independent_z;
    }
    
    // Map back to tickers
    std::map<std::string, double> result;
    for (size_t i = 0; i < n; ++i) {
        result[tickers[i]] = correlated_z[i];
    }
    
    return result;
}

std::map<std::string, double> MultiAssetSimulator::simulate_market_step(
    const std::map<std::string, double>& current_prices,
    double dt,
    const MarketEnvironment& env) {
    
    // Get ordered list of tickers
    std::vector<std::string> tickers;
    for (const auto& [ticker, price] : current_prices) {
        tickers.push_back(ticker);
    }
    
    // Generate correlated shocks
    auto correlated_z = generate_correlated_shocks(tickers, env);
    
    // Apply shocks to each asset
    std::map<std::string, double> new_prices;
    for (const auto& [ticker, price] : current_prices) {
        double z = correlated_z[ticker];
        new_prices[ticker] = model_.simulate_step(price, dt, z, ticker, env);
    }
    
    return new_prices;
}

std::vector<std::map<std::string, double>> MultiAssetSimulator::simulate_portfolio_paths(
    const std::map<std::string, double>& initial_prices,
    double T,
    size_t num_paths,
    size_t steps_per_year,
    const MarketEnvironment& env) {
    
    size_t num_steps = static_cast<size_t>(T * steps_per_year);
    if (num_steps < 1) num_steps = 1;
    double dt = T / num_steps;
    
    std::vector<std::map<std::string, double>> final_prices(num_paths);
    
    for (size_t path = 0; path < num_paths; ++path) {
        std::map<std::string, double> prices = initial_prices;
        
        for (size_t step = 0; step < num_steps; ++step) {
            prices = simulate_market_step(prices, dt, env);
        }
        
        final_prices[path] = prices;
    }
    
    return final_prices;
}
