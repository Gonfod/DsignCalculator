#include "grapher.h"
#include "../parser/core_parser.h"
#include "../evaluator/evaluator.h"
#include "../tokenizer/tokenizer.h"
#include <iostream>
#include <cmath>
#include <limits>
#include <atomic>
#include <algorithm>

static bool rpnUsesY(const std::vector<Token>& rpn) {
    for (auto& t : rpn) if (t.type == TokenType::Variable && t.text == "y") return true;
    return false;
}
std::vector<sf::Vector2f> computeWorldSamplesFromRPN(const std::vector<Token>& rpn,
    double xMin, double xMax, double step,
    const std::unordered_map<std::string, double>* env)
{
    std::vector<sf::Vector2f> samples;
    if (rpn.empty()) return samples;
    size_t estimated = 0;
    if (step > 0) estimated = (size_t)((xMax - xMin) / step) + 1;
    samples.reserve(std::min<size_t>(std::max<size_t>(estimated, 16), 200000));

    for (double x = xMin; x <= xMax; x += step) {
        try {
            double y;
            if (env) {
                std::unordered_map<std::string, double> local = *env;
                local["x"] = x;
                y = evaluateRPNEnv(rpn, local);
            }
            else {
                y = evaluateRPNVec(rpn, x);
            }
            if (!std::isfinite(y)) continue;
            samples.emplace_back(static_cast<float>(x), static_cast<float>(y));
        }
        catch (...) { /* ignore individual eval errors */ }
    }
    return samples;
}
static sf::Vector2f lerpPoint(double x1, double y1, double x2, double y2, double t) {
    return sf::Vector2f(static_cast<float>(x1 + (x2 - x1) * t), static_cast<float>(y1 + (y2 - y1) * t));
}
static std::vector<std::vector<sf::Vector2f>> marchingSquares(const std::vector<std::vector<double>>& grid,
    double x0, double y0, double dx, double dy, double iso = 0.0)
{
    int ny = (int)grid.size();
    if (ny == 0) return {};
    int nx = (int)grid[0].size();
    if (nx == 0) return {};

    std::vector<std::vector<sf::Vector2f>> segments;
    segments.reserve((nx - 1) * (ny - 1) / 4);

    for (int j = 0; j < ny - 1; ++j) {
        for (int i = 0; i < nx - 1; ++i) {
            double v[4];
            v[0] = grid[j][i]; 
            v[1] = grid[j][i + 1];  
            v[2] = grid[j + 1][i + 1]; 
            v[3] = grid[j + 1][i];  
            int mask = 0;
            if (v[0] >= iso) mask |= 1;
            if (v[1] >= iso) mask |= 2;
            if (v[2] >= iso) mask |= 4;
            if (v[3] >= iso) mask |= 8;
            if (mask == 0 || mask == 15) continue;

            auto interp = [&](int a, int b, double xa, double ya, double xb, double yb) {
                double va = (a == 0 ? v[0] : (a == 1 ? v[1] : (a == 2 ? v[2] : v[3])));
                double vb = (b == 0 ? v[0] : (b == 1 ? v[1] : (b == 2 ? v[2] : v[3])));
                double t = 0.0;
                if (std::isfinite(va) && std::isfinite(vb) && vb != va) t = (iso - va) / (vb - va);
                return lerpPoint(xa, ya, xb, yb, t);
                };

            double xL = x0 + i * dx;
            double xR = x0 + (i + 1) * dx;
            double yT = y0 + j * dy;
            double yB = y0 + (j + 1) * dy;

            std::vector<sf::Vector2f> pts;
            switch (mask) {
            case 1: case 14: {
                pts.push_back(interp(0, 1, xL, yT, xR, yT));
                pts.push_back(interp(0, 3, xL, yT, xL, yB));
                break;
            }
            case 2: case 13: {
                pts.push_back(interp(0, 1, xL, yT, xR, yT));
                pts.push_back(interp(1, 2, xR, yT, xR, yB));
                break;
            }
            case 3: case 12: {
                pts.push_back(interp(1, 2, xR, yT, xR, yB));
                pts.push_back(interp(0, 3, xL, yT, xL, yB));
                break;
            }
            case 4: case 11: {
                pts.push_back(interp(2, 3, xR, yB, xL, yB));
                pts.push_back(interp(1, 2, xR, yT, xR, yB));
                break;
            }
            case 5: case 10: {
                pts.push_back(interp(0, 1, xL, yT, xR, yT));
                pts.push_back(interp(2, 3, xR, yB, xL, yB));
                break;
            }
            case 6: case 9: {
                pts.push_back(interp(0, 3, xL, yT, xL, yB));
                pts.push_back(interp(2, 3, xR, yB, xL, yB));
                break;
            }
            case 7: case 8: {
                pts.push_back(interp(0, 3, xL, yT, xL, yB));
                pts.push_back(interp(1, 2, xR, yT, xR, yB));
                break;
            }
            default: break;
            }

            if (!pts.empty()) segments.push_back(std::move(pts));
        }
    }
    return segments;
}
std::vector<std::vector<sf::Vertex>> computeGraphFromRPN(
    const std::vector<Token>& rpn,
    sf::Color color,
    double scale,
    double xMin, double xMax, double step,
    double centerX, double centerY,
    int screenWidth, int screenHeight,
    const std::unordered_map<std::string, double>* env,
    std::atomic<bool>* cancel)
{
    std::vector<std::vector<sf::Vertex>> segmentsOut;
    if (rpn.empty()) return segmentsOut;
    if (rpnUsesY(rpn) && screenWidth > 0 && screenHeight > 0) {
        double worldXMin = (0.0 - centerX) / scale;
        double worldXMax = ((double)screenWidth - centerX) / scale;
        double worldYMax = centerY / scale;
        double worldYMin = (centerY - screenHeight) / scale;

        int nx = std::min(300, std::max(8, screenWidth / 2));
        int ny = std::min(300, std::max(8, screenHeight / 2));
        double dx = (worldXMax - worldXMin) / (nx - 1);
        double dy = (worldYMax - worldYMin) / (ny - 1);

        std::vector<std::vector<double>> grid(ny, std::vector<double>(nx, NAN));
        for (int j = 0; j < ny; ++j) {
            for (int i = 0; i < nx; ++i) {
                double wx = worldXMin + i * dx;
                double wy = worldYMin + j * dy;
                try {
                    if (env) {
                        auto local = *env; local["x"] = wx; local["y"] = wy;
                        grid[j][i] = evaluateRPNEnv(rpn, local);
                    }
                    else {
                        grid[j][i] = evaluateRPNXY(rpn, wx, wy);
                    }
                }
                catch (...) { grid[j][i] = NAN; }
            }
        }

        auto segs = marchingSquares(grid, worldXMin, worldYMin, dx, dy, 0.0);
        segmentsOut.reserve(segs.size());
        for (auto& s : segs) {
            std::vector<sf::Vertex> segV;
            segV.reserve(s.size());
            for (auto& p : s) {
                float sx = static_cast<float>(centerX + p.x * scale);
                float sy = static_cast<float>(centerY - p.y * scale);
                segV.emplace_back(sf::Vector2f(sx, sy), color);
            }
            if (segV.size() >= 2) segmentsOut.push_back(std::move(segV));
        }

        return segmentsOut;
    }
    auto samples = computeWorldSamplesFromRPN(rpn, xMin, xMax, step, env);
    if (samples.empty()) return segmentsOut;

    const double MAX_JUMP = std::max(10.0, 10.0 / (scale / 5.0));
    std::vector<sf::Vertex> curr;
    curr.reserve(1024);
    double prevY = 0.0;
    bool havePrev = false;

    for (auto& p : samples) {
        double x = p.x;
        double y = p.y;
        if (!std::isfinite(y)) { if (curr.size() >= 2) segmentsOut.push_back(std::move(curr)); curr.clear(); havePrev = false; continue; }

        if (havePrev && std::abs(y - prevY) > MAX_JUMP) {
            if (curr.size() >= 2) segmentsOut.push_back(std::move(curr));
            curr.clear(); havePrev = false;
        }

        float sx = static_cast<float>(centerX + x * scale);
        float sy = static_cast<float>(centerY - y * scale);
        curr.emplace_back(sf::Vector2f(sx, sy), color);
        prevY = y; havePrev = true;
    }
    if (curr.size() >= 2) segmentsOut.push_back(std::move(curr));
    return segmentsOut;
}
std::vector<std::vector<sf::Vertex>> computeGraph(const std::string& expr,
    sf::Color color, double scale,
    double xMin, double xMax, double step,
    double centerX, double centerY,
    int screenWidth, int screenHeight,
    const std::unordered_map<std::string, double>* env) {
    auto tokens = tokenize(expr);
    auto rpn = shuntingYard(tokens);
    std::atomic<bool> cancelFlag(false);
    return computeGraphFromRPN(rpn, color, scale, xMin, xMax, step, centerX, centerY, screenWidth, screenHeight, env, &cancelFlag);
}
void drawSegments(sf::RenderWindow& window, const std::vector<std::vector<sf::Vertex>>& segments) {
    for (const auto& seg : segments) {
        if (seg.size() < 2) continue;
        window.draw(seg.data(), seg.size(), sf::LineStrip);
    }
}
