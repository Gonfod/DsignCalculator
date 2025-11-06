#include "grapher.h"
#include "../parser/core_parser.h"
#include "../evaluator/evaluator.h"
#include "../tokenizer/tokenizer.h"
#include <iostream>
#include <cmath>
#include <limits>

static bool rpnUsesY(const std::vector<Token>& rpn) {
    for (auto &t : rpn) if (t.type == TokenType::Variable && t.text == "y") return true;
    return false;
}

std::vector<sf::Vector2f> computeWorldSamplesFromRPN(const std::vector<Token>& rpn, double xMin, double xMax, double step, const std::unordered_map<std::string,double>* env) {
    std::vector<sf::Vector2f> samples;
    if (rpn.empty()) return samples;
    size_t estimated = (size_t)((xMax - xMin) / step) + 1;
    samples.reserve(std::min<size_t>(estimated, 100000));
    for (double x = xMin; x <= xMax; x += step) {
        try {
            double y;
            if (env) {
                std::unordered_map<std::string,double> local = *env;
                local["x"] = x;
                y = evaluateRPNEnv(rpn, local);
            } else {
                y = evaluateRPNVec(rpn, x);
            }
            if (!std::isfinite(y)) continue;
            samples.emplace_back(static_cast<float>(x), static_cast<float>(y));
        }
        catch (...) { }
    }
    return samples;
}

// simple linear interpolation helper
static sf::Vector2f lerpPoint(double x1, double y1, double x2, double y2, double t) {
    return sf::Vector2f(static_cast<float>(x1 + (x2 - x1) * t), static_cast<float>(y1 + (y2 - y1) * t));
}

// marching squares for contour at value 0
static std::vector<std::vector<sf::Vector2f>> marchingSquares(const std::vector<std::vector<double>>& grid, double x0, double y0, double dx, double dy, double iso=0.0) {
    int nx = (int)grid[0].size();
    int ny = (int)grid.size();
    std::vector<std::vector<sf::Vector2f>> segments;
    for (int j = 0; j < ny-1; ++j) {
        for (int i = 0; i < nx-1; ++i) {
            double v[4];
            v[0] = grid[j][i];     // top-left
            v[1] = grid[j][i+1];   // top-right
            v[2] = grid[j+1][i+1]; // bottom-right
            v[3] = grid[j+1][i];   // bottom-left
            int mask = 0;
            if (v[0] >= iso) mask |= 1;
            if (v[1] >= iso) mask |= 2;
            if (v[2] >= iso) mask |= 4;
            if (v[3] >= iso) mask |= 8;
            // skip trivial
            if (mask == 0 || mask == 15) continue;
            // compute interpolated edge points
            auto interp = [&](int a, int b, double xa, double ya, double xb, double yb) {
                double va = a==0? v[0] : (a==1? v[1] : (a==2? v[2] : v[3]));
                double vb = b==0? v[0] : (b==1? v[1] : (b==2? v[2] : v[3]));
                double t = 0.0;
                if (vb != va) t = (iso - va) / (vb - va);
                return lerpPoint(xa, ya, xb, yb, t);
            };
            // coordinates of cell corners
            double xL = x0 + i * dx;
            double xR = x0 + (i+1) * dx;
            double yT = y0 + j * dy;
            double yB = y0 + (j+1) * dy;
            // edges midpoints depending on case (partial set of cases implemented using linear interpolation)
            std::vector<sf::Vector2f> pts;
            switch (mask) {
                case 1: case 14: {
                    // TL inside
                    auto p1 = interp(0,1, xL,yT, xR,yT);
                    auto p2 = interp(0,3, xL,yT, xL,yB);
                    pts.push_back(p1); pts.push_back(p2);
                    break;
                }
                case 2: case 13: {
                    auto p1 = interp(0,1, xL,yT, xR,yT);
                    auto p2 = interp(1,2, xR,yT, xR,yB);
                    pts.push_back(p1); pts.push_back(p2);
                    break;
                }
                case 3: case 12: {
                    auto p1 = interp(1,2, xR,yT, xR,yB);
                    auto p2 = interp(0,3, xL,yT, xL,yB);
                    pts.push_back(p1); pts.push_back(p2);
                    break;
                }
                case 4: case 11: {
                    auto p1 = interp(2,3, xR,yB, xL,yB);
                    auto p2 = interp(1,2, xR,yT, xR,yB);
                    pts.push_back(p1); pts.push_back(p2);
                    break;
                }
                case 5: case 10: {
                    auto p1 = interp(0,1, xL,yT, xR,yT);
                    auto p2 = interp(2,3, xR,yB, xL,yB);
                    pts.push_back(p1); pts.push_back(p2);
                    break;
                }
                case 6: case 9: {
                    auto p1 = interp(0,3, xL,yT, xL,yB);
                    auto p2 = interp(2,3, xR,yB, xL,yB);
                    pts.push_back(p1); pts.push_back(p2);
                    break;
                }
                case 7: case 8: {
                    auto p1 = interp(0,3, xL,yT, xL,yB);
                    auto p2 = interp(1,2, xR,yT, xR,yB);
                    pts.push_back(p1); pts.push_back(p2);
                    break;
                }
                default: break;
            }
            if (!pts.empty()) segments.push_back(pts);
        }
    }
    return segments;
}

std::vector<sf::Vertex> computeGraphFromRPN(const std::vector<Token>& rpn, sf::Color color, double scale, double xMin, double xMax, double step, double centerX, double centerY, int screenWidth, int screenHeight, const std::unordered_map<std::string,double>* env, std::atomic<bool>* cancel) {
    std::vector<sf::Vertex> graph;
    if (rpn.empty()) return graph;

    // if expression uses y, perform marching squares over 2D grid to find contour F(x,y)=0
    if (rpnUsesY(rpn) && screenWidth > 0 && screenHeight > 0) {
        // create sampling grid in world coords covering visible screen
        // compute world bounds
        double worldXMin = (0.0 - centerX) / scale;
        double worldXMax = ((double)screenWidth - centerX) / scale;
        double worldYMax = centerY / scale;
        double worldYMin = (centerY - screenHeight) / scale;
        int nx = std::min(300, screenWidth); // limit grid resolution
        int ny = std::min(300, screenHeight);
        double dx = (worldXMax - worldXMin) / (nx - 1);
        double dy = (worldYMax - worldYMin) / (ny - 1);
        std::vector<std::vector<double>> grid(ny, std::vector<double>(nx));
        for (int j = 0; j < ny; ++j) {
            for (int i = 0; i < nx; ++i) {
                double wx = worldXMin + i * dx;
                double wy = worldYMin + j * dy;
                try {
                    if (env) {
                        std::unordered_map<std::string,double> local = *env;
                        local["x"] = wx; local["y"] = wy;
                        grid[j][i] = evaluateRPNEnv(rpn, local);
                    } else {
                        grid[j][i] = evaluateRPNXY(rpn, wx, wy);
                    }
                }
                catch (...) { grid[j][i] = std::numeric_limits<double>::quiet_NaN(); }
            }
        }
        auto segments = marchingSquares(grid, worldXMin, worldYMin, dx, dy, 0.0);
        // convert segments to screen vertices
        for (auto &seg : segments) {
            for (auto &p : seg) {
                float sx = static_cast<float>(centerX + p.x * scale);
                float sy = static_cast<float>(centerY - p.y * scale);
                graph.emplace_back(sf::Vector2f(sx, sy), color);
            }
        }
        return graph;
    }

    // fallback: 1D plotting as before
    auto samples = computeWorldSamplesFromRPN(rpn, xMin, xMax, step, env);
    if (screenWidth <= 0) {
        graph.reserve(samples.size());
        for (auto &p : samples) {
            float sx = static_cast<float>(centerX + p.x * scale);
            float sy = static_cast<float>(centerY - p.y * scale);
            graph.emplace_back(sf::Vector2f(sx, sy), color);
        }
        return graph;
    }

    std::vector<double> minY(screenWidth, std::numeric_limits<double>::infinity());
    std::vector<double> maxY(screenWidth, -std::numeric_limits<double>::infinity());
    std::vector<char> has(screenWidth, 0);
    for (auto &p : samples) {
        int px = (int)std::floor(centerX + p.x * scale);
        if (px < 0 || px >= screenWidth) continue;
        double y = p.y;
        if (!has[px]) { minY[px] = maxY[px] = y; has[px]=1; }
        else { if (y < minY[px]) minY[px] = y; if (y > maxY[px]) maxY[px] = y; }
    }
    for (int px = 0; px < screenWidth; ++px) {
        if (!has[px]) continue;
        double y1 = minY[px]; double y2 = maxY[px];
        float sy1 = static_cast<float>(centerY - y1 * scale);
        float sy2 = static_cast<float>(centerY - y2 * scale);
        graph.emplace_back(sf::Vector2f((float)px, sy1), color);
        if (y2 != y1) graph.emplace_back(sf::Vector2f((float)px, sy2), color);
    }
    return graph;
}

std::vector<sf::Vertex> computeGraph(const std::string& expr, sf::Color color, double scale, double xMin, double xMax, double step, double centerX, double centerY) {
    auto tokens = tokenize(expr);
    auto rpn = shuntingYard(tokens);
    // default screenWidth/screenHeight 0 for legacy
    return computeGraphFromRPN(rpn, color, scale, xMin, xMax, step, centerX, centerY, 0, 0, nullptr, nullptr);
}

void drawGraph(sf::RenderWindow& window, const std::string& expr, const sf::Font& font) {
    window.clear(sf::Color::Black);
    float centerX = window.getSize().x / 2.0f;
    float centerY = window.getSize().y / 2.0f;
    sf::Vertex xAxis[] = { {{0.f, centerY},sf::Color::White}, {{static_cast<float>(window.getSize().x), centerY},sf::Color::White} };
    sf::Vertex yAxis[] = { {{centerX,0.f},sf::Color::White}, {{centerX, static_cast<float>(window.getSize().y)},sf::Color::White} };
    window.draw(xAxis, 2, sf::Lines);
    window.draw(yAxis, 2, sf::Lines);
    auto graph = computeGraph(expr, sf::Color::Cyan, 50.0, -8.0, 8.0, 0.01, centerX, centerY);
    if (!graph.empty())
        window.draw(&graph[0], graph.size(), sf::LineStrip);
    window.display();
}
