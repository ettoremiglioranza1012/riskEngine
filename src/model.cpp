// Implementation of Model methods that use MarketEnvironment

#include "../include/model.hh"
#include "../include/marketEnvironment.hh"

// ============================================================================
// BlackScholesModel - Market Environment implementations
// ============================================================================

double BlackScholesModel::simulate_step(double current_price, double dt,
                                         const std::string& ticker,
                                         const MarketEnvironment& env) {
    // Get rate from yield curve (short rate for simulation)
    double r = env.get_yield_curve().get_short_rate();
    
    // Get ATM vol from vol surface
    double sigma = env.get_vol_surface(ticker).get_atm_vol(dt);
    
    double z = normal_dist_(generator_);
    double drift = (r - 0.5 * sigma * sigma) * dt;
    double diffusion = sigma * std::sqrt(dt) * z;
    
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

double JumpDiffusionModel::simulate_step(double current_price, double dt,
                                          const std::string& ticker,
                                          const MarketEnvironment& env) {
    // Get rate from yield curve
    double r = env.get_yield_curve().get_short_rate();
    
    // Get vol from vol surface
    double sigma = env.get_vol_surface(ticker).get_atm_vol(dt);
    
    double z = normal_dist_(generator_);
    
    // GBM component
    double k = std::exp(jump_mean_ + 0.5 * jump_vol_ * jump_vol_) - 1.0;
    double drift = (r - jump_intensity_ * k - 0.5 * sigma * sigma) * dt;
    double diffusion = sigma * std::sqrt(dt) * z;
    
    // Jump component
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
