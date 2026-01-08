// Header file for the Model hierarchy
// Implements stochastic calculus models for pricing and risk

#ifndef MODEL_H
#define MODEL_H

#include <random>
#include <cmath>
#include <vector>
#include <string>

// Forward declaration
class MarketEnvironment;

// Greeks structure - sensitivities to market parameters
struct Greeks {
    double delta = 0.0;   // dV/dS - sensitivity to underlying price
    double gamma = 0.0;   // d²V/dS² - convexity
    double vega = 0.0;    // dV/dσ - sensitivity to volatility
    double theta = 0.0;   // dV/dt - time decay
    double rho = 0.0;     // dV/dr - sensitivity to interest rate
};

// Abstract base class for pricing models
class Model {
public:
    virtual ~Model() = default;

    // Simulate one time step, returns the new price
    virtual double simulate_step(double current_price, double dt) = 0;

    // Simulate with market environment (uses appropriate vol/rate from curves)
    virtual double simulate_step(double current_price, double dt, 
                                  const std::string& ticker,
                                  const MarketEnvironment& env) = 0;

    // Calculate option price (for derivative pricing)
    virtual double price_option(double S, double K, double T, double r, double sigma, bool is_call) const = 0;

    // Price option using market environment
    virtual double price_option(double S, double K, double T, 
                                 const std::string& ticker,
                                 const MarketEnvironment& env,
                                 bool is_call) const = 0;

    // Calculate Greeks for an option
    virtual Greeks calculate_greeks(double S, double K, double T, double r, double sigma, bool is_call) const = 0;

    // Calculate Greeks using market environment
    virtual Greeks calculate_greeks(double S, double K, double T,
                                     const std::string& ticker,
                                     const MarketEnvironment& env,
                                     bool is_call) const = 0;

    // Setters for model parameters (fallback when no market env)
    virtual void set_volatility(double sigma) = 0;
    virtual void set_rate(double r) = 0;
    virtual void set_seed(unsigned seed) = 0;
};

// Black-Scholes Model: Geometric Brownian Motion
// dS = μS dt + σS dW
class BlackScholesModel : public Model {
public:
    BlackScholesModel(double rate = 0.05, double volatility = 0.20, unsigned seed = 42)
        : rate_(rate), volatility_(volatility), generator_(seed), normal_dist_(0.0, 1.0) {}

    // GBM simulation step (uses model's rate/vol)
    double simulate_step(double current_price, double dt) override {
        double z = normal_dist_(generator_);
        // S(t+dt) = S(t) * exp((r - 0.5*σ²)dt + σ√dt * Z)
        double drift = (rate_ - 0.5 * volatility_ * volatility_) * dt;
        double diffusion = volatility_ * std::sqrt(dt) * z;
        return current_price * std::exp(drift + diffusion);
    }

    // GBM simulation step using market environment
    double simulate_step(double current_price, double dt,
                          const std::string& ticker,
                          const MarketEnvironment& env) override;

    // Black-Scholes closed-form option price
    double price_option(double S, double K, double T, double r, double sigma, bool is_call) const override {
        if (T <= 0) {
            // At expiry
            return is_call ? std::max(0.0, S - K) : std::max(0.0, K - S);
        }

        double d1 = (std::log(S / K) + (r + 0.5 * sigma * sigma) * T) / (sigma * std::sqrt(T));
        double d2 = d1 - sigma * std::sqrt(T);

        if (is_call) {
            return S * norm_cdf(d1) - K * std::exp(-r * T) * norm_cdf(d2);
        } else {
            return K * std::exp(-r * T) * norm_cdf(-d2) - S * norm_cdf(-d1);
        }
    }

    // Price option using market environment (vol surface + yield curve)
    double price_option(double S, double K, double T,
                         const std::string& ticker,
                         const MarketEnvironment& env,
                         bool is_call) const override;

    // Analytical Greeks from Black-Scholes
    Greeks calculate_greeks(double S, double K, double T, double r, double sigma, bool is_call) const override {
        Greeks g;
        
        if (T <= 0) {
            // At expiry - only delta matters
            if (is_call) {
                g.delta = (S > K) ? 1.0 : 0.0;
            } else {
                g.delta = (S < K) ? -1.0 : 0.0;
            }
            return g;
        }

        double sqrt_T = std::sqrt(T);
        double d1 = (std::log(S / K) + (r + 0.5 * sigma * sigma) * T) / (sigma * sqrt_T);
        double d2 = d1 - sigma * sqrt_T;
        double nd1 = norm_cdf(d1);
        double nd2 = norm_cdf(d2);
        double pdf_d1 = norm_pdf(d1);

        // Delta: dV/dS
        g.delta = is_call ? nd1 : (nd1 - 1.0);

        // Gamma: d²V/dS² (same for call and put)
        g.gamma = pdf_d1 / (S * sigma * sqrt_T);

        // Vega: dV/dσ (same for call and put)
        g.vega = S * pdf_d1 * sqrt_T;

        // Theta: dV/dt
        double discount = std::exp(-r * T);
        if (is_call) {
            g.theta = -(S * pdf_d1 * sigma) / (2.0 * sqrt_T) 
                      - r * K * discount * nd2;
        } else {
            g.theta = -(S * pdf_d1 * sigma) / (2.0 * sqrt_T) 
                      + r * K * discount * norm_cdf(-d2);
        }

        // Rho: dV/dr
        if (is_call) {
            g.rho = K * T * discount * nd2;
        } else {
            g.rho = -K * T * discount * norm_cdf(-d2);
        }

        return g;
    }

    // Calculate Greeks using market environment
    Greeks calculate_greeks(double S, double K, double T,
                             const std::string& ticker,
                             const MarketEnvironment& env,
                             bool is_call) const override;

    void set_volatility(double sigma) override { volatility_ = sigma; }
    void set_rate(double r) override { rate_ = r; }
    void set_seed(unsigned seed) override { generator_.seed(seed); }

    double get_volatility() const { return volatility_; }
    double get_rate() const { return rate_; }

private:
    double rate_;
    double volatility_;
    mutable std::mt19937 generator_;
    mutable std::normal_distribution<double> normal_dist_;

public:
    // Standard normal CDF (approximation) - public for use by visitors
    static double norm_cdf(double x) {
        return 0.5 * std::erfc(-x * M_SQRT1_2);
    }

    // Standard normal PDF
    static double norm_pdf(double x) {
        return std::exp(-0.5 * x * x) / std::sqrt(2.0 * M_PI);
    }
};

// Monte Carlo Pricer - uses any Model for path simulation
class MonteCarloPricer {
public:
    MonteCarloPricer(Model& model, size_t num_paths = 10000, size_t steps_per_year = 252)
        : model_(model), num_paths_(num_paths), steps_per_year_(steps_per_year) {}

    // Price an option using Monte Carlo simulation
    double price_option(double S0, double K, double T, double r, bool is_call) const {
        if (T <= 0) {
            return is_call ? std::max(0.0, S0 - K) : std::max(0.0, K - S0);
        }

        size_t num_steps = static_cast<size_t>(T * steps_per_year_);
        if (num_steps < 1) num_steps = 1;
        double dt = T / num_steps;

        double payoff_sum = 0.0;

        for (size_t path = 0; path < num_paths_; ++path) {
            double S = S0;
            for (size_t step = 0; step < num_steps; ++step) {
                S = model_.simulate_step(S, dt);
            }
            double payoff = is_call ? std::max(0.0, S - K) : std::max(0.0, K - S);
            payoff_sum += payoff;
        }

        double expected_payoff = payoff_sum / num_paths_;
        return expected_payoff * std::exp(-r * T);  // Discount to present value
    }

    // Generate multiple price paths for VaR/stress testing
    std::vector<double> simulate_paths(double S0, double T, size_t num_paths) const {
        size_t num_steps = static_cast<size_t>(T * steps_per_year_);
        if (num_steps < 1) num_steps = 1;
        double dt = T / num_steps;

        std::vector<double> final_prices(num_paths);

        for (size_t path = 0; path < num_paths; ++path) {
            double S = S0;
            for (size_t step = 0; step < num_steps; ++step) {
                S = model_.simulate_step(S, dt);
            }
            final_prices[path] = S;
        }

        return final_prices;
    }

private:
    Model& model_;
    size_t num_paths_;
    size_t steps_per_year_;
};

// Jump-Diffusion Model (Merton): GBM + Poisson jumps
// dS = (μ - λk)S dt + σS dW + S(J-1)dN
class JumpDiffusionModel : public Model {
public:
    JumpDiffusionModel(double rate = 0.05, double volatility = 0.20, 
                       double jump_intensity = 1.0, double jump_mean = -0.05, 
                       double jump_vol = 0.10, unsigned seed = 42)
        : rate_(rate), volatility_(volatility), 
          jump_intensity_(jump_intensity), jump_mean_(jump_mean), jump_vol_(jump_vol),
          generator_(seed), normal_dist_(0.0, 1.0), 
          poisson_dist_(jump_intensity), jump_size_dist_(jump_mean, jump_vol) {}

    double simulate_step(double current_price, double dt) override {
        double z = normal_dist_(generator_);
        
        // GBM component
        double k = std::exp(jump_mean_ + 0.5 * jump_vol_ * jump_vol_) - 1.0;
        double drift = (rate_ - jump_intensity_ * k - 0.5 * volatility_ * volatility_) * dt;
        double diffusion = volatility_ * std::sqrt(dt) * z;
        
        // Jump component (Poisson process)
        int num_jumps = poisson_dist_(generator_);
        double jump_component = 0.0;
        for (int i = 0; i < num_jumps; ++i) {
            jump_component += jump_size_dist_(generator_);
        }
        
        return current_price * std::exp(drift + diffusion + jump_component);
    }

    // Simulate with market environment
    double simulate_step(double current_price, double dt,
                          const std::string& ticker,
                          const MarketEnvironment& env) override;

    // Use Monte Carlo for option pricing (no closed form for jump-diffusion)
    double price_option(double S, double K, double T, double r, double sigma, bool is_call) const override {
        (void)sigma; // Uses internal volatility
        // Simplified - would need Monte Carlo in practice
        // Fall back to BS approximation
        BlackScholesModel bs(r, volatility_);
        return bs.price_option(S, K, T, r, volatility_, is_call);
    }

    // Price option using market environment
    double price_option(double S, double K, double T,
                         const std::string& ticker,
                         const MarketEnvironment& env,
                         bool is_call) const override;

    Greeks calculate_greeks(double S, double K, double T, double r, double sigma, bool is_call) const override {
        // Use BS approximation for Greeks
        BlackScholesModel bs(r, volatility_);
        return bs.calculate_greeks(S, K, T, r, sigma, is_call);
    }

    // Calculate Greeks using market environment
    Greeks calculate_greeks(double S, double K, double T,
                             const std::string& ticker,
                             const MarketEnvironment& env,
                             bool is_call) const override;

    void set_volatility(double sigma) override { volatility_ = sigma; }
    void set_rate(double r) override { rate_ = r; }
    void set_seed(unsigned seed) override { generator_.seed(seed); }

private:
    double rate_;
    double volatility_;
    double jump_intensity_;  // λ - expected jumps per year
    double jump_mean_;       // μ_J - mean of log jump size
    double jump_vol_;        // σ_J - volatility of log jump size
    
    mutable std::mt19937 generator_;
    mutable std::normal_distribution<double> normal_dist_;
    mutable std::poisson_distribution<int> poisson_dist_;
    mutable std::normal_distribution<double> jump_size_dist_;
};

#endif
