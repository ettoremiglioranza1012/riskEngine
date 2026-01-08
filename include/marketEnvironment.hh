// Header file for Market Environment
// Contains term structures: Yield Curves, Volatility Surfaces, Spot Prices

#ifndef MARKET_ENVIRONMENT_H
#define MARKET_ENVIRONMENT_H

#include <map>
#include <vector>
#include <string>
#include <cmath>
#include <stdexcept>
#include <algorithm>

// ============================================================================
// YIELD CURVE - Term structure of interest rates
// ============================================================================

class YieldCurve {
public:
    // Flat curve constructor
    explicit YieldCurve(double flat_rate = 0.05) : flat_rate_(flat_rate) {}

    // Curve from tenor points (e.g., {0.25, 0.04}, {1.0, 0.045}, {10.0, 0.05})
    YieldCurve(const std::vector<double>& tenors, const std::vector<double>& rates)
        : tenors_(tenors), rates_(rates), flat_rate_(rates.empty() ? 0.05 : rates[0]) {
        if (tenors.size() != rates.size()) {
            throw std::invalid_argument("Tenors and rates must have same size");
        }
    }

    // Get zero rate for a given maturity (linear interpolation)
    double get_rate(double T) const {
        if (tenors_.empty()) {
            return flat_rate_;
        }
        
        // Extrapolate flat at ends
        if (T <= tenors_.front()) return rates_.front();
        if (T >= tenors_.back()) return rates_.back();

        // Linear interpolation
        for (size_t i = 0; i < tenors_.size() - 1; ++i) {
            if (T >= tenors_[i] && T <= tenors_[i + 1]) {
                double t = (T - tenors_[i]) / (tenors_[i + 1] - tenors_[i]);
                return rates_[i] + t * (rates_[i + 1] - rates_[i]);
            }
        }
        return flat_rate_;
    }

    // Get discount factor
    double get_discount_factor(double T) const {
        return std::exp(-get_rate(T) * T);
    }

    // Get forward rate between T1 and T2
    double get_forward_rate(double T1, double T2) const {
        if (T2 <= T1) return get_rate(T1);
        double df1 = get_discount_factor(T1);
        double df2 = get_discount_factor(T2);
        return std::log(df1 / df2) / (T2 - T1);
    }

    // Bump the entire curve (parallel shift)
    void bump(double delta) {
        flat_rate_ += delta;
        for (auto& rate : rates_) {
            rate += delta;
        }
    }

    // Get short rate (overnight)
    double get_short_rate() const { return get_rate(1.0 / 365.0); }

private:
    std::vector<double> tenors_;  // Maturities in years
    std::vector<double> rates_;   // Zero rates
    double flat_rate_;            // Fallback flat rate
};

// ============================================================================
// VOLATILITY SURFACE - Vol varies by strike and expiry
// ============================================================================

class VolatilitySurface {
public:
    // Flat surface constructor
    explicit VolatilitySurface(double flat_vol = 0.20) : flat_vol_(flat_vol) {}

    // Surface from grid points
    // strikes: vector of strike prices (or moneyness K/S)
    // expiries: vector of expiry times
    // vols: 2D grid [expiry_idx][strike_idx]
    VolatilitySurface(const std::vector<double>& strikes,
                      const std::vector<double>& expiries,
                      const std::vector<std::vector<double>>& vols)
        : strikes_(strikes), expiries_(expiries), vols_(vols), flat_vol_(0.20) {
        if (!vols.empty() && !vols[0].empty()) {
            flat_vol_ = vols[0][vols[0].size() / 2];  // ATM vol as default
        }
    }

    // Get implied volatility for given strike and expiry
    double get_vol(double strike, double expiry) const {
        if (strikes_.empty() || expiries_.empty()) {
            return flat_vol_;
        }

        // Find surrounding expiries
        size_t exp_lo = 0, exp_hi = 0;
        double exp_t = find_interp_indices(expiries_, expiry, exp_lo, exp_hi);

        // Find surrounding strikes
        size_t str_lo = 0, str_hi = 0;
        double str_t = find_interp_indices(strikes_, strike, str_lo, str_hi);

        // Bilinear interpolation
        double v00 = vols_[exp_lo][str_lo];
        double v01 = vols_[exp_lo][str_hi];
        double v10 = vols_[exp_hi][str_lo];
        double v11 = vols_[exp_hi][str_hi];

        double v0 = v00 + str_t * (v01 - v00);
        double v1 = v10 + str_t * (v11 - v10);

        return v0 + exp_t * (v1 - v0);
    }

    // Get ATM vol for a given expiry
    double get_atm_vol(double expiry) const {
        if (strikes_.empty()) return flat_vol_;
        // ATM = middle strike
        double atm_strike = strikes_[strikes_.size() / 2];
        return get_vol(atm_strike, expiry);
    }

    // Bump entire surface (parallel shift)
    void bump(double delta) {
        flat_vol_ += delta;
        for (auto& row : vols_) {
            for (auto& v : row) {
                v += delta;
            }
        }
    }

    double get_flat_vol() const { return flat_vol_; }

private:
    std::vector<double> strikes_;
    std::vector<double> expiries_;
    std::vector<std::vector<double>> vols_;
    double flat_vol_;

    // Helper: find interpolation indices and parameter
    static double find_interp_indices(const std::vector<double>& vec, double val,
                                       size_t& lo, size_t& hi) {
        if (vec.size() == 1) {
            lo = hi = 0;
            return 0.0;
        }
        if (val <= vec.front()) {
            lo = hi = 0;
            return 0.0;
        }
        if (val >= vec.back()) {
            lo = hi = vec.size() - 1;
            return 0.0;
        }
        for (size_t i = 0; i < vec.size() - 1; ++i) {
            if (val >= vec[i] && val <= vec[i + 1]) {
                lo = i;
                hi = i + 1;
                return (val - vec[i]) / (vec[i + 1] - vec[i]);
            }
        }
        lo = hi = 0;
        return 0.0;
    }
};

// ============================================================================
// DIVIDEND CURVE - For equity pricing
// ============================================================================

class DividendCurve {
public:
    explicit DividendCurve(double continuous_yield = 0.0) : yield_(continuous_yield) {}

    // Discrete dividends: {time, amount}
    DividendCurve(const std::vector<std::pair<double, double>>& dividends, double yield = 0.0)
        : discrete_divs_(dividends), yield_(yield) {}

    double get_continuous_yield() const { return yield_; }

    // Get present value of dividends between now and T
    double get_pv_dividends(double T, const YieldCurve& yield_curve) const {
        double pv = 0.0;
        for (const auto& [t, amount] : discrete_divs_) {
            if (t > 0 && t <= T) {
                pv += amount * yield_curve.get_discount_factor(t);
            }
        }
        return pv;
    }

private:
    std::vector<std::pair<double, double>> discrete_divs_;
    double yield_;
};

// ============================================================================
// MARKET ENVIRONMENT - Container for all market data
// ============================================================================

class MarketEnvironment {
public:
    MarketEnvironment() = default;

    // ========================================================================
    // SPOT PRICES
    // ========================================================================
    
    void set_spot(const std::string& ticker, double price) {
        spots_[ticker] = price;
    }

    double get_spot(const std::string& ticker) const {
        auto it = spots_.find(ticker);
        if (it != spots_.end()) return it->second;
        throw std::runtime_error("Spot price not found for: " + ticker);
    }

    bool has_spot(const std::string& ticker) const {
        return spots_.find(ticker) != spots_.end();
    }

    // ========================================================================
    // YIELD CURVES
    // ========================================================================

    void set_yield_curve(const std::string& currency, const YieldCurve& curve) {
        yield_curves_[currency] = curve;
    }

    const YieldCurve& get_yield_curve(const std::string& currency = "USD") const {
        auto it = yield_curves_.find(currency);
        if (it != yield_curves_.end()) return it->second;
        return default_yield_curve_;
    }

    double get_rate(double T, const std::string& currency = "USD") const {
        return get_yield_curve(currency).get_rate(T);
    }

    double get_discount_factor(double T, const std::string& currency = "USD") const {
        return get_yield_curve(currency).get_discount_factor(T);
    }

    // ========================================================================
    // VOLATILITY SURFACES
    // ========================================================================

    void set_vol_surface(const std::string& ticker, const VolatilitySurface& surface) {
        vol_surfaces_[ticker] = surface;
    }

    const VolatilitySurface& get_vol_surface(const std::string& ticker) const {
        auto it = vol_surfaces_.find(ticker);
        if (it != vol_surfaces_.end()) return it->second;
        return default_vol_surface_;
    }

    double get_vol(const std::string& ticker, double strike, double expiry) const {
        return get_vol_surface(ticker).get_vol(strike, expiry);
    }

    double get_atm_vol(const std::string& ticker, double expiry) const {
        return get_vol_surface(ticker).get_atm_vol(expiry);
    }

    // ========================================================================
    // DIVIDEND CURVES
    // ========================================================================

    void set_dividend_curve(const std::string& ticker, const DividendCurve& curve) {
        dividend_curves_[ticker] = curve;
    }

    const DividendCurve& get_dividend_curve(const std::string& ticker) const {
        auto it = dividend_curves_.find(ticker);
        if (it != dividend_curves_.end()) return it->second;
        return default_dividend_curve_;
    }

    // ========================================================================
    // SCENARIO / BUMPING
    // ========================================================================

    // Parallel rate shift across all curves
    void bump_rates(double delta) {
        for (auto& [currency, curve] : yield_curves_) {
            curve.bump(delta);
        }
        default_yield_curve_.bump(delta);
    }

    // Parallel vol shift across all surfaces
    void bump_vols(double delta) {
        for (auto& [ticker, surface] : vol_surfaces_) {
            surface.bump(delta);
        }
        default_vol_surface_.bump(delta);
    }

    // Shock all spots by percentage
    void shock_spots(double pct_change) {
        for (auto& [ticker, price] : spots_) {
            price *= (1.0 + pct_change);
        }
    }

    // ========================================================================
    // VALUATION DATE
    // ========================================================================

    void set_valuation_date(double t) { valuation_date_ = t; }
    double get_valuation_date() const { return valuation_date_; }

    // Advance time by dt (for simulation)
    void advance_time(double dt) { valuation_date_ += dt; }

private:
    // Spot prices by ticker
    std::map<std::string, double> spots_;

    // Yield curves by currency
    std::map<std::string, YieldCurve> yield_curves_;
    YieldCurve default_yield_curve_{0.05};

    // Volatility surfaces by ticker/index
    std::map<std::string, VolatilitySurface> vol_surfaces_;
    VolatilitySurface default_vol_surface_{0.20};

    // Dividend curves by ticker
    std::map<std::string, DividendCurve> dividend_curves_;
    DividendCurve default_dividend_curve_{0.0};

    // Current valuation date (in years from start)
    double valuation_date_ = 0.0;
};

// ============================================================================
// HELPER: Create typical market environments for testing
// ============================================================================

inline MarketEnvironment create_sample_market() {
    MarketEnvironment env;

    // USD Yield Curve (upward sloping)
    YieldCurve usd_curve(
        {0.25, 0.5, 1.0, 2.0, 5.0, 10.0, 30.0},    // Tenors
        {0.04, 0.042, 0.045, 0.048, 0.05, 0.052, 0.055}  // Rates
    );
    env.set_yield_curve("USD", usd_curve);

    // EUR Yield Curve
    YieldCurve eur_curve(
        {0.25, 0.5, 1.0, 2.0, 5.0, 10.0},
        {0.02, 0.022, 0.025, 0.028, 0.03, 0.032}
    );
    env.set_yield_curve("EUR", eur_curve);

    // Sample equity vol surface (simplified - just ATM term structure)
    // In practice, this would be a full smile/skew surface
    VolatilitySurface aapl_vol(
        {100.0, 120.0, 140.0, 150.0, 160.0, 180.0, 200.0},  // Strikes
        {0.083, 0.25, 0.5, 1.0},  // Expiries (1M, 3M, 6M, 1Y)
        {
            {0.28, 0.25, 0.22, 0.20, 0.22, 0.25, 0.28},  // 1M smile
            {0.26, 0.23, 0.21, 0.20, 0.21, 0.24, 0.27},  // 3M smile
            {0.25, 0.22, 0.20, 0.19, 0.20, 0.23, 0.26},  // 6M smile
            {0.24, 0.21, 0.19, 0.18, 0.19, 0.22, 0.25}   // 1Y smile
        }
    );
    env.set_vol_surface("AAPL", aapl_vol);

    // Higher vol for TSLA
    VolatilitySurface tsla_vol(
        {150.0, 200.0, 250.0, 300.0, 350.0},
        {0.083, 0.25, 0.5, 1.0},
        {
            {0.55, 0.48, 0.45, 0.48, 0.55},
            {0.52, 0.45, 0.42, 0.45, 0.52},
            {0.50, 0.43, 0.40, 0.43, 0.50},
            {0.48, 0.41, 0.38, 0.41, 0.48}
        }
    );
    env.set_vol_surface("TSLA", tsla_vol);

    // Spot prices
    env.set_spot("AAPL", 150.0);
    env.set_spot("GOOGL", 140.0);
    env.set_spot("TSLA", 250.0);

    return env;
}

#endif
