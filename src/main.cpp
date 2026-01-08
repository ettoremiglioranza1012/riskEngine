// Main.cpp of riskEngine

#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <memory>
#include "../include/marketSimulator.hh"
#include "../include/instrument.hh"
#include "../include/marketEnvironment.hh"

void print_greeks(const std::string& name, const Greeks& g) {
    std::cout << name << " Greeks:" << std::endl;
    std::cout << "  Delta: " << std::fixed << std::setprecision(2) << g.delta << std::endl;
    std::cout << "  Gamma: " << std::setprecision(4) << g.gamma << std::endl;
    std::cout << "  Vega:  " << std::setprecision(2) << g.vega << std::endl;
    std::cout << "  Theta: " << std::setprecision(4) << g.theta << std::endl;
    std::cout << "  Rho:   " << std::setprecision(2) << g.rho << std::endl;
}

void print_market_env(const MarketEnvironment& env) {
    const auto& yc = env.get_yield_curve("USD");
    std::cout << "  Yield Curve (USD):" << std::endl;
    std::cout << "    Short Rate (0.1y): " << std::setprecision(4) << (yc.get_rate(0.1) * 100) << "%" << std::endl;
    std::cout << "    1Y Rate:           " << (yc.get_rate(1.0) * 100) << "%" << std::endl;
    std::cout << "    5Y Rate:           " << (yc.get_rate(5.0) * 100) << "%" << std::endl;
    std::cout << "    10Y Rate:          " << (yc.get_rate(10.0) * 100) << "%" << std::endl;
}

int main() {
    try {
        // ====================================================================
        // MARKET ENVIRONMENT SETUP - Term Structures (not flat values!)
        // ====================================================================
        
        // Create yield curve with realistic USD rates
        // Normal upward-sloping term structure
        std::vector<double> tenors = {0.25, 0.5, 1.0, 2.0, 5.0, 10.0, 30.0};
        std::vector<double> rates = {0.045, 0.048, 0.050, 0.052, 0.055, 0.058, 0.060};
        YieldCurve usd_curve(tenors, rates);
        
        // Create volatility surfaces for each underlying
        // Strikes × Expiries grid with implied vols
        std::vector<double> strikes_aapl = {120, 140, 160, 180, 200};
        std::vector<double> expiries = {0.25, 0.5, 1.0, 2.0};
        
        // Vol surface with skew (higher vols at lower strikes - "smirk")
        // Note: vols grid is [strike_idx][expiry_idx]
        std::vector<std::vector<double>> aapl_vols = {
            {0.35, 0.33, 0.32, 0.31},  // 120 strike - highest vol (put skew)
            {0.28, 0.27, 0.26, 0.26},  // 140 strike
            {0.25, 0.24, 0.24, 0.24},  // 160 strike (ATM)
            {0.27, 0.26, 0.25, 0.25},  // 180 strike
            {0.30, 0.29, 0.28, 0.27}   // 200 strike (call skew)
        };
        VolatilitySurface aapl_surface(strikes_aapl, expiries, aapl_vols);
        
        std::vector<double> strikes_tsla = {200, 250, 300, 350, 400};
        std::vector<std::vector<double>> tsla_vols = {
            {0.65, 0.60, 0.55, 0.52},  // 200 strike
            {0.55, 0.52, 0.50, 0.48},  // 250 strike (ATM)
            {0.50, 0.48, 0.46, 0.45},  // 300 strike
            {0.52, 0.50, 0.48, 0.47},  // 350 strike
            {0.58, 0.55, 0.52, 0.50}   // 400 strike
        };
        VolatilitySurface tsla_surface(strikes_tsla, expiries, tsla_vols);
        
        std::vector<double> strikes_googl = {100, 120, 140, 160, 180};
        std::vector<std::vector<double>> googl_vols = {
            {0.32, 0.30, 0.28, 0.27},
            {0.28, 0.26, 0.25, 0.24},
            {0.25, 0.24, 0.23, 0.23},  // ATM
            {0.27, 0.26, 0.25, 0.24},
            {0.30, 0.28, 0.27, 0.26}
        };
        VolatilitySurface googl_surface(strikes_googl, expiries, googl_vols);
        
        // Assemble market environment
        MarketEnvironment market_env;
        market_env.set_yield_curve("USD", usd_curve);
        market_env.set_vol_surface("AAPL", aapl_surface);
        market_env.set_vol_surface("TSLA", tsla_surface);
        market_env.set_vol_surface("GOOGL", googl_surface);
        
        // Add spot prices
        market_env.set_spot("AAPL", 150.0);
        market_env.set_spot("TSLA", 250.0);
        market_env.set_spot("GOOGL", 140.0);

        // Create market simulator with Black-Scholes model
        // Uses flat vol/rate for legacy compatibility, but also supports MarketEnvironment
        auto bs_model = std::make_unique<BlackScholesModel>(0.05, 0.20, 42);
        MarketSimulator market(std::move(bs_model));
        market.reserve_portfolios(3);

        // Create shared instruments (pure DATA - no logic)
        auto apple = std::make_shared<Stock>("AAPL", 150.0);
        auto google = std::make_shared<Stock>("GOOGL", 140.0);
        auto tesla = std::make_shared<Stock>("TSLA", 250.0);
        
        // Options need reference to underlying stock + time to expiry
        auto tesla_call = std::make_shared<Option>(
            "TSLA-C-300", 15.0, 300.0, tesla, 0.5, Option::Type::Call
        );
        auto apple_put = std::make_shared<Option>(
            "AAPL-P-140", 8.0, 140.0, apple, 0.25, Option::Type::Put
        );
        
        auto treasury = std::make_shared<Bond>("T-10Y", 98.5, 8.5, 0.04);

        // Portfolio 1: Conservative (Grandfather) - mostly bonds
        size_t id_retirement = market.create_portfolio("Grandfather", "USD");
        market.get_portfolio(id_retirement).add_position(treasury, 100);
        market.get_portfolio(id_retirement).add_position(apple, 50);

        // Portfolio 2: Balanced (Sons) - mix of stocks with protective put
        size_t id_savings = market.create_portfolio("Sons", "USD");
        market.get_portfolio(id_savings).add_position(apple, 200);
        market.get_portfolio(id_savings).add_position(google, 150);
        market.get_portfolio(id_savings).add_position(apple_put, 50);
        market.get_portfolio(id_savings).add_position(treasury, 30);

        // Portfolio 3: Aggressive (Nephew) - stocks + call options
        size_t id_aggressive = market.create_portfolio("Nephew", "USD");
        market.get_portfolio(id_aggressive).add_position(tesla, 100);
        market.get_portfolio(id_aggressive).add_position(tesla_call, 50);

        // Display header
        std::cout << "╔══════════════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║   RISK ENGINE - WITH MARKET ENVIRONMENT                  ║" << std::endl;
        std::cout << "║   Term Structures: Yield Curve + Vol Surfaces            ║" << std::endl;
        std::cout << "╚══════════════════════════════════════════════════════════╝" << std::endl;
        std::cout << std::endl;

        // Display market environment
        std::cout << "=== Market Environment ===" << std::endl;
        print_market_env(market_env);
        std::cout << std::endl;
        
        std::cout << "  Vol Surfaces (ATM at 0.5y):" << std::endl;
        std::cout << std::setprecision(2);
        std::cout << "    AAPL: " << (market_env.get_vol("AAPL", 150, 0.5) * 100) << "%" << std::endl;
        std::cout << "    TSLA: " << (market_env.get_vol("TSLA", 250, 0.5) * 100) << "%" << std::endl;
        std::cout << "    GOOGL: " << (market_env.get_vol("GOOGL", 140, 0.5) * 100) << "%" << std::endl;
        std::cout << std::endl;

        // Show vol skew
        std::cout << "  AAPL Vol Smile (0.5y expiry):" << std::endl;
        std::cout << "    K=120: " << (market_env.get_vol("AAPL", 120, 0.5) * 100) << "%" << std::endl;
        std::cout << "    K=140: " << (market_env.get_vol("AAPL", 140, 0.5) * 100) << "%" << std::endl;
        std::cout << "    K=160 (ATM): " << (market_env.get_vol("AAPL", 160, 0.5) * 100) << "%" << std::endl;
        std::cout << "    K=180: " << (market_env.get_vol("AAPL", 180, 0.5) * 100) << "%" << std::endl;
        std::cout << std::endl;

        std::cout << "=== Initial Portfolio Values ===" << std::endl;
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Retirement (Conservative): $" << market.get_portfolio_value(id_retirement) << std::endl;
        std::cout << "Savings (Balanced):        $" << market.get_portfolio_value(id_savings) << std::endl;
        std::cout << "Aggressive (Leveraged):    $" << market.get_portfolio_value(id_aggressive) << std::endl;
        std::cout << std::endl;

        // Calculate and display initial Greeks using GreeksVisitor
        std::cout << "=== Initial Portfolio Greeks ===" << std::endl;
        print_greeks("Retirement", market.get_portfolio_greeks(id_retirement));
        std::cout << std::endl;
        print_greeks("Savings", market.get_portfolio_greeks(id_savings));
        std::cout << std::endl;
        print_greeks("Aggressive", market.get_portfolio_greeks(id_aggressive));
        std::cout << std::endl;

        // ====================================================================
        // DEMONSTRATE VISITOR PATTERN - Same data, different simulation methods
        // ====================================================================

        std::cout << "=== Monte Carlo Simulation (252 days) ===" << std::endl;
        market.simulate_days(252);
        std::cout << "Retirement: $" << market.get_portfolio_value(id_retirement) << std::endl;
        std::cout << "Savings:    $" << market.get_portfolio_value(id_savings) << std::endl;
        std::cout << "Aggressive: $" << market.get_portfolio_value(id_aggressive) << std::endl;
        std::cout << std::endl;

        // Apply stress test - 2008 financial crisis scenario
        std::cout << "=== Stress Test: 2008 Crisis Scenario ===" << std::endl;
        std::cout << "    (Price: -30%, Vol: +50%, Rates: -2%)" << std::endl;
        double initial_retirement = market.get_portfolio_value(id_retirement);
        double initial_savings = market.get_portfolio_value(id_savings);
        double initial_aggressive = market.get_portfolio_value(id_aggressive);
        
        market.apply_stress_test(-0.30, 0.50, -0.02);
        
        std::cout << "Retirement: $" << market.get_portfolio_value(id_retirement) 
                  << " (Δ: " << (market.get_portfolio_value(id_retirement) - initial_retirement) << ")" << std::endl;
        std::cout << "Savings:    $" << market.get_portfolio_value(id_savings)
                  << " (Δ: " << (market.get_portfolio_value(id_savings) - initial_savings) << ")" << std::endl;
        std::cout << "Aggressive: $" << market.get_portfolio_value(id_aggressive)
                  << " (Δ: " << (market.get_portfolio_value(id_aggressive) - initial_aggressive) << ")" << std::endl;
        std::cout << std::endl;

        // Show instrument prices
        std::cout << "=== Final Instrument Prices ===" << std::endl;
        std::cout << "AAPL:  $" << apple->get_price() << std::endl;
        std::cout << "GOOGL: $" << google->get_price() << std::endl;
        std::cout << "TSLA:  $" << tesla->get_price() << std::endl;
        std::cout << "TSLA Call: $" << tesla_call->get_price() 
                  << " (TTE: " << tesla_call->get_time_to_expiry() << "y)" << std::endl;
        std::cout << "AAPL Put:  $" << apple_put->get_price()
                  << " (TTE: " << apple_put->get_time_to_expiry() << "y)" << std::endl;
        std::cout << std::endl;

        // Final Greeks
        std::cout << "=== Final Aggregate Greeks ===" << std::endl;
        print_greeks("Total Portfolio", market.get_total_greeks());

    } catch (const std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}